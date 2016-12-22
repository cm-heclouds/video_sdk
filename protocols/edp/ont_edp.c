#include "ont_edp.h"
#include "ont_edp_private.h"
#include "ont_base_common.h"
#include "ont_channel.h"
#include "ont/log.h"


#ifdef ONT_PROTOCOL_EDP

#define CAST_TYPE(type, src, dst)\
    type dst=(type)src

#define CHECK_RESULT(retcode, error)\
    do \
    {\
        if (retcode)\
            return error;\
    } while (0)


int ont_edp_cb_recv_packet(void* ctx, const char* buf,
    size_t buf_size, size_t * read_size);
int ont_edp_cb_close(void* channel);

int ont_edp_check_packet(const unsigned char* buf, int len, 
    int *remain_len, int *pkt_len, int *data_start_pos);


int ont_edp_create(ont_device_t **dev)
{
    ont_edp_device_t* edp_dev = NULL;
    if (NULL == dev)
    {
        return ONT_ERR_BADPARAM;
    }
    edp_dev = (ont_edp_device_t*)ont_platform_malloc(sizeof(ont_edp_device_t));
    if (NULL == edp_dev)
        return ONT_ERR_NOMEM;

    edp_dev->channel.channel = NULL;

    *dev = (ont_device_t *)edp_dev;

    edp_dev->msg_id = 1;
    edp_dev->connect_status = DEVICE_NOT_CONNECTED;
    edp_dev->last_keep_alive_time = 0;
    edp_dev->cmd = ont_list_create();
    edp_dev->formatter = (ont_pkt_formatter_t*)ont_platform_malloc(sizeof(ont_pkt_formatter_t));
    if (NULL == edp_dev->formatter)
    {
        ont_list_destroy(edp_dev->cmd);
        ont_platform_free(edp_dev);
        return ONT_ERR_NOMEM;
    }
        

    if (ONT_ERR_OK != ont_pkt_formatter_init(edp_dev->formatter)){
        ont_list_destroy(edp_dev->cmd);
        ont_platform_free(edp_dev->formatter);
        ont_platform_free(edp_dev);
    }

    return ONT_ERR_OK;
}

void ont_cb_destroy_cmd(void* ele, void* ctx)
{
    CAST_TYPE(ont_device_cmd_t*, ele, cmd);
    if(cmd && cmd->req)
    {
        ont_platform_free(cmd->req);
        cmd->req = NULL;
    }
}

void ont_edp_destroy(ont_device_t *dev)
{
    CAST_TYPE(ont_edp_device_t*, dev, device);
    if (NULL == dev)
        return;

    if (device->channel.fn_deinitilize)
    {
        device->channel.fn_deinitilize(device->channel.channel);
    }
    
    if (device->cmd)
    {
        ont_list_foreach(device->cmd, ont_cb_destroy_cmd, NULL);
        ont_list_destroy(device->cmd);
    }

    if (device->formatter)
    {
        ont_pkt_formatter_destroy(device->formatter);
        ont_platform_free(device->formatter);
    }

    ont_platform_free(device);
}
int ont_edp_connect(ont_device_t *dev, const char *auth_info)
{
    CAST_TYPE(ont_edp_device_t*, dev, device);

    int retcode = ont_tcp_channel_create(
                        &device->channel,
                        device->info.ip,
                        device->info.port,
                        ONT_SOCK_RECV_BUF_SIZE,
                        ONT_SOCK_SEND_BUF_SIZE,
                        device,
                        EDP_CONNECT_TIMEOUT);
    CHECK_RESULT(retcode, retcode);

    retcode = device->channel.fn_initilize(
        device->channel.channel,
        ont_edp_cb_recv_packet);
    CHECK_RESULT(retcode, retcode);

    retcode = device->channel.fn_connect(device->channel.channel, &dev->exit);
    CHECK_RESULT(retcode, retcode);

    retcode = ont_edp_handle_connect(device, device->info.key);
    CHECK_RESULT(retcode, retcode);

    retcode = device->channel.fn_write(
        device->channel.channel,
        device->formatter->result.data,
        device->formatter->result.len);

    ont_pkt_formatter_reset(device->formatter, 0);
    CHECK_RESULT(retcode, retcode);
    
    device->resp_status = EDP_RESP_CONTINUE;
    do 
    {
        retcode = device->channel.fn_process(device->channel.channel);
        ont_platform_sleep(10);
    } while (retcode == 0 && device->resp_status == EDP_RESP_CONTINUE);

    if(retcode)
        device->result = retcode;

    if (ONT_ERR_OK == device->result)
    {
        device->last_keep_alive_time = ont_platform_time();
        device->connect_status = DEVICE_CONNECTED;
    }
       
    return device->result;
}

