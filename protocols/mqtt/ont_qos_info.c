#include "ont_qos_info.h"

uint8_t  get_qos_level(qos_info *info){ return (info->qos_tag_ & QOS_BIT) >> QOS_SHIFT; }

uint8_t  get_dup_flag(qos_info *info){ return (info->qos_tag_ & DUP_BIT) >> DUP_SHIFT; }

uint8_t  get_retain_flag(qos_info *info) { return (info->qos_tag_ & RETAIN_BIT) >> RETAIN_SHIFT; }

void set_qos_level(qos_info *info,const uint8_t qos_level){ info->qos_tag_ |= ((qos_level << QOS_SHIFT) & QOS_BIT); }

void set_dup_flag(qos_info *info,const uint8_t dup_flag){ info->qos_tag_ |= ((dup_flag << DUP_SHIFT) & DUP_BIT); }

void set_retain_flag(qos_info *info,const uint8_t retain_flag){ info->qos_tag_ |= ((retain_flag << RETAIN_SHIFT) & RETAIN_BIT); }
