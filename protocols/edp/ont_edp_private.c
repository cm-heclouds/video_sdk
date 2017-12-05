#include <string.h>
#include "ont_edp_private.h"
#include "ont/log.h"
#include "parser/parser.h"

#ifdef ONT_PROTOCOL_EDP

#define EDP_MIN_KEEP_ALIVE  6
#define EDP_MAX_KEEP_ALIVE  1800

#define ONT_EDP_SERIALIZE_FINISH(msg_type)\
    ont_pkt_serialize_finish(device->formatter, msg_type, &pkt->head)

static int32_t edp_user_id_filter(ont_parser_list_node_t* node){
    ont_parser_uint8_t* p = (ont_parser_uint8_t*)node;
    if (p->value & 0x80){
        return 0;
    }

    return 1;
}

static int32_t edp_msg_id_filter(ont_parser_list_node_t* node){
    ont_parser_uint8_t* p = (ont_parser_uint8_t*)node;
    if (p->value & 0x40){
        return 0;
    }

    return 1;
}

/*parse connect packet*/
ont_parser_init_begin(edp_connect_t, pro, bytes)
ont_parser_queue_up(ver, uint8, NULL, none);
ont_parser_queue_up(flag, uint8, NULL, none);
ont_parser_queue_up(keepalive, uint16, NULL, none);
ont_parser_queue_up(dev_id, bytes, NULL, none);
ont_parser_queue_up(user_id, bytes, edp_user_id_filter, flag);
ont_parser_queue_up(auth_info, bytes, NULL, none);
ont_parser_init_end()

/*parse send dp packet*/
ont_parser_init_begin(edp_dp_t, flag, uint8)
ont_parser_queue_up(msg_id, uint16, edp_msg_id_filter, flag);
ont_parser_queue_up(dps, raw, NULL, none);
ont_parser_init_end()



/*parse save data ack packet*/
ont_parser_init_begin(edp_save_ack_t, flag, uint8)
ont_parser_queue_up(msg_id, uint16, NULL, none)
ont_parser_queue_up(code, uint8, NULL, none)
ont_parser_init_end()

/*cmd req && resp packet*/
ont_parser_init_begin(edp_cmd_t, cmd_id, bytes)
ont_parser_queue_up(bin, binary, NULL, none)
ont_parser_init_end()

/*cmd trans data req && resp packet*/
ont_parser_init_begin(edp_trans_data_t, svr_name, bytes)
ont_parser_queue_up(data, raw, NULL, none)
ont_parser_init_end()

/*cmd pushdata req && resp packet*/
ont_parser_init_begin(edp_push_data_t, devid, bytes)
ont_parser_queue_up(data, raw, NULL, none)
ont_parser_init_end()


int ont_edp_handle_connect(ont_edp_device_t* device, const char* auth_info){
    edp_connect_t* pkt = NULL;

    char id_buffer[16];

    if (!device){
        return -1;
    }
    if(device->info.keepalive < EDP_MIN_KEEP_ALIVE)
        device->info.keepalive = EDP_MIN_KEEP_ALIVE;
    if(device->info.keepalive > EDP_MAX_KEEP_ALIVE)
        device->info.keepalive = EDP_MAX_KEEP_ALIVE;
    pkt = (edp_connect_t*)ont_platform_malloc(sizeof(edp_connect_t));
    if (NULL == pkt)
    {
        return -1;
    }
    if(device->info.device_id != 0)
    {
	ont_platform_snprintf(id_buffer, sizeof(id_buffer),
		"%u", device->info.device_id);
    }else{
	ont_platform_snprintf(id_buffer, sizeof(id_buffer),
		"%u", device->info.product_id);
    }
    ont_parser_init_edp_connect_t(pkt);
    pkt->pro.value = "EDP";
    pkt->pro.len = 3;
    pkt->ver.value = 1;
    pkt->keepalive.value = device->info.keepalive;
    if(device->info.device_id != 0){
	pkt->flag.value |= 0x40;
	pkt->dev_id.value = id_buffer;
	pkt->dev_id.len = strlen(id_buffer);
	pkt->auth_info.value = device->info.key;
	pkt->auth_info.len = strlen(device->info.key);
    }else{
	pkt->flag.value |= 0xC0;
	pkt->dev_id.value = "";
	pkt->dev_id.len = strlen("");
	pkt->user_id.value = id_buffer;
	pkt->user_id.len = strlen(id_buffer);
	pkt->auth_info.value = auth_info;
	pkt->auth_info.len = strlen(auth_info);
    }
    ONT_EDP_SERIALIZE_FINISH(EDP_CONNECT_REQ);
    ont_platform_free(pkt);

    return 0;
}

