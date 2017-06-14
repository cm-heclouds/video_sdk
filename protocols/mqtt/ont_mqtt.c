#include <string.h>
#include "ont_mqtt_private.h"
#include "ont_mqtt.h"
#include "ont/platform.h"
#include "ont/error.h"
#include "ont/log.h"

int update_access_time(ont_device_t *device);

int ont_mqtt_read_packet_cb(void *ctx,const char *buf,size_t buf_size,size_t *read_size);

int ont_is_complete_packet(const char *buf,
							   size_t buf_size,
							   size_t *remain_length,
							   size_t *fix_header_bytes);

int ont_handle_normal_publish_packet(ont_device_t *device,mqtt_publish_packet_0 *packet);

int ont_handle_cmd_publish_packet(ont_device_t *device,mqtt_publish_packet_0 *packet);

int ont_handle_mqtt_publish_packet(ont_device_t *device,
									   const char *buf,
									   size_t *remain_length,
									   size_t *read_size,
									   size_t *packet_read_size);

int ont_handle_mqtt_publish_ack(ont_device_t *device,
									const char *buf,
									size_t *remain_length,
									size_t *read_size,
									size_t *packet_read_size);

int ont_handle_mqtt_connect_ack(ont_device_t *device,
									const char *buf,
									size_t *remain_length,
									size_t *read_size,
									size_t *packet_read_size);

int ont_handle_mqtt_subscribe_ack(ont_device_t *device,
									  const char *buf,
									  size_t *remain_length,
									  size_t *read_size,
									  size_t *packet_read_size);

int ont_handle_mqtt_unsub_ack(ont_device_t *device,
								  const char *buf,
								  size_t *remain_length,
								  size_t *read_size,
								  size_t *packet_read_size);

int ont_handle_mqtt_keepalive_rsp(ont_device_t*device,
									  const char *buf,
									  size_t *remain_length,
									  size_t *read_size,
									  size_t *packet_read_size);
									  
int ont_free_device_cmd_list(ont_device_t *device,ont_list_t *cmd_list);

int decode_variable_length(const char *buf,size_t* value,size_t*read_size);

int ont_handle_process_err(ont_device_t *device,int err);

/*
	处理process函数返回值
*/
int ont_handle_process_err(ont_device_t *device,int err)
{
	ont_mqtt_device_t *mqtt_instance = DEV2MQTT(device);
	if(ONT_ERR_DISCONNECTED_BY_USER == err)
	{
		ONT_LOG_DEBUG0("disconnect mqtt_device by user!");
		ont_mqtt_disconnect(device);
		mqtt_instance->conn_status = MQTT_DISCONNECTED;
	}
	if(ONT_ERR_OK != err && ONT_ERR_DISCONNECTED_BY_USER != err)
	{
		mqtt_instance->conn_status = MQTT_DISCONNECTED;
	}
	return err;
}
int update_access_time(ont_device_t *device)
{
	ont_mqtt_device_t *mqtt_instance = DEV2MQTT(device);
	
	if(NULL == device)
		return ONT_ERR_BADPARAM;

	mqtt_instance->access_time = ont_platform_time();
	
	return ONT_ERR_OK;
}
int decode_variable_length(const char *buf,size_t* value,size_t*read_size)
{
	uint32_t lenghtMultiplier_ = 1;
	const char *tmp = NULL;
	unsigned char  tmp_char;
	int max_length = 0;
	
	if(NULL == buf )
		return ONT_ERR_BADPARAM;
	
	tmp = buf;
	tmp_char = *(tmp + (*read_size));
	++(*read_size);
	max_length = MAX_BASIC_HEADER_LENGTH - 1;
	*value = 0;
	while (max_length > 0)
	{
		*value += ((uint8_t)tmp_char & ~FIRST_BIT) * lenghtMultiplier_;
		lenghtMultiplier_ *= FIRST_BIT;

		if (((uint8_t)tmp_char & FIRST_BIT) == 0)
		{
			break;
		}
		tmp_char = *(tmp + (*read_size));
		++(*read_size);
		max_length--;
	}
	return 0;
}

