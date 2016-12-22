#ifndef ONT_INCLUDE_ONT_PLATFORM_H_
#define ONT_INCLUDE_ONT_PLATFORM_H_

/* #define ONT_OS_WIN */

#if defined(ONT_OS_WIN) || defined(ONT_OS_POSIX)
#  include <stddef.h>  /* for size_t */
#  include <stdint.h>  /* for interger types */
#endif

# ifdef __cplusplus
extern "C" {
# endif

#include "config.h"



/**
 * socket句柄，移植时自行定义其具体属性
 */
typedef struct ont_socket_t ont_socket_t;


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

/**
 * 格式化输出到指定内存. 语义同C标准库中的snprintf
 * @param str 输出结果保存的内存
 * @param size 输出结果保存的内存的最大字节数
 * @param format 格式化字符串
 * @return 格式化输出成功后返回输出的字符个数(不包括结尾标识符'\0')
 */
int ont_platform_snprintf(char *str, size_t size, const char *format, ...);

/**
 * 获取当前时间，单位秒
 * @return 返回距离某固定时间点的秒数
 */
int32_t ont_platform_time();

/**
 * 睡眠 milliseconds 毫秒
 * @param  milliseconds 要睡眠的时间，单位：毫秒
 */
void ont_platform_sleep(int milliseconds);

/**
 * 创建TCP socket句柄， 必须将其设置为非阻塞模式
 * @param  sock 指向保存 TCP socket句柄的内存
 * @return 成功则返回ONT_ERR_OK
 */
int ont_platform_tcp_create(ont_socket_t **sock);

/**
 * 建立TCP连接
 * @param sock socket句柄
 * @param ip 服务端IP地址
 * @param port 服务端端口
 * @return 成功则返回ONT_ERR_OK，如果正在连接，返回ONT_ERR_CONNECTING
 */
int ont_platform_tcp_connect(ont_socket_t *sock, const char *ip, uint16_t port);

/**
 * 发送数据
 * @param sock socket句柄 
 * @param buf  指向将要被发送的数据
 * @param size 将要被发送数据的字节数
 * @param bytes_sent [OUT] 成功发送的字节数
 * @return 发送数据正常，返回ONT_ERR_OK
 */
int ont_platform_tcp_send(ont_socket_t *sock, const char *buf,
                          unsigned int size, unsigned int *bytes_sent);
/**
 * 接收数据
 * @param sock socket句柄
 * @param buf  指向存储接收数据的缓冲区
 * @param size 接收数据缓冲区最大字节数
 * @param bytes_read [OUT] 写入缓冲区的字节数
 * @return 接收数据正常，返回ONT_ERR_OK
 */
int ont_platform_tcp_recv(ont_socket_t *sock, char *buf,
                          unsigned int size, unsigned int *bytes_read);

/**
 * 关闭TCP socket句柄
 * @param sock 将要被关闭的socket句柄
 */
int ont_platform_tcp_close(ont_socket_t *sock);

#ifdef ONT_PLATFORM_PERSISTENCE_SUPPORT /* 定义后系统支持持久化存储设备状态 */
/**
 * 持久化保存设备状态数据
 * @param status 指向设备状态数据块
 * @param size 设备状态数据块字节数
 * @return 保存成功返回ONT_ERR_OK
 * @remark 存储设备状态的过程中，要防止存储的数据被写坏
 */
int ont_platform_save_device_status(const char *status, size_t size);
/**
 * 加载设备状态数据
 * @param status 指向用于保存设备状态数据的内存
 * @param size 加载的设备状态的字节数
 * @return 加载成功返回ONT_ERR_OK
 */
int ont_platform_load_device_status(char *status, size_t size);
#endif /* ONT_PLATFORM_PERSISTENCE_SUPPORT */

# ifdef __cplusplus
}      /* extern "C" */
# endif

#endif /* ONT_INCLUDE_ONT_PLATFORM_H_ */