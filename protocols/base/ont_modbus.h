#ifndef ONT_SRC_COMM_ONT_MODBUS_H_
#define ONT_SRC_COMM_ONT_MODBUS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ont/error.h"
#include "ont/device.h"

#ifdef ONT_PROTOCOL_MODBUS

/**
 * 创建OneNET接入设备实例
 * @param dev [OUT] 保存创建后的OneNET接入设备实例
 * @return 创建成功返回ONT_ERR_OK
 * @remark 不使用OneNet接入设备实例后，调用@see ont_modbus_destroy销毁
**/
int ont_modbus_create( ont_device_t **dev );

/**
 * 销毁ONENET接入设备实例
 * @param dev OneNET接入设备实例
**/
void ont_modbus_destroy( ont_device_t *dev );

/**
 * 与OneNET建立连接
 * @param dev OneNET接入设备实例
 * @return 连接成功返回ONT_ERR_OK
**/
int ont_modbus_connect( ont_device_t *dev, const char *auth_info );

/**
 * 发送数据点到OneNET
 * @param dev     OneNET接入设备实例
 * @param payload 按格式8序列化数据点
 * @param bytes   数据点字节数
 * @return 成功则返回ONT_ERR_OK
**/
int ont_modbus_send_datapoints( ont_device_t *dev,
                                const char *payload,
                                size_t bytes );

/**
 * 获取OneNET平台下发命令
 * @param dev OneNET接入设备实例
 * @return 获取成功返回有效ont_device_cmd_t指针
**/
ont_device_cmd_t* ont_modbus_get_cmd( ont_device_t *dev );

/**
 * 发送命令响应数据到OneNET
 * @param dev   OneNET接入设备实例
 * @param cmdid OneNET平台下发命令的唯一标识
 * @param resp  设备响应数据
 * @param size  响应数据大小
 * @return 发送成功返回ONT_ERR_OK
**/
int ont_modbus_reply_cmd( ont_device_t *dev,
                          const char *cmdid,
                          const char *resp,
                          size_t size );

/**
 * 检查设备连接、发送心跳、接收OneNET平台下发数据
 * @param dev   OneNET接入设备实例
**/
int ont_modbus_keepalive( ont_device_t *dev );

#endif /* ONT_PROTOCOL_MODBUS */

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* ONT_SRC_COMM_ONT_MODBUS_H_ */