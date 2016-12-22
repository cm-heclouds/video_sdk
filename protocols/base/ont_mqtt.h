#ifndef ONT_SRC_COMM_ONT_MQTT_H_
#define ONT_SRC_COMM_ONT_MQTT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ont/platform.h"
#include "ont/device.h"

/**
 * 创建mqtt设备
 * @param device 外部公用接口的ont_device_t指针
 * @return 成功则返回ONT_ERR_OK
 */
int ont_mqtt_create(ont_device_t **dev);

/**
 * 销毁mqtt设备
 * @param device 创建的mqtt设备的指针
 */
void ont_mqtt_destroy(ont_device_t *device);


/**
 * 连接Server
 * @param device 创建的mqtt设备的指针,包含有dev的ip地址和端口
 * @param auth_info 鉴权信息
 * @return 成功则返回ONT_ERR_OK
 */
int ont_mqtt_connect(ont_device_t *device,const char *auth_info);

/**
 * 断开连接
 * @param device 创建的mqtt设备的指针
 */
void ont_mqtt_disconnect(ont_device_t *device);

/**
 * 连接保活
 * @param device 创建的mqtt设备的指针
 * @return 成功则返回ONT_ERR_OK
 */
int ont_mqtt_keepalive(ont_device_t *device);

/**
 * 发送数据
 * @param device 创建的mqtt设备的指针
 * @param payload 发送数据的起始地址
 * @param bytes 发送数据的长度
 * @return 成功则返回ONT_ERR_OK
 */
int ont_mqtt_send_datapoints(ont_device_t *device,
								 const char* payload,
								 size_t bytes);

/**
 * 获取平台下发的命令
 * @param device 创建的mqtt设备的指针
 * @return 成功则返回ont_device_cmd_t命令地址，否则返回NULL
 * 注：在获取命令后须释放内存，先调用ont_platform_free(cmd->req),
 * 同时将 ont_device_cmd_t->req指针 设置为NULL,
 * 然后调用ont_platform_free(ont_device_cmd_t*);将ont_device_cmd_t 设置为NULL
 */
ont_device_cmd_t* ont_mqtt_get_cmd(ont_device_t *device);

/**
 * 回复命令
 * @param device 创建的mqtt设备的指针
 * @param cmdid 命令的id
 * @param resp 回复命令的内容
 * @param bytes 回复命令的长度
 * @param qos_level qos级别，1：返回确认ack，0：不返回
 * @return 成功则返回ONT_ERR_OK
 */
int ont_mqtt_reply_cmd(ont_device_t *device,
						   const char *cmdid,
						   const char *resp,
						   size_t bytes);


#ifdef __cplusplus
} /* extern "C" */
#endif
#endif