int ont_mqtt_create(ont_device_t **device)
{
	ont_mqtt_device_t *mqtt_instance = NULL;
	if(NULL == device)
	{
		return ONT_ERR_BADPARAM;
	}
	mqtt_instance = (ont_mqtt_device_t *)ont_platform_malloc(sizeof(ont_mqtt_device_t));
	if(NULL == mqtt_instance)
	{
		return ONT_ERR_NOMEM;
	}
    memset(mqtt_instance, 0x00, sizeof(ont_mqtt_device_t));
    
	mqtt_instance->mqtt_formatter = (ont_pkt_formatter_t*)ont_platform_malloc(sizeof(ont_pkt_formatter_t));
	if(NULL == mqtt_instance->mqtt_formatter)
	{
		ont_platform_free(mqtt_instance);
		mqtt_instance = NULL;
		return ONT_ERR_NOMEM;
	}
	ont_pkt_formatter_init(mqtt_instance->mqtt_formatter);
	
	mqtt_instance->cmd_list = ont_list_create();
	if (NULL == mqtt_instance->cmd_list)
	{	
		ont_pkt_formatter_destroy(mqtt_instance->mqtt_formatter);
		ont_platform_free(mqtt_instance->mqtt_formatter);
		mqtt_instance->mqtt_formatter = NULL;
		
		ont_platform_free(mqtt_instance);
		mqtt_instance = NULL;
		
		return ONT_ERR_NOMEM;
	}
		
	mqtt_instance->subscribe_pkt_id_count = 1;
	mqtt_instance->publish_pkd_id_count = 1;
	mqtt_instance->unsub_pkt_id_count = 1;
	mqtt_instance->access_time = 0;
	mqtt_instance->mqtt_conn_keepalive = 0;
	mqtt_instance->pkt_read_status = MQTT_READ_START;
	mqtt_instance->mqtt_oper_result = 0;
	mqtt_instance->conn_status = MQTT_DISCONNECTED;
	*device = MQTT2DEV(mqtt_instance);

	return ONT_ERR_OK;
}
int ont_free_device_cmd_list(ont_device_t *device,ont_list_t *cmd_list)
{
	ont_mqtt_device_t *mqtt_instance = NULL;
	ont_device_cmd_t *cmd_t = NULL;
	if(NULL == device || NULL == cmd_list)
		return ONT_ERR_BADPARAM;
	
	mqtt_instance = DEV2MQTT(device);
	while(ont_list_size(mqtt_instance->cmd_list) > 0)
	{
		ont_list_pop_front(mqtt_instance->cmd_list,(void **)&cmd_t);
		if(NULL != cmd_t)
		{
			ont_platform_free(cmd_t->req);
			cmd_t->req = NULL;
			ont_platform_free(cmd_t);
			cmd_t = NULL;
		}
	}
	ont_list_destroy(mqtt_instance->cmd_list);
	return ONT_ERR_OK;
}
void ont_mqtt_destroy(ont_device_t *device)
{
    if(NULL != device)
    {
		
		ont_mqtt_device_t *mqtt_instance = DEV2MQTT(device);
		if(MQTT_CONNECTED == mqtt_instance->conn_status)
		{
			ont_mqtt_disconnect(device);
		}
		if (mqtt_instance->channel.channel)
		{
		    mqtt_instance->channel.fn_deinitilize(mqtt_instance->channel.channel);
		}

		ont_pkt_formatter_destroy(mqtt_instance->mqtt_formatter);
		ont_platform_free(mqtt_instance->mqtt_formatter);
		mqtt_instance->mqtt_formatter = NULL;
		
		ont_free_device_cmd_list(device,mqtt_instance->cmd_list);
		
        ont_platform_free(mqtt_instance);
		mqtt_instance = NULL;
    }
}
void ont_mqtt_disconnect(ont_device_t *device)
{
	int err = 0;
	mqtt_disconn_packet packet;
	char send_buf[2]={'\0'};
	ont_mqtt_device_t *mqtt_instance = NULL;
	
	if(NULL == device)
		return ;

	packet.packet_tag_ = CMD_DISCONNECT << 4;
	packet.remain_length_ = 0;
	
	send_buf[0] = packet.packet_tag_;
	send_buf[1] = packet.remain_length_;
	
	mqtt_instance = DEV2MQTT(device);
	
	err = mqtt_instance->channel.fn_write(mqtt_instance->channel.channel
												, send_buf,sizeof(send_buf));
	if(ONT_ERR_OK != err)
		return;
	do
	{
		err = mqtt_instance->channel.fn_process(mqtt_instance->channel.channel);
	} while (0);
}
int ont_mqtt_connect(ont_device_t *device,const char *auth_info)
{
	int err = 0;
	ont_mqtt_device_t *mqtt_instance = NULL;
	
	if(NULL == device)
		return ONT_ERR_BADPARAM;
	
	mqtt_instance = DEV2MQTT(device);
	
	ont_pkt_formatter_reset(device->formatter,0);
	err = ont_serialize_mqtt_connect_packet(device, auth_info);
    CHECK_ONT_ERR_OK_RETURN(ONT_ERR_OK, err)
	
	mqtt_instance->mqtt_conn_keepalive = device->keepalive;
	
	err = ont_tcp_channel_create(&(mqtt_instance->channel), device->ip, 
								   device->port, ONT_SOCK_RECV_BUF_SIZE,
								   ONT_SOCK_SEND_BUF_SIZE, device, device->keepalive);
	CHECK_ONT_ERR_OK_RETURN(ONT_ERR_OK, err)

	
	err = mqtt_instance->channel.fn_initilize(mqtt_instance->channel.channel,
											 ont_mqtt_read_packet_cb);										 
	CHECK_ONT_ERR_OK_RETURN(ONT_ERR_OK, err)
	
	err = mqtt_instance->channel.fn_connect(mqtt_instance->channel.channel,&(device->exit));
	CHECK_ONT_ERR_OK_RETURN(ONT_ERR_OK, err)
	
	ONT_LOG_DEBUG2("Connect to the server '%s:%u' successfully.",device->ip, device->port);

	err = mqtt_instance->channel.fn_write(mqtt_instance->channel.channel,
										  device->formatter->result.data,
										  device->formatter->result.len);
	CHECK_ONT_ERR_OK_RETURN(ONT_ERR_OK, err)
	
	mqtt_instance->pkt_read_status = MQTT_READ_START;
	do
	{
		err = mqtt_instance->channel.fn_process(mqtt_instance->channel.channel);
		
		if(ONT_ERR_OK != ont_handle_process_err(device,err))
			return err;
		
		ont_platform_sleep(30);
	} while (mqtt_instance->pkt_read_status != MQTT_READ_COMPLETE);

	
	if(ONT_ERR_OK == mqtt_instance->mqtt_oper_result)
	{
		update_access_time(device);
		mqtt_instance->conn_status = MQTT_CONNECTED;
	}
	ont_pkt_formatter_reset(device->formatter,0);
	
	return mqtt_instance->mqtt_oper_result;
}