int ont_edp_handle_save_ack(const unsigned char* data, uint32_t len,ont_edp_device_t* device)
{
    int value;
    edp_save_ack_t* pkt = (edp_save_ack_t*)ont_platform_malloc(sizeof(edp_save_ack_t));
    if (NULL == pkt)
    {
        return -1;
    }
    ont_parser_init_edp_save_ack_t(pkt);

    if (ont_parser_deserialize((const char *)data, len, &pkt->head, 0))
    {
	ont_platform_free(pkt);
        return -1;
    }

    if (pkt->code.value != 0){
	ont_platform_free(pkt);
        return 0;
    }
    value = pkt->msg_id.value;
    ont_platform_free(pkt);

#ifdef ONT_PROTOCOL_EDP_EXTRA
    if( device->save_ack_cb_ex == NULL )
    {
        /*return -1;*/
    }
    else
    {
        device->save_ack_cb_ex( &device->info, value );
    }
#endif

    return value;
}

int ont_edp_handle_get_cmd(const unsigned char* data, uint32_t len,
    ont_edp_device_t* device)
{
    ont_device_cmd_t* cmd = NULL;
    edp_cmd_t pkt;
    ont_parser_init_edp_cmd_t(&pkt);

    if (ont_parser_deserialize((const char *)data, len, &pkt.head, 0))
    {
        return -1;
    }

    cmd = (ont_device_cmd_t*)ont_platform_malloc(sizeof(ont_device_cmd_t));
    if (NULL == cmd)
        return -1;


    memcpy(cmd->id, pkt.cmd_id.value, pkt.cmd_id.len);
    cmd->id[pkt.cmd_id.len] = 0;

    cmd->req = (char*)ont_platform_malloc(pkt.bin.len);
    if (cmd->req == NULL)
    {
        ont_platform_free(cmd);
        return -1;
    }
        

    memcpy(cmd->req, pkt.bin.value, pkt.bin.len);
    cmd->size = pkt.bin.len;

    ont_list_insert(device->cmd, cmd);
    return 0;
}

int ont_edp_handle_reply_cmd(ont_edp_device_t* device,
    const char* cmd_id, const char*data, uint32_t len)
{
    edp_cmd_t* pkt = (edp_cmd_t*)ont_platform_malloc(sizeof(edp_cmd_t));
    if (NULL == pkt)
        return -1;

    ont_parser_init_edp_cmd_t(pkt);

    pkt->cmd_id.value = cmd_id;
    pkt->cmd_id.len = strlen(cmd_id);
    pkt->bin.value = data;
    pkt->bin.len = len;

    ONT_EDP_SERIALIZE_FINISH(EDP_CMD_RESP);
    ont_platform_free(pkt);
    return 0;
}

