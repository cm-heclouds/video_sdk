#ifndef ONT_PROTOCOLS_MQTT_ONT_MQTT_CONNECT_FLAG_H_
#define ONT_PROTOCOLS_MQTT_ONT_MQTT_CONNECT_FLAG_H_


#ifdef __cplusplus
extern "C" {
#endif

#include "ont/platform.h"

#define  USER_FLAG_BIT		     (1<<7)
#define  USER_PASS_BIT		     (1<<6)
#define  WILL_RETAIN_BIT	     (1<<5)
#define  WILL_QOS_BIT			 (3<<3)
#define  WILL_BIT				 (1<<2)
#define  CLEAN_SESSION_BIT       (1<<1)
#define  RESERVE_BIT             (1<<0)

#define  USER_FLAG_SHIFT		 7
#define  PASS_FLAG_SHIFT		 6
#define  WILL_RETAIN_FLAG_SHIFT  5
#define  WILL_QOS_FLAG_SHIFT     3
#define  WILL_FLAG_SHIFT         2
#define  CLEAN_SESSION_SHIFT     1
#define  RESERVE_FLAG_SHIFT      0

typedef struct mqtt_connect_flag
{
	uint8_t connect_flag_;
}mqtt_connect_flag;



uint8_t  GetUserFlag(mqtt_connect_flag *mqttConnFlag);

uint8_t  GetPasswordFlag(mqtt_connect_flag *mqttConnFlag);

uint8_t  GetWillRetainFlag(mqtt_connect_flag *mqttConnFlag);

uint8_t  GetWillQosFlag(mqtt_connect_flag *mqttConnFlag);

uint8_t  GetWillFlag(mqtt_connect_flag *mqttConnFlag);

uint8_t  GetCleanSessionFlag(mqtt_connect_flag *mqttConnFlag);

uint8_t  GetReserveFlag(mqtt_connect_flag *mqttConnFlag);

void SetUserFlag(mqtt_connect_flag *mqttConnFlag,const uint8_t value);
	
void SetPasswordFlag(mqtt_connect_flag *mqttConnFlag,const uint8_t value);
	
void SetWillRetainFlag(mqtt_connect_flag *mqttConnFlag,const uint8_t value);
	
void SetWillQosFlag(mqtt_connect_flag *mqttConnFlag,const uint8_t value);

void SetWillFlag(mqtt_connect_flag *mqttConnFlag,const uint8_t value);
	
void SetCleanSessionFlag(mqtt_connect_flag *mqttConnFlag,const uint8_t value);
	
void SetReserveFlag(mqtt_connect_flag *mqttConnFlag,const uint8_t value);

#ifdef __cplusplus
}      /* extern "C" */
#endif
#endif