#ifndef _ONT_PLATFORM_SOCKET_H_
#define _ONT_PLATFORM_SOCKET_H_


# ifdef __cplusplus
extern "C" {
# endif



/**
 * socket句柄，移植时自行定义其具体属性
 */
typedef struct ont_socket_t ont_socket_t;



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

int ont_platform_udp_create(ont_socket_t **sock);

/**
* 建立UDP连接
* @param sock socket句柄
* @param ip 服务端IP地址
* @param port 服务端端口
* @return 成功则返回ONT_ERR_OK，如果正在连接，返回ONT_ERR_CONNECTING
*/
int ont_platform_udp_send(ont_socket_t *sock, char *buf,
                          unsigned int size, unsigned int *bytes_sent);
/**
* 接收数据
* @param sock socket句柄
* @param buf  指向存储接收数据的缓冲区
* @param size 接收数据缓冲区最大字节数
* @param bytes_read [OUT] 写入缓冲区的字节数
* @return 接收数据正常，返回ONT_ERR_OK
*/
int ont_platform_udp_recv(ont_socket_t *sock, char *buf,
                          unsigned int size, unsigned int *bytes_read);

/**
* 关闭UDP socket句柄
* @param sock 将要被关闭的socket句柄
*/
int ont_platform_udp_close(ont_socket_t *sock);


/**
* 获取socket fd
*/
int ont_platform_tcp_socketfd(ont_socket_t *sock);



#ifdef __cplusplus
} /* extern "C" */
#endif




#endif


