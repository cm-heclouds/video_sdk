#include "ont_mqtt_connect_flag.h"


uint8_t  GetUserFlag(mqtt_connect_flag *mqttConnFlag){ return (mqttConnFlag->connect_flag_ & USER_FLAG_BIT) >> USER_FLAG_SHIFT; }

uint8_t  GetPasswordFlag(mqtt_connect_flag *mqttConnFlag){ return (mqttConnFlag->connect_flag_ & USER_PASS_BIT) >> PASS_FLAG_SHIFT; }

uint8_t  GetWillRetainFlag(mqtt_connect_flag *mqttConnFlag) { return (mqttConnFlag->connect_flag_ & WILL_RETAIN_BIT) >> WILL_RETAIN_FLAG_SHIFT; }

uint8_t  GetWillQosFlag(mqtt_connect_flag *mqttConnFlag) { return (mqttConnFlag->connect_flag_ & WILL_QOS_BIT) >> WILL_QOS_FLAG_SHIFT; }

uint8_t  GetWillFlag(mqtt_connect_flag *mqttConnFlag) { return (mqttConnFlag->connect_flag_ & WILL_BIT) >> WILL_FLAG_SHIFT; }

uint8_t  GetCleanSessionFlag(mqtt_connect_flag *mqttConnFlag) { return (mqttConnFlag->connect_flag_ & CLEAN_SESSION_BIT) >> CLEAN_SESSION_SHIFT; }

uint8_t  GetReserveFlag(mqtt_connect_flag *mqttConnFlag) { return (mqttConnFlag->connect_flag_ & RESERVE_BIT) >> RESERVE_FLAG_SHIFT; }

void SetUserFlag(mqtt_connect_flag *mqttConnFlag,const uint8_t value){ mqttConnFlag->connect_flag_ |= ((value << USER_FLAG_SHIFT) & USER_FLAG_BIT); }
	
void SetPasswordFlag(mqtt_connect_flag *mqttConnFlag,const uint8_t value){ mqttConnFlag->connect_flag_ |= ((value << PASS_FLAG_SHIFT) & USER_PASS_BIT); }
	
void SetWillRetainFlag(mqtt_connect_flag *mqttConnFlag,const uint8_t value){ mqttConnFlag->connect_flag_ |= ((value << WILL_RETAIN_FLAG_SHIFT) & WILL_RETAIN_BIT); }
	
void SetWillQosFlag(mqtt_connect_flag *mqttConnFlag,const uint8_t value){ mqttConnFlag->connect_flag_ |= ((value << WILL_QOS_FLAG_SHIFT) & WILL_QOS_BIT); }
	
void SetWillFlag(mqtt_connect_flag *mqttConnFlag,const uint8_t value){ mqttConnFlag->connect_flag_ |= ((value << USER_FLAG_SHIFT) & USER_FLAG_BIT); }
	
void SetCleanSessionFlag(mqtt_connect_flag *mqttConnFlag,const uint8_t value){ mqttConnFlag->connect_flag_ |= ((value << CLEAN_SESSION_SHIFT) & CLEAN_SESSION_BIT); }
	
void SetReserveFlag(mqtt_connect_flag *mqttConnFlag,const uint8_t value){ mqttConnFlag->connect_flag_ |= ((value << RESERVE_FLAG_SHIFT) & RESERVE_BIT); }