int ont_edp_handle_send_dp(ont_edp_device_t* device,
    const char* data, size_t data_len, uint16_t msg_id)
{
    edp_dp_t* pkt = (edp_dp_t*)ont_platform_malloc(sizeof(edp_dp_t));
    if (NULL == pkt)
        return -1;

    ont_parser_init_edp_dp_t(pkt);
    pkt->flag.value = msg_id == 0 ? 0x0 : 0x40;
    pkt->msg_id.value = msg_id;
    pkt->dps.value = data;
    pkt->dps.len = data_len;

    ONT_EDP_SERIALIZE_FINISH(EDP_SAVE_DATA);

    ont_platform_free(pkt);
    return 0;
}
int ont_edp_handle_send_trans_data(ont_edp_device_t* device,const char* svr_name,
	const char* data, size_t data_len)
{
    edp_trans_data_t* pkt = (edp_trans_data_t*)ont_platform_malloc(sizeof(edp_trans_data_t));
    if (NULL == pkt)
        return -1;

    ont_parser_init_edp_trans_data_t(pkt);
    pkt->svr_name.len = strlen(svr_name);
    pkt->svr_name.value = svr_name;
    pkt->data.len = data_len;
    pkt->data.value = data;

    ONT_EDP_SERIALIZE_FINISH(EDP_TRANS_DATA);

    ont_platform_free(pkt);
    return 0;

}
int ont_edp_handle_get_transdata(const unsigned char* data,size_t len,ont_edp_device_t* device)
{
    edp_trans_data_t pkt;
    ont_parser_init_edp_trans_data_t(&pkt);
    
    pkt.data.len = len - ( *data << 8 | *(data+1)) - sizeof(uint16_t);
    if (ont_parser_deserialize((const char *)data,(uint32_t) len, &pkt.head, 0))
    {
        return -1;
    }

    if(device->transdata_cb == NULL)
    {
        /*return -1;*/
    }
    else
    {
        device->transdata_cb( pkt.svr_name.value,pkt.svr_name.len,
                              pkt.data.value,pkt.data.len);
    }

#ifdef ONT_PROTOCOL_EDP_EXTRA
    if( device->transdata_cb_ex == NULL )
    {
        /*return -1;*/
    }
    else
    {
        device->transdata_cb_ex( &device->info,
                                 pkt.svr_name.value,
                                 pkt.svr_name.len,
                                 pkt.data.value,
                                 pkt.data.len );
    }
#endif

    return 0;
}

int ont_edp_handle_get_pushdata(const unsigned char* data,size_t len,ont_edp_device_t* device)
{
    edp_push_data_t pkt;
    ont_parser_init_edp_push_data_t(&pkt);
    
    pkt.data.len = len - ( *data << 8 | *(data+1)) - sizeof(uint16_t);
    if (ont_parser_deserialize((const char *)data,(uint32_t) len, &pkt.head, 0))
    {
        return -1;
    }

    if(device->pushdata_cb == NULL)
    {
        /* return -1; */
    }
    else
    {
        device->pushdata_cb( pkt.devid.value,
                             pkt.devid.len,
                             pkt.data.value,
                             pkt.data.len);
    }

#ifdef ONT_PROTOCOL_EDP_EXTRA
    if( device->pushdata_cb_ex == NULL )
    {
        /*return -1;*/
    }
    else
    {
        device->pushdata_cb_ex( &device->info,
                                 pkt.devid.value,
                                 pkt.devid.len,
                                 pkt.data.value,
                                 pkt.data.len );
    }
#endif

    return 0;
}

int ont_edp_handle_send_pushdata(ont_edp_device_t* device,const char* devid,
    const char* data, size_t data_len)
{
    edp_push_data_t* pkt = (edp_push_data_t*)ont_platform_malloc(sizeof(edp_push_data_t));
    if (NULL == pkt)
        return -1;

    ont_parser_init_edp_push_data_t(pkt);
    pkt->devid.len = strlen(devid);
    pkt->devid.value = devid;
    pkt->data.len = data_len;
    pkt->data.value = data;

    ONT_EDP_SERIALIZE_FINISH(EDP_PUSH_DATA);

    ont_platform_free(pkt);
    return 0;

}

#endif
