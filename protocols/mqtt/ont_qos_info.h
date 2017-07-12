#ifndef ONT_PROTOCOLS_MQTT_ONT_QOS_INFO_H_
#define ONT_PROTOCOLS_MQTT_ONT_QOS_INFO_H_


#ifdef __cplusplus
extern "C" {
#endif

#include "ont/platform.h"

#define  QOS_BIT   0x06
#define  DUP_BIT   0x08
#define  RETAIN_BIT 0x01

#define  QOS_SHIFT  1
#define  DUP_SHIFT  3
#define  RETAIN_SHIFT 0

typedef struct qos_info
{
	uint8_t qos_tag_;
	uint16_t packetid_;
	uint32_t qos_info_int32_;
}qos_info;



uint8_t  get_qos_level(qos_info *info);

uint8_t  get_dup_flag(qos_info *info);

uint8_t  get_retain_flag(qos_info *info);

void set_qos_level(qos_info *info,const uint8_t qos_level);

void set_dup_flag(qos_info *info,const uint8_t dup_flag);

void set_retain_flag(qos_info *info,const uint8_t retain_flag);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
