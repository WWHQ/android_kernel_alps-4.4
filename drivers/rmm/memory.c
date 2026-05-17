/**
 * ============================================================================
 * 泪心开源驱动 - TearGame Open Source Driver (雷电 4.4.146 内存读写兼容修正版)
 * ============================================================================
 */

#include "memory.h"
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/kallsyms.h> // 必须引入，用于解密内核函数指针

/* =================【定义动态函数指针类型】================= */
static void (*dyn_put_pid)(struct pid *pid) = NULL;

/* =================【驱动模块初始化：运行时自动解密符号】================= */
static int __init teargame_memory_init(void)
{
    printk(KERN_INFO "[TearGame] 正在解密内存读写所需的内核符号...\n");

    // 动态提取雷电运行时的真实 put_pid 函数地址
    dyn_put_pid = (void (*)(struct pid *))kallsyms_lookup_name("put_pid");

    if (!dyn_put_pid) {
        printk(KERN_ERR "[TearGame] 错误：内存驱动无法获取 put_pid 符号地址！\n");
        return -EINVAL;
    }

    printk(KERN_INFO "[TearGame] 内存读写驱动符号绑定成功！\n");
    return 0;
}

static void __exit teargame_memory_exit(void)
{
    printk(KERN_INFO "[TearGame] 内存读写驱动已卸载。\n");
}

module_init(teargame_memory_init);
module_exit(teargame_memory_exit);

/* =================【核心业务逻辑：读取进程内存】================= */
bool read_process_memory(pid_t pid, uintptr_t addr, void *buffer, size_t size)
{
    struct task_struct *task;
    struct pid *pid_struct;
    int bytes_read;
    void *kbuf;

    if (size == 0 || size > (1024 * 1024)) // 限制最大1MB
        return false;

    // 如果解密未完成，安全拦截
    if (!dyn_put_pid)
        return false;

    pid_struct = find_get_pid(pid);
    if (!pid_struct)
        return false;

    task = get_pid_task(pid_struct, PIDTYPE_PID);
    
    // 【修改点 1】：使用动态指针释放 PID，彻底消灭 Unknown symbol put_pid
    dyn_put_pid(pid_struct); 
    
    if (!task)
        return false;

    kbuf = kmalloc(size, GFP_KERNEL);
    if (!kbuf) {
        put_task_struct(task);
        return false;
    }

    // 【修改点 2】：物理对齐 4.4 内核底层的 access_process_vm 参数结构
    // 4.4 内核最后一个参数传入 0 代表读取，不能传 FOLL_FORCE，否则会格机或读取失败
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
    bytes_read = access_process_vm(task, addr, kbuf, size, 0); 
#else
    bytes_read = access_process_vm(task, addr, kbuf, size, FOLL_FORCE);
#endif

    put_task_struct(task);

    if (bytes_read != size) {
        kfree(kbuf);
        return false;
    }

    if (copy_to_user(buffer, kbuf, size)) {
        kfree(kbuf);
        return false;
    }

    kfree(kbuf);
    return true;
}

/* =================【核心业务逻辑：写入进程内存】================= */
bool write_process_memory(pid_t pid, uintptr_t addr, void *buffer, size_t size)
{
    struct task_struct *task;
    struct pid *pid_struct;
    int bytes_written;
    void *kbuf;

    if (size == 0 || size > (1024 * 1024)) // 限制最大1MB
        return false;

    if (!dyn_put_pid)
        return false;

    pid_struct = find_get_pid(pid);
    if (!pid_struct)
        return false;

    task = get_pid_task(pid_struct, PIDTYPE_PID);
    
    // 【修改点 3】：使用动态指针释放 PID
    dyn_put_pid(pid_struct); 
    
    if (!task)
        return false;

    kbuf = kmalloc(size, GFP_KERNEL);
    if (!kbuf) {
        put_task_struct(task);
        return false;
    }

    if (copy_from_user(kbuf, buffer, size)) {
        kfree(kbuf);
        put_task_struct(task);
        return false;
    }

    // 【修改点 4】：物理对齐 4.4 内核底层的 access_process_vm 写入参数
    // 4.4 内核最后一个参数传入 1 代表写入，强行规避高版本 FOLL_WRITE 导致的挂载崩溃
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 180)
    bytes_written = access_process_vm(task, addr, kbuf, size, 1); 
#else
    bytes_written = access_process_vm(task, addr, kbuf, size, FOLL_FORCE | FOLL_WRITE);
#endif

    put_task_struct(task);
    kfree(kbuf);

    return (bytes_written == size);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tear");
