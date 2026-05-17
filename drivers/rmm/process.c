/**
 * ============================================================================
 * 泪心开源驱动 - TearGame Open Source Driver (雷电 4.4.146 兼容修正版)
 * ============================================================================
 */

#include "process.h"
#include <linux/sched.h>   
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/version.h>
#include <linux/pid.h>
#include <linux/fs.h>
#include <linux/dcache.h>
#include <linux/rwsem.h>
#include <linux/kallsyms.h> // 必须引入此头文件用于动态地址查找

#define ARC_PATH_MAX 256

/* =================【定义动态函数指针类型】================= */
static void (*dyn_put_pid)(struct pid *pid) = NULL;
static char *(*dyn_d_path)(const struct path *path, char *buf, int buflen) = NULL;
static void (*dyn_up_read)(struct rw_semaphore *sem) = NULL;

/* =================【驱动初始化：运行时自动解密符号地址】================= */
static int __init teargame_driver_init(void)
{
    printk(KERN_INFO "[TearGame] 开始动态解析雷电内核符号...\n");

    // 从雷电内核内存中强行提取真实函数指针
    dyn_put_pid = (void (*)(struct pid *))kallsyms_lookup_name("put_pid");
    dyn_d_path  = (char *(*)(const struct path *, char *, int))kallsyms_lookup_name("d_path");
    dyn_up_read  = (void (*)(struct rw_semaphore *))kallsyms_lookup_name("up_read");

    // 安全检查，防止解析失败导致挂载崩溃
    if (!dyn_put_pid || !dyn_d_path || !dyn_up_read) {
        printk(KERN_ERR "[TearGame] 错误：无法动态获取内核关键符号地址！\n");
        return -EINVAL;
    }

    printk(KERN_INFO "[TearGame] 雷电 4.4.146 专属符号解密绑定成功！\n");
    return 0;
}

static void __exit teargame_driver_exit(void)
{
    printk(KERN_INFO "[TearGame] 驱动已成功卸载。\n");
}

module_init(teargame_driver_init);
module_exit(teargame_driver_exit);

/* =================【核心业务逻辑重构】================= */
uintptr_t get_module_base(pid_t pid, char *name)
{
    struct pid *pid_struct;
    struct task_struct *task;
    struct mm_struct *mm;
    struct vm_area_struct *vma;
    uintptr_t base_addr = 0;

    // 如果初始化未完成或失败，直接拦截
    if (!dyn_put_pid || !dyn_d_path || !dyn_up_read)
        return 0;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
    struct vma_iterator vmi;
#endif

    pid_struct = find_get_pid(pid);
    if (!pid_struct)
        return 0;

    task = get_pid_task(pid_struct, PIDTYPE_PID);
    
    // 【修改点 1】：使用动态解密的指针释放 PID
    dyn_put_pid(pid_struct); 
    
    if (!task)
        return 0;

    mm = get_task_mm(task);
    put_task_struct(task);
    if (!mm)
        return 0;

    /* 兼容旧内核版本的 mmap 锁 API */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
    down_read(&mm->mmap_sem); // 4.4.146 环境下直接展开
#else
    mmap_read_lock(mm);
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
    vma_iter_init(&vmi, mm, 0);
    for_each_vma(vmi, vma)
#else
    for (vma = mm->mmap; vma; vma = vma->vm_next)
#endif
    {
        if (vma->vm_file) {
            char buf[ARC_PATH_MAX];
            char *path_nm;

            // 【修改点 2】：使用动态解密的指针获取 VMA 路径
            path_nm = dyn_d_path(&vma->vm_file->f_path, buf, ARC_PATH_MAX - 1);
            if (!IS_ERR(path_nm)) {
                const char *basename = kbasename(path_nm);
                if (strcmp(basename, name) == 0) {
                    base_addr = vma->vm_start;
                    break;
                }
            }
        }
    }

    /* 释放内存锁 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
    // 【修改点 3】：使用动态解密的指针释放读写信号量
    dyn_up_read(&mm->mmap_sem); 
#else
    mmap_read_unlock(mm);
#endif

    mmput(mm);
    return base_addr;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tear");
MODULE_DESCRIPTION("TearGame Core Memory Driver");
