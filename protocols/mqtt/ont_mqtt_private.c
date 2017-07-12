#include <string.h>
#include "ont/error.h"
#include "ont_mqtt_private.h"
#include "ont_qos_info.h"
#include "ont_mqtt_connect_flag.h"
#include "parser/macros.h"

/*parse mqtt_connect_packet*/
    ont_parser_init_begin(mqtt_connect_packet, protocol_name_, bytes)
    ont_parser_queue_up(protocol_level_, uint8, NULL, none)
    ont_parser_queue_up(connect_flag_, uint8, NULL, none)
    ont_parser_queue_up(keep_alive, uint16, NULL, none)
    ont_parser_queue_up(client_id_, bytes, NULL, none)
    ont_parser_queue_up(usr_name_, bytes, NULL, none)
    ont_parser_queue_up(password_, bytes, NULL, none)
ont_parser_init_end()

    /*parse mqtt_connect_ack*/
    ont_parser_init_begin(mqtt_connect_ack,ack_flags_,uint8)
    ont_parser_queue_up(ret_code_, uint8, NULL, none)
ont_parser_init_end()


    /*parse mqtt_publish_packet_1*/
    ont_parser_init_begin(mqtt_publish_packet_1, topic_name, bytes)
    ont_parser_queue_up(packet_identifier_, uint16, NULL, none)
    ont_parser_queue_up(payload_, raw, NULL, none)
ont_parser_init_end()

    /*parse mqtt_publish_ack_1,only publish_packet_1 has ack*/
    ont_parser_init_begin(mqtt_publish_ack, packet_identifier_, uint16)
ont_parser_init_end()

    /*parse mqtt_publish_packet_0*/
    ont_parser_init_begin(mqtt_publish_packet_0, topic_name, bytes)
    ont_parser_queue_up(payload_, raw, NULL, none)
ont_parser_init_end()

    /*parse mqtt_send_dp_packet_1*/
    ont_parser_init_begin(mqtt_send_dp_packet_1, topic_name, bytes)
    ont_parser_queue_up(packet_identifier_, uint16, NULL, none)
    ont_parser_queue_up(data_type,uint8,NULL,none)
    ont_parser_queue_up(payload_, bytes, NULL, none)
ont_parser_init_end()

    /*parse mqtt_subscribe_packet*/
    ont_parser_init_begin(mqtt_subscribe_packet, topic, bytes)
    ont_parser_queue_up(qos, uint8, NULL, none)
ont_parser_init_end()

    /*parse mqtt_subscribe_ack*/
    ont_parser_init_begin(mqtt_subscribe_ack, packet_identifier_, uint16)
    ont_parser_queue_up(Results, raw, NULL, none)
ont_parser_init_end()

    /*parse mqtt_unsub_packet*/
    ont_parser_init_begin(mqtt_unsub_packet, topic, bytes)
ont_parser_init_end()

    /*parse mqtt_unsub_ack*/
    ont_parser_init_begin(mqtt_unsub_ack, packet_identifier_, uint16)
ont_parser_init_end()

    /*
       ont_mqtt_subscribe_begin
       ont_mqtt_subscribe_packet_id
       ont_mqtt_subscribe_add_topic
       ont_mqtt_subscribe_end
       这几个函数只适用于topic订阅
       */
int ont_mqtt_subscribe_begin(ont_device_t* device, uint8_t pkt_tag)
{
    ont_pkt_formatter_reset(device->formatter,0);
    device->formatter->header.type.value = pkt_tag;

    return ONT_ERR_OK;
}
int ont_mqtt_subscribe_packet_id(ont_device_t* device, uint16_t pkt_id)
{
    ont_pkt_formatter_realloc(device->formatter, 7);

    device->formatter->buf[5] = (pkt_id >> 8) & 0xFF;
    device->formatter->buf[6] = pkt_id & 0xFF;
    device->formatter->offset = 7;
    return ONT_ERR_OK;
}
int ont_mqtt_subscribe_add_topic(ont_device_t* device,const char* topic)
{
    mqtt_subscribe_packet pkt;
    ont_pkt_formatter_t* formatter = NULL;
    size_t len = 0;
    if (NULL == device || NULL == topic)
    {
	return ONT_ERR_BADPARAM;
    }

    ont_parser_init_mqtt_subscribe_packet(&pkt);
    pkt.topic.value = topic;
    pkt.topic.len = strlen(topic);
    pkt.qos.value = 0;

    formatter = device->formatter;
    ont_parser_len(&pkt.head,&len);
    ont_pkt_formatter_realloc(formatter, formatter->offset + len);

    len = 0;

    if (ont_parser_serialize((formatter->buf + formatter->offset), formatter->cap - formatter->offset, &pkt.head))
    {
	return ONT_ERR_BADPARAM;
    }

    ont_parser_len(&pkt.head,&len);
    formatter->offset += len;
    return ONT_ERR_OK;
}
int ont_mqtt_subscribe_end(ont_device_t* device)
{
    return ont_pkt_formatter_finish(device->formatter);
}

