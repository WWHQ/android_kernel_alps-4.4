/**
 * ============================================================================
 * 泪心开源驱动 - TearGame Open Source Driver (雷电 4.4.146 内存模块头文件)
 * ============================================================================
 */

#ifndef _MEMORY_H_
#define _MEMORY_H_

#include <linux/kernel.h>
#include <linux/types.h>

/* 
 * ============================================================================
 * 声明对外的内存读写核心业务接口
 * ============================================================================
 */
bool read_process_memory(pid_t pid, uintptr_t addr, void *buffer, size_t size);
bool write_process_memory(pid_t pid, uintptr_t addr, void *buffer, size_t size);

#endif /* _MEMORY_H_ */