int ont_is_complete_packet(const char *buf,
							   size_t buf_size,
							   size_t *remain_length,
							   size_t *fix_header_bytes)
{
	const char *tmp = buf;
	/*
		只有packet的第一个packet_tag字段(packet的长度至少为2)
	*/
	if(buf_size <= 1)
		return NOT_COMPLETE_PACKET;
	
	/*
		*remain_length:记录packet的剩余长度
		*fix_header_bytes:记录packet的packet_tag和remain_length占用的字节数
	*/
	decode_variable_length(tmp+sizeof(uint8_t),remain_length,fix_header_bytes); 
	
	/*
		buf_size的长度至少要大于等于fixHeader占用的字节数+packet的剩余长度
	*/
	if(buf_size >= *remain_length + sizeof(uint8_t) + *fix_header_bytes)
	{
		/*
			++操作是为了加上packet的fixHeader的tag字段长度
		*/
		++(*fix_header_bytes); 
		return COMPLETE_PACKET;
	}
	else
	{
		ONT_LOG2(ONTLL_ERROR,"buf does not contain one complete packet,buf %d, remain %d \n", buf_size, *remain_length);
	}
	return NOT_COMPLETE_PACKET;
}

int ont_handle_normal_publish_packet(ont_device_t *device, mqtt_publish_packet_0 *packet)
{
	/*
		处理用户的topic(非平台下发的指令)
	*/
	size_t len = 0;
	char *topic_name = NULL;
	ONT_LOG_DEBUG1("%s begin!",__FUNCTION__);
	if(NULL == device || NULL == packet 
					  || NULL == packet->topic_name.value)
		return ONT_ERR_BADPARAM;
	len = packet->topic_name.len + 1;
	topic_name = (char *)ont_platform_malloc(len);
	if(NULL == topic_name){
		return ONT_ERR_NOMEM;
	}
	memcpy(topic_name,packet->topic_name.value,packet->topic_name.len);
	topic_name[len - 1] = '\0'; 
	ONT_LOG_DEBUG2("recv publish packet topic = %s, payload_len = %u\n",topic_name,packet->payload_.len);
	
	ont_platform_free(topic_name);
	return ONT_ERR_OK;
}
int ont_handle_cmd_publish_packet(ont_device_t *device,mqtt_publish_packet_0 *packet)
{
	/*
		只用于处于平台下发的命令: $creq/c87164fe-ce20-56b5-870d-94cbee139f8b
	*/
	ont_mqtt_device_t *mqtt_instance = NULL;
	ont_list_t *cmd_list_ = NULL;
	ont_device_cmd_t * cmd = NULL;

	ONT_LOG_DEBUG1("%s begin!",__FUNCTION__);
	if(NULL == device || NULL == packet)
		return ONT_ERR_BADPARAM;
	
	mqtt_instance = DEV2MQTT(device);

		cmd_list_ = mqtt_instance->cmd_list;

	cmd = (ont_device_cmd_t *)ont_platform_malloc(sizeof(ont_device_cmd_t));
	if (NULL == cmd)
		return ONT_ERR_NOMEM;
	
	/*
		将平台下发的cmdid存入ont_device_cmd_t->id中,
		例如c87164fe-ce20-56b5-870d-94cbee139f8b
		6 : strlen("$creq/")
	*/
	memset(cmd->id, 0x00, sizeof(cmd->id));
	memcpy(cmd->id, packet->topic_name.value + 6,
			packet->topic_name.len - 6);

	/*
		将平台下发的cmd的具体内容存入ont_device_cmd_t->req中
	*/
	cmd->req = (char *)ont_platform_malloc(packet->payload_.len + 1);
	
	if (NULL == cmd->req)
	{
		ont_platform_free(cmd);
		cmd = NULL;
		return ONT_ERR_NOMEM;
	}
		
	memset(cmd->req, 0x00, packet->payload_.len + 1);
	
	memcpy(cmd->req, packet->payload_.value,packet->payload_.len);
	cmd->size = packet->payload_.len;

	/*
		将平台下发的cmd插入到cmd_list中
	*/
	if(NULL == ont_list_insert(cmd_list_, cmd))
	{
		ONT_LOG1(ONTLL_ERROR,"Insert ont_device_cmd_t(%s) into cmd_list failed \n",cmd->id);
		ont_platform_free(cmd->req);
		cmd->req = NULL;
		ont_platform_free(cmd);
		cmd = NULL;
		return ONT_ERR_BADPARAM;
	}
	return ONT_ERR_OK;
}
int ont_handle_mqtt_publish_packet(ont_device_t *device,
									   const char *buf,
									   size_t *remain_length,
									   size_t *fix_header_bytes,
									   size_t *packet_read_size)
{
	/*
		该函数用于处理接收到的publish包，该包无packet_identifier字段，
		因此用mqtt_publish_packet_0类型
	*/
	mqtt_publish_packet_0 packet;
	uint16_t topic_len = 0;
	/*
		platform_cmd_str store "$creq"
	*/
	char platform_cmd_str[6] = {'\0'};
	
	ONT_LOG_DEBUG1("%s begin!\n",__FUNCTION__);
	if(NULL == device || NULL == buf 
					  || NULL == remain_length
					  || NULL == fix_header_bytes
					  || NULL == packet_read_size)
		return ONT_ERR_BADPARAM;

	ont_parser_init_mqtt_publish_packet_0(&packet);
	
	/*
		计算topic的长度，topic_len 为uint16类型 
	*/
	topic_len = (*(buf+(*fix_header_bytes)) << 8) | *(buf+(*fix_header_bytes)+1);
	/*
		payload 的长度为remain_length的长度-topic_len字段(uint16_t) - topic_name的长度
	*/
	packet.payload_.len = *remain_length - topic_len - sizeof(uint16_t);
	
	*packet_read_size = *fix_header_bytes + *remain_length;
	if((ont_parser_deserialize(buf+(*fix_header_bytes), *remain_length, &packet.head, 1)) != 0)
	{ 
		ONT_LOG0(ONTLL_ERROR,"deserialize mqtt_publish_packet failed\n");
		return DESERIALIZE_PACKET_ERR;
	}

	memcpy(platform_cmd_str,packet.topic_name.value,5);
	/*
		判断是否为平台下发的命令包，
		SYS_CMD_REQ_PLATFORM_CMD = "$creq"
	*/
	if(strcmp(platform_cmd_str,SYS_CMD_REQ_PLATFORM_CMD) == 0 
							&& packet.topic_name.value[5] == '/')
	{
		ont_handle_cmd_publish_packet(device,&packet);
	}
	else
	{
		ont_handle_normal_publish_packet(device,&packet);
	}
	
	ont_parser_destroy(&packet.head);
	return ONT_ERR_OK;
}

