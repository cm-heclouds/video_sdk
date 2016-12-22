#ifndef ONT_PROTOCOLS_MQTT_ONT_MQTT_PRIVATE_H_
#define ONT_PROTOCOLS_MQTT_ONT_MQTT_PRIVATE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ont_list.h"
#include "ont_channel.h"
#include "ont_tcp_channel.h"
#include "ont/device.h"
#include "parser/parser.h"
#include "parser/ont_pkt_formatter.h"

#define ALL_TOPIC_SUB_SUCC 0x00
#define NOT_ALL_TOPIC_SUB_SUCC -1

#define CMD_RESERVERD 	0x15
#define CMD_CONNECT 	0x01
#define CMD_CONNACK 	0x02
#define CMD_PUBLISH 	0x03
#define CMD_PUBACK 		0x04
#define CMD_SUBSCRIBE 	0x08
#define CMD_SUBACK 		0x09
#define CMD_UNSUBSCRIBE 0x0A
#define CMD_UNSUBACK 	0x0B
#define CMD_KEEPALIVE 		0x0C
#define CMD_KEEPALIVERSP 	0x0D
#define CMD_DISCONNECT 	0x0E

#define CONNECT_PACKET 		 0x01
#define CONNECT_ACK_PACKET   0x02

#define PUBLISH_PACKET 		 0x03
#define PUBLISH_ACK_PACKET   0x04
	  
#define SUBSCRIBE_PACKET 	 0x08
#define SUBSCRIBE_ACK_PACKET 0x09
	  
#define UNSUB_PACKET 		 0x0A
#define UNSUB_ACK_PACKET 	 0x0B
	  
#define KEEPALIVE_PACKET 	 0x0C
#define KEEPALIVE_RSP_PACKET 0x0D

#define RESERVERD_PACKET 	 0x0F

#define SYS_CMD_REQ_PLATFORM_CMD "$creq"
#define SYS_CMD_RSP_PLATFORM_CMD "$crsp"
#define SYS_CMD_UPLOAD_DATA_POINT "$dp"

#define MAX_BASIC_HEADER_LENGTH  5
#define MAX_MESSAGE_LENGTH  0x0FFFFFFF
#define MAX_MESSAGE_TYPE_LENGTH 0x0F
#define FIRST_BIT 0x80
#define MAX_FIX_HEADER_LENGTH 5

#define MQTT_PROTOCOL_NAME "MQTT"
#define MQTT_PROTOCOL_LEVEL 4

#define COMPLETE_PACKET 0x01
#define NOT_COMPLETE_PACKET 0x02

#define DESERIALIZE_PACKET_ERR -32
#define CHECK_ONT_ERR_OK_RETURN(ONT_ERR_TYPE,err) \
         if(ONT_ERR_TYPE != err) return err;
	 
/* define mqtt_connect result*/
#define CONNACK_OK                   0x00
/*
#define CONNACK_ERR_PROTOCOL         0x01
#define CONNACK_ERR_INVALID_DEVICE   0x02
#define CONNACK_ERR_SYSTEM           0x03
#define CONNACK_ERR_USERNAME_OR_PWD  0x04
#define CONNACK_ERR_INVALID_CONNECT  0x05
*/

#define MQTT2DEV(obj) (&(obj->dev))
#define DEV2MQTT(obj) ((ont_mqtt_device_t*)(obj))

/*
 *	define mqtt_connect_packet
 */
ont_parser_def_packet_begin(mqtt_connect_packet)
ont_parser_add_elem(protocol_name_, bytes)
ont_parser_add_elem(protocol_level_, uint8)
ont_parser_add_elem(connect_flag_, uint8)
ont_parser_add_elem(keep_alive, uint16)
ont_parser_add_elem(client_id_, bytes)
ont_parser_add_elem(usr_name_, bytes)
ont_parser_add_elem(password_, bytes)
ont_parser_def_packet_end(mqtt_connect_packet)
/*
 *	define mqtt_connect_ack
 */
ont_parser_def_packet_begin(mqtt_connect_ack)
ont_parser_add_elem(ack_flags_, uint8)
ont_parser_add_elem(ret_code_, uint8)
ont_parser_def_packet_end(mqtt_connect_ack)

/*
 *	define mqtt_keepalive_packet
 */
typedef struct mqtt_keepalive_packet
{
	uint8_t packet_tag_;
	uint8_t remain_length_;
}mqtt_keepalive_packet;

/*
 *	define mqtt_keepalive_rsp
 */
typedef struct mqtt_keepalive_rsp
{
	uint8_t packet_tag_;
	uint8_t remain_length_;
}mqtt_keepalive_rsp;

/*
 *	define mqtt_publish_packet with pkt_id
 */
ont_parser_def_packet_begin(mqtt_publish_packet_1)
ont_parser_add_elem(topic_name, bytes)
ont_parser_add_elem(packet_identifier_, uint16)
ont_parser_add_elem(payload_, raw)
ont_parser_def_packet_end(mqtt_publish_packet_1)
/*
 *	define mqtt_publish_packet without pkt_id
 */
ont_parser_def_packet_begin(mqtt_publish_packet_0)
ont_parser_add_elem(topic_name, bytes)
ont_parser_add_elem(payload_, raw)
ont_parser_def_packet_end(mqtt_publish_packet_0)
/*
 *	define mqtt_send_dp_packet with pkt_id
 */
