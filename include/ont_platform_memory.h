#ifndef _ONT_PLATFORM_MEMORY_H_
#define _ONT_PLATFORM_MEMORY_H_


# ifdef __cplusplus
extern "C" {
# endif

#include "config.h"

/**
 * 分配内存
 * @param  size 需要分配的内存字节数
 * @return 分配成功后返回指向新分配的内存指针，否则返回 NULL
 */
void *ont_platform_malloc(size_t size);

/**
 * 释放由 @see ont_platform_malloc分配的内存
 * @param ptr 指向将要被释放的内存
 */
void ont_platform_free(void *ptr);


# ifdef __cplusplus
}      /* extern "C" */
# endif

#endif /* _ONT_PLATFORM_MEMORY_H_ */