int ont_handle_mqtt_publish_ack(ont_device_t *device,
									const char *buf,
									size_t *remain_length,
									size_t *fix_header_bytes,
									size_t *packet_read_size)
{
	mqtt_publish_ack packet;
	
	ONT_LOG_DEBUG1("%s begin!",__FUNCTION__);
	if(NULL == device || NULL == buf 
					  || NULL == remain_length
					  || NULL == fix_header_bytes
					  || NULL == packet_read_size)
		return ONT_ERR_BADPARAM;
	
	*packet_read_size = *fix_header_bytes + *remain_length;
	ont_parser_init_mqtt_publish_ack(&packet);
	if((ont_parser_deserialize(buf+(*fix_header_bytes), *remain_length, &packet.head, 0)) != 0)
	{
		ONT_LOG0(ONTLL_ERROR,"deserialize mqtt_publish_ack failed\n");
		return DESERIALIZE_PACKET_ERR;
	}
	ONT_LOG_DEBUG2("%s: packet_identifier_ = %d\n",__FUNCTION__,packet.packet_identifier_.value);

	return ONT_ERR_OK;
}
int ont_handle_mqtt_connect_ack(ont_device_t *device,
									const char *buf,
									size_t *remain_length,
									size_t *fix_header_bytes,
									size_t *packet_read_size)
{
	mqtt_connect_ack packet;
	
	ONT_LOG_DEBUG1("%s begin!",__FUNCTION__);
	if(NULL == device || NULL == buf 
					  || NULL == remain_length
					  || NULL == fix_header_bytes
					  || NULL == packet_read_size)
		return ONT_ERR_BADPARAM;

	*packet_read_size = *fix_header_bytes + *remain_length;
	ont_parser_init_mqtt_connect_ack(&packet);
	if((ont_parser_deserialize(buf+(*fix_header_bytes), *remain_length, &packet.head, 0)) != 0)
	{
		ONT_LOG0(ONTLL_ERROR,"deserialize mqtt_connect_ack failed\n");
		return DESERIALIZE_PACKET_ERR;
	}	
	
	if(packet.ret_code_.value == CONNACK_OK)
	{
		ONT_LOG_DEBUG0("connect server succ!");
		return ONT_ERR_OK;
	}
	else
	{
		ONT_LOG_DEBUG1("connect server failed! ret_code = %d\n",packet.ret_code_.value);
		return ONT_ERR_FAILED_TO_AUTH;
	}
	return ONT_ERR_OK;
}

int ont_handle_mqtt_subscribe_ack(ont_device_t *device,
									  const char *buf,
									  size_t *remain_length,
									  size_t *fix_header_bytes,
									  size_t *packet_read_size)
{
	mqtt_subscribe_ack packet;
	int sub_succ_all = 0;
	int i = 0;
	
	ONT_LOG_DEBUG1("%s begin!",__FUNCTION__);
	if(NULL == device || NULL == buf 
					  || NULL == remain_length
					  || NULL == fix_header_bytes
					  || NULL == packet_read_size)
		return ONT_ERR_BADPARAM;

	*packet_read_size = *fix_header_bytes + *remain_length;
	ont_parser_init_mqtt_subscribe_ack(&packet);
	packet.Results.len = *remain_length - sizeof(uint16_t);
	if((ont_parser_deserialize(buf+(*fix_header_bytes), *remain_length, &packet.head, 1)) != 0)
	{
		ONT_LOG0(ONTLL_ERROR,"deserialize mqtt_subscribe_ack failed\n");
		return DESERIALIZE_PACKET_ERR;
	}
	for(;i<packet.Results.len;++i)
	{
		if(0 != packet.Results.value[i])
		{
			sub_succ_all = 1;
			break;
		}
	}
	if(0 == sub_succ_all)
	{
		ont_parser_destroy(&packet.head);
		ONT_LOG_DEBUG0("all topics subscribe succ!\n");
		return ALL_TOPIC_SUB_SUCC;
	}
	ont_parser_destroy(&packet.head);
	return NOT_ALL_TOPIC_SUB_SUCC;
}
int ont_handle_mqtt_unsub_ack(ont_device_t *device, 
								  const char *buf,
								  size_t *remain_length,
								  size_t *fix_header_bytes,
								  size_t *packet_read_size)
{
	mqtt_unsub_ack packet;
	
	ONT_LOG_DEBUG1("%s begin!",__FUNCTION__);
	if(NULL == device || NULL == buf 
					  || NULL == remain_length
					  || NULL == fix_header_bytes
					  || NULL == packet_read_size)
		return ONT_ERR_BADPARAM;
	
	*packet_read_size = *fix_header_bytes + *remain_length;
	ont_parser_init_mqtt_unsub_ack(&packet);
	if((ont_parser_deserialize(buf+(*fix_header_bytes), *remain_length, &packet.head, 0)) != 0)
	{
		ONT_LOG0(ONTLL_ERROR,"deserialize mqtt_unsub_ack failed\n");
		return DESERIALIZE_PACKET_ERR;
	}
	ONT_LOG_DEBUG2("%s: packet_identifier_ = %d\n",__FUNCTION__,packet.packet_identifier_.value);
	
	return ONT_ERR_OK;
}
int ont_handle_mqtt_keepalive_rsp(ont_device_t *device, 
								      const char *buf,
								      size_t *remain_length,
									  size_t *fix_header_bytes,
									  size_t *packet_read_size)
{
	ONT_LOG_DEBUG1("%s begin!",__FUNCTION__);
	if(NULL == device || NULL == buf 
					  || NULL == remain_length
					  || NULL == fix_header_bytes
					  || NULL == packet_read_size)
		return ONT_ERR_BADPARAM;
	/*
		*packet_read_size = 2
	*/
	*packet_read_size = *remain_length + *fix_header_bytes; 
	
	return ONT_ERR_OK;
}
/*
	如果读取packet失败，那么应当停止整个程序，否则，无法区分buf中不同包的界限
*/
int ont_mqtt_read_packet_cb(void *ctx,
								const char *buf,
								size_t buf_size,
								size_t *read_size)
{
	uint8_t packet_tag_ = 0;
	uint8_t packet_type_ = 0;
	size_t packet_read_size = 0; 
	size_t remain_length = 0;  
	size_t fix_header_bytes = 0; 
		
	ont_device_t *device = (ont_device_t *)ctx;
	ont_mqtt_device_t *mqtt_instance = DEV2MQTT(device);
	
	
	if (NULL == ctx || NULL == buf || NULL == read_size)
		return ONT_ERR_BADPARAM;

	mqtt_instance->mqtt_oper_result = 0;
	mqtt_instance->pkt_read_status = MQTT_READ_START;
	/*
			检查当前的buf是否包含一个完整的packet
	*/
	if(ont_is_complete_packet(buf,buf_size,&remain_length,&fix_header_bytes) != COMPLETE_PACKET)
	{		
		*read_size = 0;
		mqtt_instance->pkt_read_status = MQTT_READ_CONTINUE;
		return ONT_ERR_OK;
	}
	packet_tag_ = (uint8_t )(*buf);	
	packet_type_ = packet_tag_ >> 4;
	switch(packet_type_)
	{
	case CONNECT_PACKET:
		break;
	case CONNECT_ACK_PACKET:
	{
		mqtt_instance->mqtt_oper_result = ont_handle_mqtt_connect_ack(device,buf,&remain_length
												        ,&fix_header_bytes,&packet_read_size);
		break;
	}
	case PUBLISH_PACKET:
	{
		mqtt_instance->mqtt_oper_result = ont_handle_mqtt_publish_packet(device,buf,&remain_length
										             ,&fix_header_bytes,&packet_read_size);
		break;
	}
	case PUBLISH_ACK_PACKET:
	{
			
		mqtt_instance->mqtt_oper_result = ont_handle_mqtt_publish_ack(device,buf,&remain_length
										            ,&fix_header_bytes,&packet_read_size);
		break;
	}
	case SUBSCRIBE_PACKET:
		break;
	case SUBSCRIBE_ACK_PACKET:
	{
			
		mqtt_instance->mqtt_oper_result = ont_handle_mqtt_subscribe_ack(device,buf,&remain_length
																		,&fix_header_bytes,&packet_read_size);
		break;
	}
	case UNSUB_PACKET:
		break;
	case UNSUB_ACK_PACKET:
	{
		mqtt_instance->mqtt_oper_result = ont_handle_mqtt_unsub_ack(device,buf,&remain_length
										              ,&fix_header_bytes,&packet_read_size); 
		break;
	}
	case KEEPALIVE_PACKET:
		break;
	case KEEPALIVE_RSP_PACKET:
	{
		mqtt_instance->mqtt_oper_result = ont_handle_mqtt_keepalive_rsp(device,buf,&remain_length
										              ,&fix_header_bytes,&packet_read_size); 
		break;
	}
	default:
		break;
	}
	*read_size = packet_read_size;
	mqtt_instance->pkt_read_status = MQTT_READ_COMPLETE;
	return ONT_ERR_OK;
}

int ont_mqtt_keepalive(ont_device_t *device)
{
	int now_time = 0;
	
	int err = 0;
	ont_mqtt_device_t *mqtt_instance = NULL;
	
	if(NULL == device)
		return ONT_ERR_BADPARAM;
	
	now_time = ont_platform_time();
	mqtt_instance = DEV2MQTT(device);
	if (MQTT_DISCONNECTED == mqtt_instance->conn_status)
        return ONT_ERR_SOCKET_OP_FAIL;
	
	if((uint16_t)(now_time - mqtt_instance->access_time) >  mqtt_instance->mqtt_conn_keepalive/2)
	{
		
		mqtt_keepalive_packet packet;
		char send_buf[2]={'\0'};
		
		packet.packet_tag_ = CMD_KEEPALIVE<<4;
		packet.remain_length_ = 0;
	
		send_buf[0] = packet.packet_tag_;
		send_buf[1] = packet.remain_length_;	
		err = mqtt_instance->channel.fn_write(mqtt_instance->channel.channel
												,send_buf,sizeof(send_buf));
		CHECK_ONT_ERR_OK_RETURN(ONT_ERR_OK, err)
		
		mqtt_instance->pkt_read_status = MQTT_READ_START;
		do
		{
			err = mqtt_instance->channel.fn_process(mqtt_instance->channel.channel);
			
			if(ONT_ERR_OK != ont_handle_process_err(device,err))
			{
				return err;
			}
			ont_platform_sleep(30);
		} while (mqtt_instance->pkt_read_status != MQTT_READ_COMPLETE);
		
		if(ONT_ERR_OK == mqtt_instance->mqtt_oper_result)
		{
			update_access_time(device);
		}
	}
	else
	{
		mqtt_instance->pkt_read_status = MQTT_READ_START;
		do
		{
			err = mqtt_instance->channel.fn_process(mqtt_instance->channel.channel);
			
			if(ONT_ERR_OK != ont_handle_process_err(device,err))
			{
				return err;
			}
		} while (0);
	}
	return mqtt_instance->mqtt_oper_result;
}


