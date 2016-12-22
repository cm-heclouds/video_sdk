#ifndef ONT_SRC_EDP_ONT_EDP_PRIVATE_H_
#define ONT_SRC_EDP_ONT_EDP_PRIVATE_H_


#include "ont_tcp_channel.h"

#include "ont/device.h"
#include "ont/platform.h"
#include "ont_list.h"
#include "parser/macros.h"
#include "parser/ont_pkt_formatter.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EDP_CONNECT_TIMEOUT         5
#define EDP_CONNECT_KEEPALVIE       600

#define MIN(a,b)    (a>b?b:a)


enum EDP_PROTOCOL_TYPE
{
    EDP_CONNECT_REQ   = 0x10,
    EDP_CONNECT_RESP  = 0x20,
    EDP_CONNECT_CLOSE = 0x40,
    EDP_SAVE_DATA     = 0x80,
    EDP_SAVE_ACK      = 0x90,
    EDP_CMD_REQ       = 0xA0,
    EDP_CMD_RESP      = 0xB0,
    EDP_PING_REQ      = 0xC0,
    EDP_PING_RESP     = 0xD0,
    EDP_ENCRPT_REQ    = 0xE0,
    EDP_ENCRPT_RESP   = 0xF0
};

enum EDP_DEVICE_CONNECT_STATUS{
    DEVICE_NOT_CONNECTED = 0,
    DEVICE_CONNECTED
};

enum EDP_RESP_STATUS{
    EDP_RESP_ERR,
    EDP_RESP_OK,
    EDP_RESP_CONTINUE
};

/*
@brief  edp类型设备结构
*/
typedef struct ont_edp_device_t{
    ont_device_t                info; /*设备信息*/
    ont_channel_interface_t     channel;/*连接信息*/
   
/*    ont_unified_pkt_formatter_t data_formatter;
    ont_pkt_formatted_data_t    finished_data;
    */
    ont_pkt_formatter_t         *formatter;
    ont_list_t*                 cmd;
    int                         result;
    int                         resp_status;
    unsigned                    msg_id;
    int                         connect_status;
    int                         last_keep_alive_time;
}ont_edp_device_t;

/*edp connect packet*/
ont_parser_def_packet_begin(edp_connect_t)
ont_parser_add_elem(pro, bytes)
ont_parser_add_elem(ver, uint8)
ont_parser_add_elem(flag, uint8)
ont_parser_add_elem(keepalive, uint16)
ont_parser_add_elem(dev_id, bytes)
ont_parser_add_elem(user_id, bytes)
ont_parser_add_elem(auth_info, bytes)
ont_parser_def_packet_end(edp_connect_t)


/*edp send datapoints packet*/
ont_parser_def_packet_begin(edp_dp_t)
ont_parser_add_elem(flag, uint8)
ont_parser_add_elem(msg_id, uint16)
ont_parser_add_elem(dps, raw)
ont_parser_def_packet_end(edp_dp_t)

/*edp save data ack packet*/
ont_parser_def_packet_begin(edp_save_ack_t)
ont_parser_add_elem(flag, uint8)
ont_parser_add_elem(msg_id, uint16)
ont_parser_add_elem(code, uint8)
ont_parser_def_packet_end(edp_save_ack_t)

/*edp cmd req && resp packet*/
ont_parser_def_packet_begin(edp_cmd_t)
ont_parser_add_elem(cmd_id, bytes)
ont_parser_add_elem(bin, binary)
ont_parser_def_packet_end(edp_cmd_t)

/*** api ***/

/*connect*/ 
int ont_edp_handle_connect(ont_edp_device_t* device, const char* auth_info);
int ont_edp_handle_send_dp(ont_edp_device_t* device,
    const char* data, size_t data_len, uint16_t msg_id);
int ont_edp_handle_reply_cmd(ont_edp_device_t* device, const char* cmd_id, const char*data, uint32_t len);


int ont_edp_handle_save_ack(const unsigned char* data, uint32_t len);
int ont_edp_handle_get_cmd(const unsigned char* data, uint32_t len,
    ont_edp_device_t* device);
/*
int ont_edp_realloc(void** src, uint32_t src_len, uint32_t dst_len);
*/

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif
