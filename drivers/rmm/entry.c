/**
 * ============================================================================
 * 泪心开源驱动 - TearGame Open Source Driver (雷电 4.4.146 完美整合版入口)
 * ============================================================================
 */

#include <linux/module.h>
#include <linux/tty.h>
#include <linux/miscdevice.h>
#include <linux/kallsyms.h> // 引入解密内核符号的关键头文件
#include "comm.h"
#include "memory.h"
#include "process.h"

#define DEVICE_NAME "TearGame"

/* =================【跨文件共享的动态函数指针定义】================= */
void (*dyn_put_pid)(struct pid *pid) = NULL;
char *(*dyn_d_path)(const struct path *path, char *buf, int buflen) = NULL;
void (*dyn_up_read)(struct rw_semaphore *sem) = NULL;

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
		if (read_process_memory(cm.pid, cm.addr, cm.buffer, cm.size) == false)
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
		if (write_process_memory(cm.pid, cm.addr, cm.buffer, cm.size) == false)
		{
			return -1;
		}
		break;
	}
	case OP_MODULE_BASE:
	{
		if (copy_from_user(&mb, (void __user *)arg, sizeof(mb)) != 0 || copy_from_user(name, (void __user *)mb.name, sizeof(name) - 1) != 0)
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

/* =================【唯一下载/启动入口】================= */
int __init driver_entry(void)
{
	int ret;
	printk(KERN_INFO "=============================================\n");
	printk(KERN_INFO "[TearGame] Driver loading...\n");
	printk(KERN_INFO "[TearGame] Author: 泪心 (Tear)\n");
	printk(KERN_INFO "[TearGame] QQ: 2254013571\n");
	printk(KERN_INFO "[TearGame] Email: tearhacker@outlook.com\n");
	printk(KERN_INFO "[TearGame] Telegram: t.me/TearGame\n");
	printk(KERN_INFO "[TearGame] GitHub: github.com/tearhacker\n");
	printk(KERN_INFO "=============================================\n");
	
	// 【核心动作】：在注册设备前，秒速解密雷电 4.4 内核的所有动态印记
	dyn_put_pid = (void (*)(struct pid *))kallsyms_lookup_name("put_pid");
	dyn_d_path  = (char *(*)(const struct path *, char *, int))kallsyms_lookup_name("d_path");
	dyn_up_read  = (void (*)(struct rw_semaphore *))kallsyms_lookup_name("up_read");

	if (!dyn_put_pid || !dyn_d_path || !dyn_up_read) {
		printk(KERN_ERR "[TearGame] 致命错误：解密雷电内核符号失败！拒绝挂载。\n");
		return -EINVAL;
	}
	printk(KERN_INFO "[TearGame] 恭喜！雷电 4.4.146 全量内核符号动态指针绑定成功！\n");

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
	printk(KERN_INFO "[TearGame] Device /dev/%s unregistered\n", DEVICE_NAME);
	printk(KERN_INFO "[TearGame] Goodbye! - by 泪心\n");
}

module_init(driver_entry);
module_exit(driver_unload);

MODULE_DESCRIPTION("TearGame Memory Driver - t.me/TearGame");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("泪心 QQ:2254013571");