ont_device_cmd_t* ont_mqtt_get_cmd(ont_device_t *device)
{
	ont_device_cmd_t *cmd = NULL;
	ont_mqtt_device_t *mqtt_instance = NULL;
	
	mqtt_instance = DEV2MQTT(device);
	if(NULL == device)
		return NULL;
	if(NULL == mqtt_instance->cmd_list)
		return NULL;
	if(ont_list_size(mqtt_instance->cmd_list) > 0)
	{
		ont_list_pop_front(mqtt_instance->cmd_list,(void **) &cmd);
	}
	return cmd;
}

int ont_mqtt_reply_cmd(ont_device_t *device,const char *cmdid,const char *resp,size_t size)
{
	int err = 0;
	uint16_t sys_cmd_rsp_len = 0;
	uint16_t cmdid_len = 0;
	char *cmd_rsp_topic = NULL;
	ont_mqtt_device_t *mqtt_instance = NULL;
	
        ONT_LOG_DEBUG1("%s begin\n",__FUNCTION__);
	if(NULL == device || NULL == cmdid 
					  || NULL == resp || 0 == size)
		return ONT_ERR_BADPARAM;

	sys_cmd_rsp_len = strlen(SYS_CMD_RSP_PLATFORM_CMD);
	cmdid_len = strlen(cmdid);
	/*
		include '/' && '\0'
	*/
	cmd_rsp_topic = (char *)ont_platform_malloc(sys_cmd_rsp_len + cmdid_len + 2); 
	if (NULL == cmd_rsp_topic)
		return ONT_ERR_NOMEM;

	
	mqtt_instance = DEV2MQTT(device);
	/*
		拼凑回复平台的topic_name: $crsp/c87164fe-ce20-56b5-870d-94cbee139f8b
	*/
	strncpy(cmd_rsp_topic, SYS_CMD_RSP_PLATFORM_CMD, sys_cmd_rsp_len);
	cmd_rsp_topic[sys_cmd_rsp_len] = '/';
	
	strncpy(cmd_rsp_topic + sys_cmd_rsp_len + 1, cmdid, cmdid_len);
	cmd_rsp_topic[sys_cmd_rsp_len + cmdid_len + 1] = '\0';

	ont_pkt_formatter_reset(device->formatter,0);
	ont_serialize_mqtt_publish_packet(device,cmd_rsp_topic,resp,size,1);
	

	err = mqtt_instance->channel.fn_write(mqtt_instance->channel.channel,
										  device->formatter->result.data,
										  device->formatter->result.len);
	CHECK_ONT_ERR_OK_RETURN(ONT_ERR_OK, err)

	ont_platform_free(cmd_rsp_topic);
	cmd_rsp_topic = NULL;
	
	mqtt_instance->pkt_read_status = MQTT_READ_START;
	do
	{
		err = mqtt_instance->channel.fn_process(mqtt_instance->channel.channel);
		
		if(ONT_ERR_OK != ont_handle_process_err(device,err))
			return err;
		
		ont_platform_sleep(30);
	} while (mqtt_instance->pkt_read_status != MQTT_READ_COMPLETE);

	if(ONT_ERR_OK == mqtt_instance->mqtt_oper_result)
	{
		update_access_time(device);
	}	
	return mqtt_instance->mqtt_oper_result;
}


int ont_mqtt_sub_topic(ont_device_t *device,const char **topics,size_t size)
{
	int err = 0;
	ont_mqtt_device_t *mqtt_instance = NULL;
	
	if(NULL == device || NULL == topics || 0 == size)
		return ONT_ERR_BADPARAM;
	
	mqtt_instance = DEV2MQTT(device);
	
	ont_pkt_formatter_reset(device->formatter,0);
	err = ont_serialize_mqtt_subscribe_packet(device,topics,size);
	CHECK_ONT_ERR_OK_RETURN(ONT_ERR_OK, err)
	
	
	
	err = mqtt_instance->channel.fn_write(mqtt_instance->channel.channel,
										  device->formatter->result.data,
										  device->formatter->result.len);
	CHECK_ONT_ERR_OK_RETURN(ONT_ERR_OK, err)
	
	mqtt_instance->pkt_read_status = MQTT_READ_START;
	do
	{
		err = mqtt_instance->channel.fn_process(mqtt_instance->channel.channel);
		
		if(ONT_ERR_OK != ont_handle_process_err(device,err))
			return err;
		
		ont_platform_sleep(30);
	} while (mqtt_instance->pkt_read_status != MQTT_READ_COMPLETE);

	if(ONT_ERR_OK == mqtt_instance->mqtt_oper_result)
	{
		update_access_time(device);
	}
	
	return mqtt_instance->mqtt_oper_result;
}


