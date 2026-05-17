/**
 * ============================================================================
 * 泪心开源驱动 - TearGame Open Source Driver (内存读写模块修正版)
 * ============================================================================
 */

#include "memory.h"
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/pid.h>

// 声明外部主入口已经动态解密好的内核指针
extern void (*dyn_put_pid)(struct pid *pid);

/* =================【核心业务逻辑：读取进程内存】================= */
bool read_process_memory(pid_t pid, uintptr_t addr, void *buffer, size_t size)
{
    struct task_struct *task;
    struct pid *pid_struct;
    int bytes_read;
    void *kbuf;

    if (size == 0 || size > (1024 * 1024)) 
        return false;

    if (!dyn_put_pid)
        return false;

    pid_struct = find_get_pid(pid);
    if (!pid_struct)
        return false;

    task = get_pid_task(pid_struct, PIDTYPE_PID);
    
    // 使用外部主文件解密出的函数指针
    dyn_put_pid(pid_struct); 
    
    if (!task)
        return false;

    kbuf = kmalloc(size, GFP_KERNEL);
    if (!kbuf) {
        put_task_struct(task);
        return false;
    }

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

    if (size == 0 || size > (1024 * 1024)) 
        return false;

    if (!dyn_put_pid)
        return false;

    pid_struct = find_get_pid(pid);
    if (!pid_struct)
        return false;

    task = get_pid_task(pid_struct, PIDTYPE_PID);
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

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 180)
    bytes_written = access_process_vm(task, addr, kbuf, size, 1); 
#else
    bytes_written = access_process_vm(task, addr, kbuf, size, FOLL_FORCE | FOLL_WRITE);
#endif

    put_task_struct(task);
    kfree(kbuf);

    return (bytes_written == size);
}