/*retcode <0  protocol error; 0 complete; >0 need more bytes*/
int ont_edp_check_packet(const unsigned char* buf, 
                        int len, 
                        int *remain_len, 
                        int *pkt_len,
                        int *data_start_pos)
{
    int read_count = 1;
    int pos = 0;
    int multiplicator = 1;

    *remain_len = 0;
    *pkt_len = 0;
    do
    {
        if (read_count > 4)
            return -1;/*protocol error*/
        if (read_count > len - 1)
            return 1;/*continue to receive*/
        
        read_count++;
        pos++;
        *remain_len += buf[pos] * multiplicator;
        multiplicator *= 0x80;
    } while (buf[pos] & 0x080);

    *pkt_len = *remain_len + read_count;
    if (*pkt_len <= len)
    {
        *data_start_pos = read_count;
         return 0;
    }
        
    return 1;
}


int ont_edp_cb_recv_packet(void* ctx, const char* buf, size_t buf_size, size_t * read_size)
{
    CAST_TYPE(ont_edp_device_t*, ctx, device);

    const unsigned char* data = (const unsigned char*)buf;
    uint8_t edp_type = 0;
    int remain_len = 0;
    int total_len = 0;
    int retcode = 0;
    int data_start_pos = 0;

    device->result = 0;
    device->resp_status = EDP_RESP_ERR;
    
    retcode = ont_edp_check_packet(data, buf_size, &remain_len, &total_len, &data_start_pos);
    if (retcode < 0)
        return ONT_ERR_PROTOCOL;

    if (retcode > 0)
    {
        *read_size = 0;/*need more bytes*/
        device->resp_status = EDP_RESP_CONTINUE;
        return ONT_ERR_OK;
    }

    *read_size = total_len;

    edp_type = data[0];
    switch (edp_type)
    {
    case EDP_CONNECT_RESP:
        ONT_LOG0(ONTLL_INFO, "recv connect resp!");
        if (total_len != 4)
            device->result = ONT_ERR_PROTOCOL;
        else
        {
            device->result = data[3] & 0x00FF;
            if(device->result != 0)
            {
                ONT_LOG1(ONTLL_ERROR, "recv connect resp , error:%d", device->result);
                device->result = ONT_ERR_FAILED_TO_AUTH;
            }
        }
            
        break;
    case EDP_CONNECT_CLOSE:
       ONT_LOG0(ONTLL_INFO, "recv connect close!");
        device->connect_status = DEVICE_NOT_CONNECTED;

        if (total_len != 3)
            device->result = ONT_ERR_PROTOCOL;
        else
        {
            device->result = data[2] & 0x00FF;
            ONT_LOG1(ONTLL_ERROR, "recv close packet, error:%d", device->result);
            device->result = ONT_ERR_SOCKET_OP_FAIL;
        }
        break;
    case EDP_PING_RESP:
       ONT_LOG0(ONTLL_INFO, "recv ping resp!");
        if (total_len != 2 
            || (buf[0] & 0x00FF) != 0x0D0 || buf[1] != 0)
            device->result = ONT_ERR_PROTOCOL;
        else
            device->result = 0;
        break;
    case EDP_SAVE_ACK:
       ONT_LOG0(ONTLL_INFO, "recv save ack!");
        device->result = ont_edp_handle_save_ack(data + data_start_pos, remain_len);
        if (device->result == device->msg_id)
            device->result = 0;
        else
            device->result = ONT_ERR_TIMEOUT;
        break;
    case EDP_CMD_REQ:
       ONT_LOG0(ONTLL_INFO, "recv cmd!");
        device->result = ont_edp_handle_get_cmd(data + data_start_pos, remain_len, device);
        device->resp_status = EDP_RESP_CONTINUE;
        return 1;/*continue recv*/
    default:
        break;
    }
    device->resp_status = EDP_RESP_OK;
    return device->result;
}


