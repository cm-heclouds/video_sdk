#ifndef ONT_INCLUDE_ONT_CONFIG_H_
#define ONT_INCLUDE_ONT_CONFIG_H_

/**
 * @def ONT_SOCK_SEND_BUF_SIZE
 *
 * socket通信时，发送缓冲区的最大字节数
 */
#define ONT_SOCK_SEND_BUF_SIZE    4096

/**
 * @def ONT_SOCK_RECV_BUF_SIZE
 *
 * socket通信时， 接收缓冲区的最大字节数
 */
#define ONT_SOCK_RECV_BUF_SIZE    2048

/**
 * @def ONT_SSERVER_ADDRESS
 *
 * 服务器地址
 */
#define ONT_SERVER_ADDRESS        "183.230.40.42"

/**
 * @def ONT_SERVER_ADDRESS
 *
 * 服务器端口
 */
#define ONT_SERVER_PORT           9101

/**
 * @def ONT_SERVER_PATH
 *
 * 服务器路径
 */
/* #undef ONT_SERVER_PATH */


/**
 * @def ONT_SECURITY_SUPPORT
 *
 * 平台支持加密
 * 打开后需要移植加密相关的接口
 */
/* #undef ONT_SECURITY_SUPPORT */

/**
 * \def ONT_DEBUG
 *
 * 调试模式标记
 * 打开后，会增加调试相关的代码
 */
/* #undef ONT_DEBUG */

#endif /* ONT_INCLUDE_ONT_CONFIG_H_ */