/*
   ont_mqtt_unsub_begin
   ont_mqtt_unsub_packet_id
   ont_mqtt_unsub_add_topic
   ont_mqtt_unsub_end
   这几个函数只适用于取消订阅
   */
int ont_mqtt_unsub_begin(ont_device_t* device, uint8_t pkt_tag)
{
    ont_pkt_formatter_reset(device->formatter,0);
    device->formatter->header.type.value = pkt_tag;

    return ONT_ERR_OK;
}
int ont_mqtt_unsub_packet_id(ont_device_t* device, uint16_t pkt_id)
{
    ont_pkt_formatter_realloc(device->formatter, 7);

    device->formatter->buf[5] = (pkt_id >> 8) & 0xFF;
    device->formatter->buf[6] = pkt_id & 0xFF;
    device->formatter->offset = 7;
    return ONT_ERR_OK;
}
int ont_mqtt_unsub_add_topic(ont_device_t* device,const char* topic)
{
    mqtt_unsub_packet pkt;
    ont_pkt_formatter_t* formatter = NULL;
    size_t len = 0;
    if (NULL == device || NULL == topic){
	return ONT_ERR_BADPARAM;
    }

    ont_parser_init_mqtt_unsub_packet(&pkt);
    pkt.topic.value = topic;
    pkt.topic.len = strlen(topic);

    formatter = device->formatter;

    ont_parser_len(&pkt.head,&len);
    ont_pkt_formatter_realloc(formatter, formatter->offset + len);

    len = 0;

    if (ont_parser_serialize((formatter->buf + formatter->offset), formatter->cap - formatter->offset, &pkt.head)){
	return ONT_ERR_BADPARAM;
    }

    ont_parser_len(&pkt.head,&len);
    formatter->offset += len;
    return ONT_ERR_OK;
}
int ont_mqtt_unsub_end(ont_device_t* device)
{
    return ont_pkt_formatter_finish(device->formatter);
}