int ont_mqtt_unsub_topic(ont_device_t *device,const char **topics,size_t size)
{
	int err = 0;
	ont_mqtt_device_t *mqtt_instance = NULL;
	if(NULL == device || NULL == topics || 0 == size)
		return ONT_ERR_BADPARAM;
	
	mqtt_instance = DEV2MQTT(device);
	
	ont_pkt_formatter_reset(device->formatter,0);
	err = ont_serialize_mqtt_unsub_packet(device,topics,size);
	CHECK_ONT_ERR_OK_RETURN(ONT_ERR_OK, err)

	
	
	err = mqtt_instance->channel.fn_write(mqtt_instance->channel.channel,
										  device->formatter->result.data,
										  device->formatter->result.len);
	CHECK_ONT_ERR_OK_RETURN(ONT_ERR_OK, err)

	mqtt_instance->pkt_read_status = MQTT_READ_START;
	do
	{
		err = mqtt_instance->channel.fn_process(mqtt_instance->channel.channel);
		
		if(ONT_ERR_OK != ont_handle_process_err(device,err))
			return err;
		
		ont_platform_sleep(30);
	} while (mqtt_instance->pkt_read_status != MQTT_READ_COMPLETE);

	if(ONT_ERR_OK == mqtt_instance->mqtt_oper_result)
	{
		update_access_time(device);
	}
	
	return mqtt_instance->mqtt_oper_result;
}

int ont_mqtt_publish(ont_device_t *device,const char *topic_name,
						 const char *content,size_t size,uint8_t qos_level)
{
	int err = 0;
	ont_mqtt_device_t *mqtt_instance = NULL;
	
	if(NULL == device || NULL == topic_name 
				      || NULL == content || 0 == size)
		return ONT_ERR_BADPARAM;
	
	mqtt_instance = DEV2MQTT(device);
	
	ont_pkt_formatter_reset(device->formatter,0);
	err = ont_serialize_mqtt_publish_packet(device,topic_name,content,size,qos_level);
	CHECK_ONT_ERR_OK_RETURN(ONT_ERR_OK, err)
	
	
	err = mqtt_instance->channel.fn_write(mqtt_instance->channel.channel,
										  device->formatter->result.data,
										  device->formatter->result.len);
	CHECK_ONT_ERR_OK_RETURN(ONT_ERR_OK, err)
	
	mqtt_instance->pkt_read_status = MQTT_READ_START;
	do
	{
		err = mqtt_instance->channel.fn_process(mqtt_instance->channel.channel);
		
		if(ONT_ERR_OK != ont_handle_process_err(device,err))
			return err;
		
		ont_platform_sleep(30);
	} while (mqtt_instance->pkt_read_status != MQTT_READ_COMPLETE);

	if(ONT_ERR_OK == mqtt_instance->mqtt_oper_result)
	{
		update_access_time(device);
	}
	return mqtt_instance->mqtt_oper_result;
}

int ont_mqtt_send_datapoints(ont_device_t *device, const char* payload, size_t bytes)
{
	int err = 0;
	ont_mqtt_device_t *mqtt_instance = NULL;
	
	if (NULL == device || NULL == payload || 0 == bytes)
		return ONT_ERR_BADPARAM;
	
	mqtt_instance = DEV2MQTT(device);
	
	ont_pkt_formatter_reset(mqtt_instance->mqtt_formatter,0);
	err = ont_serialize_mqtt_send_dp_packet(device,SYS_CMD_UPLOAD_DATA_POINT,payload,bytes);
	CHECK_ONT_ERR_OK_RETURN(ONT_ERR_OK, err)
	
	err = mqtt_instance->channel.fn_write(mqtt_instance->channel.channel,
										  mqtt_instance->mqtt_formatter->result.data,
										  mqtt_instance->mqtt_formatter->result.len);
	CHECK_ONT_ERR_OK_RETURN(ONT_ERR_OK, err)

	mqtt_instance->pkt_read_status = MQTT_READ_START;
	do
	{
		err = mqtt_instance->channel.fn_process(mqtt_instance->channel.channel);
		
		if(ONT_ERR_OK != ont_handle_process_err(device,err))
			return err;
		
		ont_platform_sleep(30);
	} while (mqtt_instance->pkt_read_status != MQTT_READ_COMPLETE);

	if(ONT_ERR_OK == mqtt_instance->mqtt_oper_result)
	{
		update_access_time(device);
	}
	
	return mqtt_instance->mqtt_oper_result;
}
