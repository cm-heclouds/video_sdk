#ifndef ONT_INCLUDE_ONT_MQTT_H_
#define ONT_INCLUDE_ONT_MQTT_H_


#ifdef __cplusplus
extern "C" {
#endif


#include "platform.h"
#include "device.h"

/**
 * 订阅topic
 * @param device 创建的mqtt设备的指针
 * @param topic_arry 订阅的topic的二维字符串数组的起始地址(用于订阅多个topic)
 * @param size 订阅的topic的总个数(即二维数据的第一维长度)
 * @return 成功则返回ONT_ERR_OK
 */
int ont_mqtt_sub_topic(ont_device_t *device,
						   const char **topic_arry,
						   size_t size);

/**
 * 取消订阅topic
 * @param device 创建的mqtt设备的指针
 * @param topic_arry 取消订阅的topic的二维字符串数组的起始地址(用于取消订阅多个topic)
 * @param size 取消订阅的topic的总个数(即二维数据的第一维长度)
 * @return 成功则返回ONT_ERR_OK
 */
int ont_mqtt_unsub_topic(ont_device_t *device,
							 const char **topic_arry,
							 size_t size);

/**
 * Topic推送消息
 * @param device 创建的mqtt设备的指针
 * @param topic_name 推送消息的topic名称
 * @param content 推送消息的内容
 * @param bytes 推送消息的内容的长度
 * @param qos_level 1：返回确认ack，0：不返回
 * @return 成功则返回ONT_ERR_OK
 */
int ont_mqtt_publish(ont_device_t *device,
					     const char *topic_name,
						 const char*content,
						 size_t bytes,
						 uint8_t qos_level);

#ifdef __cplusplus
} /* extern "C" */
#endif
						 
#endif