ont_device_cmd_t* ont_edp_get_cmd(ont_device_t *dev)
{
    CAST_TYPE(ont_edp_device_t*, dev, device);
    ont_device_cmd_t *cmd = NULL;
    if (ont_list_size(device->cmd) > 0)
    {
        ont_list_pop_front(device->cmd, (void **)&cmd);
    }

    return cmd;
}

int  ont_edp_reply_cmd(ont_device_t *dev,
    const char* cmd_id,
    const char* resp_data,
    size_t  data_len)
{
    CAST_TYPE(ont_edp_device_t*, dev, device);

    int retcode = 0;
    if (dev == NULL || cmd_id == NULL || resp_data == NULL)
        return ONT_ERR_BADPARAM;

    retcode = ont_edp_handle_reply_cmd(device, cmd_id, resp_data, data_len);
    if (retcode)
    {
        return ONT_ERR_BADPARAM;
    }

    retcode = device->channel.fn_write(device->channel.channel,
        device->formatter->result.data,
        device->formatter->result.len);
    ont_pkt_formatter_reset(device->formatter, 0);
    CHECK_RESULT(retcode, retcode);

    device->last_keep_alive_time = ont_platform_time();

    do
    {
        retcode = device->channel.fn_process(device->channel.channel);
    } while (retcode == 0 &&
        (ont_tcp_channel_left_write_buf_size(device->channel.channel) > 0));

    return retcode;
}

int ont_edp_keepalive(ont_device_t *dev)
{
    CAST_TYPE(ont_edp_device_t*, dev, device);

    const char data[2] = { (char)EDP_PING_REQ, 0 };
    int retcode = 0;
    if (device->connect_status == DEVICE_NOT_CONNECTED)
        return ONT_ERR_SOCKET_OP_FAIL;

    if (ont_platform_time() - device->last_keep_alive_time < device->info.keepalive/3)
        return ONT_ERR_OK;

    retcode = device->channel.fn_write(device->channel.channel, data, sizeof(data));
    CHECK_RESULT(retcode, retcode);

    device->resp_status = EDP_RESP_CONTINUE;
    do
    {
        retcode = device->channel.fn_process(device->channel.channel);
        ont_platform_sleep(10);
    } while (retcode == 0 && device->resp_status == EDP_RESP_CONTINUE);

    if(retcode)
        device->result = retcode;

    if (device->result == ONT_ERR_OK)
        device->last_keep_alive_time = ont_platform_time();

    return device->result;
}

int  ont_edp_send_datapoints(
    ont_device_t *dev,
    const char *data,
    size_t data_len)
{
    CAST_TYPE(ont_edp_device_t*, dev, device);
    int retcode = 0;

    if (dev == NULL || data == NULL || data_len == 0)
        return ONT_ERR_BADPARAM;

    retcode = ont_edp_handle_send_dp(device, data, data_len, device->msg_id);
    CHECK_RESULT(retcode, retcode);

    retcode = device->channel.fn_write(device->channel.channel, 
        device->formatter->result.data,
        device->formatter->result.len);
    ont_pkt_formatter_reset(device->formatter, 0);
    CHECK_RESULT(retcode, retcode);

    device->resp_status = EDP_RESP_CONTINUE;
    do
    {
        retcode = device->channel.fn_process(device->channel.channel);
        ont_platform_sleep(50);
    } while (retcode == 0 && device->resp_status == EDP_RESP_CONTINUE);

    if(retcode)
        device->result = retcode;

    device->msg_id++;
    if (device->msg_id > 65535)
        device->msg_id = 1;

    device->last_keep_alive_time = ont_platform_time();

    return retcode;
}

#endif