int ont_serialize_mqtt_connect_packet(ont_device_t* device,const char *auth_info)
{
    mqtt_connect_packet packet;
    mqtt_connect_flag mFlag;

    char client_id_str[32] = {'\0'}; 
    char usr_name_str[32] = {'\0'}; 

    if(NULL == device )
	return ONT_ERR_BADPARAM;
    mFlag.connect_flag_ = 0;
    SetUserFlag(&mFlag,1);
    SetPasswordFlag(&mFlag,1);
    SetWillRetainFlag(&mFlag,0);
    SetWillQosFlag(&mFlag,0);
    SetWillFlag(&mFlag,0);
    SetCleanSessionFlag(&mFlag,1);
    SetReserveFlag(&mFlag,0);


    ont_parser_init_mqtt_connect_packet(&packet);

    packet.connect_flag_.value = mFlag.connect_flag_;
    packet.protocol_name_.len = strlen(MQTT_PROTOCOL_NAME);
    packet.protocol_name_.value = MQTT_PROTOCOL_NAME;
    packet.protocol_level_.value = MQTT_PROTOCOL_LEVEL;
    packet.keep_alive.value = device->keepalive;

    ont_platform_snprintf(client_id_str,sizeof(client_id_str),"%u",device->device_id);
    packet.client_id_.len = strlen(client_id_str);
    packet.client_id_.value = client_id_str;

    ont_platform_snprintf(usr_name_str,sizeof(usr_name_str),"%u",device->product_id);
    packet.usr_name_.len = strlen(usr_name_str);
    packet.usr_name_.value = usr_name_str;

    if (device->key){
	packet.password_.len = strlen(device->key);
	packet.password_.value = device->key;
    }
    else{
	packet.password_.len = strlen(auth_info);
	packet.password_.value = auth_info;
    }

    return ont_pkt_serialize_finish(device->formatter, CMD_CONNECT << 4, &packet.head);;
}
int ont_serialize_mqtt_publish_packet(ont_device_t* device,const char *topic_name,
	const char *content,size_t bytes,uint8_t qos_level)
{

    /*
       mqtt_publish_packet_1 <----------> qos_level = 1,带有packet_identifier
       mqtt_publish_packet_0 <----------> qos_level = 0,不带packet_identifier
       */
    uint8_t packet_tag = CMD_PUBLISH << 4;
    qos_info qos;

    ont_mqtt_device_t *mqtt_instance = NULL;
    if(NULL == device || NULL == topic_name 
	    || NULL == content
	    || 0 == bytes){
	return ONT_ERR_BADPARAM;
    }

    qos.qos_tag_ = 0;
    qos.packetid_ = 0;
    qos.qos_info_int32_ = 0;
    set_qos_level(&qos, qos_level);

    mqtt_instance = DEV2MQTT(device);

    if(qos_level == 1){
	mqtt_publish_packet_1 packet;
	ont_parser_init_mqtt_publish_packet_1(&packet);
	packet.topic_name.len = strlen(topic_name);
	packet.topic_name.value = topic_name;

	if(mqtt_instance->publish_pkd_id_count == 0){
	    mqtt_instance->publish_pkd_id_count++;
	}
	packet.packet_identifier_.value = mqtt_instance->publish_pkd_id_count++;

	packet.payload_.len = bytes;
	packet.payload_.value = content;	
	return ont_pkt_serialize_finish(device->formatter, packet_tag |= qos.qos_tag_, &packet.head);
    }
    else if(qos_level == 0){
	mqtt_publish_packet_0 packet;
	ont_parser_init_mqtt_publish_packet_0(&packet);
	packet.topic_name.len = strlen(topic_name);
	packet.topic_name.value = topic_name;

	packet.payload_.len = bytes;
	packet.payload_.value = content;
	return ont_pkt_serialize_finish(device->formatter, packet_tag |= qos.qos_tag_, &packet.head);
    }
    else{
	return ONT_ERR_BADPARAM;
    }

    return ONT_ERR_OK;
}
int ont_serialize_mqtt_send_dp_packet(ont_device_t* device,const char *topic_name,
	const char *content,size_t bytes)
{
    /*
       mqtt_send_dp_packet_1 <----------> qos_level = 1,带有packet_identifier
       mqtt_send_dp_packet_0 <----------> qos_level = 0,不带packet_identifier
       */
    uint8_t packet_tag = CMD_PUBLISH << 4;
    qos_info qos;
    uint8_t qos_level = 1;

    mqtt_send_dp_packet_1 packet;

    ont_mqtt_device_t *mqtt_instance = NULL;
    if(NULL == device || NULL == topic_name 
	    || NULL == content
	    || 0 == bytes){
	return ONT_ERR_BADPARAM;	
    }

    qos.qos_tag_ = 0;
    qos.packetid_ = 0;
    qos.qos_info_int32_ = 0;
    set_qos_level(&qos, qos_level);

    mqtt_instance = DEV2MQTT(device);

    ont_parser_init_mqtt_send_dp_packet_1(&packet);
    packet.topic_name.len = strlen(topic_name);
    packet.topic_name.value = topic_name;

    if(mqtt_instance->publish_pkd_id_count == 0){
	mqtt_instance->publish_pkd_id_count = 1;
    }
    packet.packet_identifier_.value = mqtt_instance->publish_pkd_id_count++;

    /*
byte0:data_type(uint8_t)
byte1~2:date_length(uint16_t)
*/
    packet.data_type.value = 0x08;
    packet.payload_.len = bytes;
    packet.payload_.value = content;	
    return ont_pkt_serialize_finish(mqtt_instance->mqtt_formatter, packet_tag |= qos.qos_tag_, &packet.head);										
}
int ont_serialize_mqtt_subscribe_packet(ont_device_t* device,const char **topic_arry,size_t size)
{
    int err = 0;
    size_t i = 0;

    ont_mqtt_device_t *mqtt_instance = NULL;
    if(NULL == device || NULL == topic_arry || 0 == size){
	return ONT_ERR_BADPARAM;
    }

    mqtt_instance = DEV2MQTT(device);

    err = ont_mqtt_subscribe_begin(device,CMD_SUBSCRIBE << 4);
    CHECK_ONT_ERR_OK_RETURN(ONT_ERR_OK,err)

	if(mqtt_instance->subscribe_pkt_id_count == 0){
	    mqtt_instance->subscribe_pkt_id_count = 1;
	}
    err = ont_mqtt_subscribe_packet_id(device, mqtt_instance->subscribe_pkt_id_count++);
    CHECK_ONT_ERR_OK_RETURN(ONT_ERR_OK,err)

	for(;i<size;++i)
	{
	    ont_mqtt_subscribe_add_topic(device, topic_arry[i]);
	}
    return ont_mqtt_subscribe_end(device);
}
int ont_serialize_mqtt_unsub_packet(ont_device_t* device,const char **topic_arry,size_t size)
{
    size_t i = 0;
    int err = 0;

    ont_mqtt_device_t *mqtt_instance = NULL;
    if(NULL == device || NULL == topic_arry || 0 == size){
	return ONT_ERR_BADPARAM;
    }

    mqtt_instance = DEV2MQTT(device);

    err = ont_mqtt_unsub_begin(device,CMD_UNSUBSCRIBE << 4);
    CHECK_ONT_ERR_OK_RETURN(ONT_ERR_OK,err)

	if(mqtt_instance->unsub_pkt_id_count == 0){
	    mqtt_instance->unsub_pkt_id_count = 1;
	}
    err = ont_mqtt_unsub_packet_id(device, mqtt_instance->unsub_pkt_id_count++);
    CHECK_ONT_ERR_OK_RETURN(ONT_ERR_OK,err)

	for(; i < size; ++i)
	{
	    ont_mqtt_unsub_add_topic(device, topic_arry[i]);
	}
    return ont_mqtt_unsub_end(device);
}

int ont_serialize_mqtt_publish_ack(ont_device_t *device, uint16_t packet_id)
{
    mqtt_publish_ack packet;

    if(NULL == device || 0 == packet_id){
	return ONT_ERR_BADPARAM;
    }

    ont_parser_init_mqtt_publish_ack(&packet);
    packet.packet_identifier_.value = packet_id;

    return ont_pkt_serialize_finish(device->formatter, CMD_PUBACK << 4, &packet.head);
}
