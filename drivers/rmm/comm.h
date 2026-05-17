#ifndef _COMM_H_
#define _COMM_H_

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#include <sys/types.h>
#endif

/* 
 * ============================================================================
 * 使用固定长度类型（uint64_t）强制对齐指针与长度，彻底消除 32位App 与 64位驱动的对齐断层
 * ============================================================================
 */
typedef struct _COPY_MEMORY
{
	uint32_t pid;       // 显式 4 字节，兼容所有架构的 pid_t
	uint32_t pad;       // 填充对齐
	uint64_t addr;      // 显式 8 字节，完美承载 64 位物理/虚拟地址
	uint64_t buffer;    // 强转为 uint64_t 传输指针，防止 32/64 位下指针长度不一致
	uint64_t size;      // 强转为 uint64_t 传输长度，防止 size_t 在不同架构下变形
} COPY_MEMORY, *PCOPY_MEMORY;

typedef struct _MODULE_BASE
{
	uint32_t pid;       // 显式 4 字节
	uint32_t pad;       // 填充对齐
	uint64_t name;      // 强转为 uint64_t 传输字符串指针（指向模块名字）
	uint64_t base;      // 显式 8 字节返回基址
} MODULE_BASE, *PMODULE_BASE;

/* 
 * ============================================================================
 * 严格按照 Linux 内核标准的 IOCTL 宏生成控制码。
 * 如果直接用 0x801 这种裸数，当 32位App 调用 64位内核时，内核的 compat_ioctl 会直接拦截拒绝！
 * ============================================================================
 */
#define TEARGAME_IOC_MAGIC 'T'

enum OPERATIONS
{
	OP_INIT_KEY    = _IOW(TEARGAME_IOC_MAGIC, 0x00, char[256]),
	OP_READ_MEM    = _IOWR(TEARGAME_IOC_MAGIC, 0x01, COPY_MEMORY),
	OP_WRITE_MEM   = _IOWR(TEARGAME_IOC_MAGIC, 0x02, COPY_MEMORY),
	OP_MODULE_BASE = _IOWR(TEARGAME_IOC_MAGIC, 0x03, MODULE_BASE),
};

#endif
