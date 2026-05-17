/**
 * ============================================================================
 * 泪心开源驱动 - TearGame Open Source Driver (Kprobe 符号过检测版入口)
 * ============================================================================
 */

#include <linux/module.h>
#include <linux/tty.h>
#include <linux/miscdevice.h>
#include <linux/kprobes.h> // 必须引入此头文件以使用 Kprobe 绕过封锁
#include "comm.h"
#include "memory.h"
#include "process.h"

#define DEVICE_NAME "TearGame"

/* =================【定义全局解密函数指针】================= */
void (*dyn_put_pid)(struct pid *pid) = NULL;
char *(*dyn_d_path)(const struct path *path, char *buf, int buflen) = NULL;
void (*dyn_up_read)(struct rw_semaphore *sem) = NULL;

/* 前置声明，防止编译器报 prototype 警告 */
int dispatch_open(struct inode *node, struct file *file);
int dispatch_close(struct inode *node, struct file *file);
long dispatch_ioctl(struct file *const file, unsigned int const cmd, unsigned long const arg);
int __init driver_entry(void);
void __exit driver_unload(void);

/* 
 * =================【核心黑客算法：利用 Kprobe 动态白嫖被封锁的 API】================= 
 */
typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);
static kallsyms_lookup_name_t get_kallsyms_lookup_name(void)
{
    struct kprobe kp;
    kallsyms_lookup_name_t func_ptr = NULL;
    
    memset(&kp, 0, sizeof(struct kprobe));
    kp.symbol_name = "kallsyms_lookup_name"; // 让探针直接指向这个被封锁的函数
    
    if (register_kprobe(&kp) < 0) {
        return NULL;
    }
    
    // 成功挂载后，探针的 .addr 字段里存放的就是该函数的绝对内存地址
    func_ptr = (kallsyms_lookup_name_t)kp.addr;
    unregister_kprobe(&kp); // 拿到地址立刻卸载探针，安全无残留
    return func_ptr;
}

int dispatch_open(struct inode *node, struct file *file)
{
	return 0;
}

int dispatch_close(struct inode *node, struct file *file)
{
	return 0;
}

long dispatch_ioctl(struct file *const file, unsigned int const cmd, unsigned long const arg)
{
	static COPY_MEMORY cm;
	static MODULE_BASE mb;
	static char key[0x100] = {0};
	static char name[0x100] = {0};
	static bool is_verified = false;

	if (cmd == OP_INIT_KEY && !is_verified)
	{
		if (copy_from_user(key, (void __user *)arg, sizeof(key) - 1) != 0)
		{
			return -1;
		}
	}
	switch (cmd)
	{
	case OP_READ_MEM:
	{
		if (copy_from_user(&cm, (void __user *)arg, sizeof(cm)) != 0)
		{
			return -1;
		}
		if (read_process_memory(cm.pid, (uintptr_t)cm.addr, (void *)(uintptr_t)cm.buffer, (size_t)cm.size) == false)
		{
			return -1;
		}
		break;
	}
	case OP_WRITE_MEM:
	{
		if (copy_from_user(&cm, (void __user *)arg, sizeof(cm)) != 0)
		{
			return -1;
		}
		if (write_process_memory(cm.pid, (uintptr_t)cm.addr, (void *)(uintptr_t)cm.buffer, (size_t)cm.size) == false)
		{
			return -1;
		}
		break;
	}
	case OP_MODULE_BASE:
	{
		if (copy_from_user(&mb, (void __user *)arg, sizeof(mb)) != 0 || copy_from_user(name, (void __user *)(uintptr_t)mb.name, sizeof(name) - 1) != 0)
		{
			return -1;
		}
		mb.base = get_module_base(mb.pid, name);
		if (copy_to_user((void __user *)arg, &mb, sizeof(mb)) != 0)
		{
			return -1;
		}
		break;
	}
	default:
		break;
	}
	return 0;
}

struct file_operations dispatch_functions = {
	.owner = THIS_MODULE,
	.open = dispatch_open,
	.release = dispatch_close,
	.unlocked_ioctl = dispatch_ioctl,
};

struct miscdevice misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEVICE_NAME,
	.fops = &dispatch_functions,
};

int __init driver_entry(void)
{
	int ret;
	kallsyms_lookup_name_t local_kallsyms_lookup = NULL;

	printk(KERN_INFO "=============================================\n");
	printk(KERN_INFO "[TearGame] Driver loading...\n");
	printk(KERN_INFO "[TearGame] Author: 泪心 (Tear)\n");
	printk(KERN_INFO "=============================================\n");
	
	// 1. 先通过 Kprobe 绕过封锁解密核心查找器
	local_kallsyms_lookup = get_kallsyms_lookup_name();
	if (!local_kallsyms_lookup) {
		printk(KERN_ERR "[TearGame] 错误：无法通过 Kprobe 解密内核核心符号器！\n");
		return -EINVAL;
	}

	// 2. 使用成功解密的查找器提取雷电 4.4 内核的所有动态印记
	dyn_put_pid = (void (*)(struct pid *))local_kallsyms_lookup("put_pid");
	dyn_d_path  = (char *(*)(const struct path *, char *, int))local_kallsyms_lookup("d_path");
	dyn_up_read  = (void (*)(struct rw_semaphore *))local_kallsyms_lookup("up_read");

	if (!dyn_put_pid || !dyn_d_path || !dyn_up_read) {
		printk(KERN_ERR "[TearGame] 错误：动态解析雷电内核关键符号失败！\n");
		return -EINVAL;
	}
	printk(KERN_INFO "[TearGame] 成功！雷电内核运行指针已安全注入！\n");

	ret = misc_register(&misc);
	if (ret == 0) {
		printk(KERN_INFO "[TearGame] Device registered: /dev/%s\n", DEVICE_NAME);
		printk(KERN_INFO "[TearGame] Driver loaded successfully!\n");
	} else {
		printk(KERN_ERR "[TearGame] Failed to register device! ret=%d\n", ret);
	}
	return ret;
}

void __exit driver_unload(void)
{
	printk(KERN_INFO "[TearGame] Driver unloading...\n");
	misc_deregister(&misc);
	printk(KERN_INFO "[TearGame] Goodbye! - by 泪心\n");
}

module_init(driver_entry);
module_exit(driver_unload);

MODULE_DESCRIPTION("TearGame Memory Driver - t.me/TearGame");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("泪心 QQ:2254013571");
