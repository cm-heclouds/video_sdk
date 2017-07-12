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
#define ONT_SERVER_ADDRESS        "api.heclouds.com"

/**
 * @def ONT_SERVER_ADDRESS
 *
 * 服务器端口
 */
#define ONT_SERVER_PORT           80

/**
 * @def ONT_PROTOCOL_EDP
 *
 * SDK支持EDP协议
 * 注释后，SDK不再支持EDP协议，EDP协议相关代码不参与编译
 */
/* #undef ONT_PROTOCOL_EDP */

/**
 * @def ONT_PROTOCOL_MQTT
 *
 * SDK支持MQTT协议
 * 注释后，SDK不再支持MQTT协议，MQTT协议相关代码不参与编译
 */
#define ONT_PROTOCOL_MQTT

/**
 * @def ONT_PROTOCOL_MODBUS
 *
 * SDK支持MODBUS协议
 * 注释后，SDK不再支持MODBUS协议，MODBUS协议相关代码不参与编译
 */
/* #undef ONT_PROTOCOL_MODBUS */

/**
 * @def ONT_PROTOCOL_JTEXT
 *
 * SDK支持JTEXT协议
 * 注释后，SDK不再支持JTEXT协议，JTEXT协议相关代码不参与编译
 */
/* #undef ONT_PROTOCOL_JTEXT */



/**
 * @def ONT_PROTOCOL_VIDEO
 *
 * SDK支持VIDEO协议
 *
 */
#define ONT_PROTOCOL_VIDEO

/**
 * @def ONT_PLATFORM_PERSISTENCE_SUPPORT
 *
 * 平台支持持久化存储
 * 注释掉后将关闭持久化相关的代码
 */
#define ONT_PLATFORM_PERSISTENCE_SUPPORT

/**
 * @def ONT_PLATFORM_FLOATING_POINT_SUPPORT
 *
 * 平台支持浮点数
 * 注释掉后将关闭浮点相关的代码
 */
#define ONT_PLATFORM_FLOATING_POINT_SUPPORT

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