ont_parser_def_packet_begin(mqtt_send_dp_packet_1)
ont_parser_add_elem(topic_name, bytes)
ont_parser_add_elem(packet_identifier_, uint16)
ont_parser_add_elem(data_type,uint8)
ont_parser_add_elem(payload_, bytes)
ont_parser_def_packet_end(mqtt_send_dp_packet_1)

/*
 *	define mqtt_publish_ack
 */
ont_parser_def_packet_begin(mqtt_publish_ack)
ont_parser_add_elem(packet_identifier_, uint16)
ont_parser_def_packet_end(mqtt_publish_ack)


/*
 *	define mqtt_sub_topic_packet
 */
ont_parser_def_packet_begin(mqtt_subscribe_packet)
ont_parser_add_elem(topic, bytes)
ont_parser_add_elem(qos, uint8)
ont_parser_def_packet_end(mqtt_subscribe_packet)

/*
 *	define mqtt_sub_ack
 */
ont_parser_def_packet_begin(mqtt_subscribe_ack)
ont_parser_add_elem(packet_identifier_, uint16)
ont_parser_add_elem(Results, raw)
ont_parser_def_packet_end(mqtt_subscribe_ack)

/*
 *	define mqtt_unsub_packet
 */
ont_parser_def_packet_begin(mqtt_unsub_packet)
ont_parser_add_elem(topic, bytes)
ont_parser_def_packet_end(mqtt_unsub_packet)

/*
 *	define mqtt_unsub_topic_ack
 */
ont_parser_def_packet_begin(mqtt_unsub_ack)
ont_parser_add_elem(packet_identifier_, uint16)
ont_parser_def_packet_end(mqtt_unsub_ack)

/*
 *	define mqtt_disconn_packet
 */
typedef struct mqtt_disconn_packet
{
	uint8_t packet_tag_;
	uint8_t remain_length_;
}mqtt_disconn_packet;

enum mqtt_read_packet_status
{
	MQTT_READ_START = 1,
	MQTT_READ_COMPLETE = 2,
	MQTT_READ_CONTINUE = 3
};

enum mqtt_device_conn_status
{
	MQTT_CONNECTED = 1,
	MQTT_DISCONNECTED = 2
};

/*define ont_mqtt_device_t for mqtt usage only*/
typedef struct ont_mqtt_device_t
{
	ont_device_t dev;
	ont_channel_interface_t channel;
    
	ont_list_t *cmd_list;
	uint16_t publish_pkd_id_count;
	uint16_t subscribe_pkt_id_count;
	uint16_t unsub_pkt_id_count;
	
	ont_pkt_formatter_t *mqtt_formatter;
	int32_t access_time;
	uint16_t mqtt_conn_keepalive;
	int pkt_read_status;
	int conn_status;
	int mqtt_oper_result;
}ont_mqtt_device_t;

/* declaration of packet serialize func */

int ont_mqtt_subscribe_begin(ont_device_t* device,uint8_t pkt_tag);

int ont_mqtt_subscribe_packet_id(ont_device_t* device, uint16_t pkt_id);

int ont_mqtt_subscribe_add_topic(ont_device_t* device,const char* topic);

int ont_mqtt_subscribe_end(ont_device_t* device);

int ont_mqtt_unsub_begin(ont_device_t* device,uint8_t pkt_tag);

int ont_mqtt_unsub_packet_id(ont_device_t* device, uint16_t pkt_id);

int ont_mqtt_unsub_add_topic(ont_device_t* device,const char* topic);

int ont_mqtt_unsub_end(ont_device_t* device);

/*serialize packets of mqtt*/
int ont_serialize_mqtt_connect_packet(ont_device_t* device,const char *auth_info);

int ont_serialize_mqtt_publish_packet(ont_device_t* device,const char *topic_name,
								       const char *content,size_t bytes,uint8_t qos_level);

int ont_serialize_mqtt_send_dp_packet(ont_device_t* device,const char *topic_name,
								       const char *content,size_t bytes);
int ont_serialize_mqtt_keepalive_packet(ont_device_t* device);

int ont_serialize_mqtt_subscribe_packet(ont_device_t* device,const char **topic_arry,size_t size);

int ont_serialize_mqtt_unsub_packet(ont_device_t* device,const char **topic_arry,size_t size);



/* declaration of packet init func */
int ont_parser_init_mqtt_connect_packet(mqtt_connect_packet* pkt);

int ont_parser_init_mqtt_connect_ack(mqtt_connect_ack* pkt);

int ont_parser_init_mqtt_publish_packet_1(mqtt_publish_packet_1* pkt);

int ont_parser_init_mqtt_publish_packet_0(mqtt_publish_packet_0* pkt);

int ont_parser_init_mqtt_publish_ack(mqtt_publish_ack* pkt);

int ont_parser_init_mqtt_subscribe_packet(mqtt_subscribe_packet* pkt);

int ont_parser_init_mqtt_subscribe_ack(mqtt_subscribe_ack* pkt);

int ont_parser_init_mqtt_unsub_packet(mqtt_unsub_packet* pkt);

int ont_parser_init_mqtt_unsub_ack(mqtt_unsub_ack* pkt);

# ifdef __cplusplus
}      /* extern "C" */
# endif
#endif