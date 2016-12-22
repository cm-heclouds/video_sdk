#include "ont_jtext.h"
#include <string.h>
#include <ont/platform.h>
#include <ont/error.h>
#include <ont/device.h>
#include <ont/log.h>
#include <ont_channel.h>
#include <ont_tcp_channel.h>
#include "types.h"
#include "packet.h"

#ifdef ONT_PROTOCOL_JTEXT


#define ont_jtext_update_ping_time(ctx) do {                            \
        ctx->next_ping_time = ont_platform_time() + ctx->dev.keepalive / 2; \
    } while(0)

static int ont_jtext_handle_reg_resp(ont_jtext_t *ctx)
{
    int err;
    uint16_t sn;
    char result;
    const char *auth_code;
    uint16_t auth_code_len;

    err = ont_packet_parse_msg_regresp(ont_packet_get_body(ctx->recv_pkt),
                                       ont_packet_get_body_len(ctx->recv_pkt),
                                       &sn , &result, &auth_code, &auth_code_len);

    if (ONT_ERR_OK != err)
        return err;

    if (0 != result)
    {
        ONT_LOG1(ONTLL_ERROR, "Failed to 'REGISTER' the device, result=%d", result);
        return ONT_ERR_FAILED_TO_AUTH;
    }

    ctx->auth_code = (char*)ont_platform_malloc(auth_code_len + 1);
    if (NULL == ctx->auth_code)
        return ONT_ERR_NOMEM;

    memcpy(ctx->auth_code, auth_code, auth_code_len);
    ctx->auth_code[auth_code_len] = '\0';
    return ONT_ERR_OK;
}

static int ont_jtext_handle_platresp(ont_jtext_t *ctx)
{
    int err;
    uint16_t sn;
    uint16_t msg_id;
    char result;

    err = ont_packet_parse_msg_platresp(ont_packet_get_body(ctx->recv_pkt),
                                        ont_packet_get_body_len(ctx->recv_pkt),
                                        &sn, &msg_id, &result);

    if (ONT_ERR_OK != err)
        return err;

    switch (msg_id) {
    case ONT_JTEXT_MSG_AUTH:
        if (0 == result)
        {
            ctx->dev.status = ONTDEV_STATUS_AUTHENTICATED;
        }
        else
        {
            ONT_LOG_DEBUG1("Failed to authenticate: %d", result);
            return ONT_ERR_FAILED_TO_AUTH;
        }
        break;

    case ONT_JTEXT_MSG_HEARTBEAT:
    case ONT_JTEXT_MSG_UPLINK:
        if (0 == result)
        {
            ctx->status = ONT_JTEXT_STATUS_IDLE;
        }
        else
        {
            ONT_LOG_DEBUG1("Failed to send uplink message: %d", result);
            return ONT_ERR_INTERNAL;
        }
        break;

    default:
        break;
    }

    return ONT_ERR_OK;

}

static int ont_jtext_read_packet(void *arg, const char *buf,
                                 size_t size, size_t *read_bytes)
{
    int bytes, err;
    ont_jtext_t *ctx = (ont_jtext_t*)arg;
    uint16_t msg_id;

    *read_bytes = 0;
    bytes = ont_packet_unpack(ctx, buf, size);
    if (0 == bytes)
        return ONT_ERR_OK;

    if (bytes < 0)
        return bytes;

    *read_bytes = bytes;

    msg_id = ont_packet_get_msg_id(ctx->recv_pkt);
    switch(msg_id)
    {
    case ONT_JTEXT_MSG_REG_RESP:
        err = ont_jtext_handle_reg_resp(ctx);
        break;

    case ONT_JTEXT_MSG_PLAT_RESP:
        err = ont_jtext_handle_platresp(ctx);
        break;

    default:
        err = ONT_ERR_OK;
        break;
    }

    return err;
}

static void ont_jtext_clear_packets(ont_jtext_t *ctx)
{
    ont_packet_t *pkt = ctx->packets->next;

    while (pkt)
    {
        ont_packet_t *tmp = pkt;
        pkt = pkt->next;
        ont_platform_free(tmp);
    }

    ctx->packets->next = NULL;
    ctx->last_pkt = ctx->packets;
}

static int ont_jtext_register(ont_jtext_t *ctx)
{
    int err;
    ont_packet_t *pkt;

    err = ont_tcp_channel_create(ctx->channel, ctx->dev.ip, ctx->dev.port,
                                 ONT_SOCK_RECV_BUF_SIZE,
                                 ONT_SOCK_SEND_BUF_SIZE,
                                 ctx, ctx->dev.keepalive);
    if (ONT_ERR_OK != err)
        return err;

    err = ctx->channel->fn_initilize(ctx->channel->channel,
                                     ont_jtext_read_packet);
    if (ONT_ERR_OK != err)
        return err;


    err = ctx->channel->fn_connect(ctx->channel->channel, &ctx->dev.exit);
    if (ONT_ERR_OK != err)
        return err;

    err = ont_packet_pack_register_pkt(ctx);
    if (ONT_ERR_OK != err)
        return err;

    pkt = ctx->packets->next;
    err = ctx->channel->fn_write(ctx->channel->channel, pkt->pkt, pkt->size);

    while ((ONT_ERR_OK == err) && !ctx->dev.exit)
    {
        err = ctx->channel->fn_process(ctx->channel->channel);

        if (ctx->auth_code)
            break;

        ont_platform_sleep(40);
    }

    ont_jtext_clear_packets(ctx);
    ctx->channel->fn_deinitilize(ctx->channel->channel);

    return err;
}

static int ont_jtext_auth(ont_jtext_t *ctx)
{
    int err;
    ont_packet_t *pkt;

    err = ont_packet_pack_auth_pkt(ctx);
    if (ONT_ERR_OK != err)
        return err;

    pkt = ctx->packets->next;
    err = ctx->channel->fn_write(ctx->channel->channel, pkt->pkt, pkt->size);
    while ((ONT_ERR_OK == err) && !ctx->dev.exit)
    {
        err = ctx->channel->fn_process(ctx->channel->channel);

        if (ONTDEV_STATUS_AUTHENTICATED == ctx->dev.status)
            break;

        ont_platform_sleep(40);
    }

    ont_jtext_clear_packets(ctx);
    return err;
}

static int ont_jtext_send(ont_jtext_t *ctx)
{
    ont_packet_t *cursor;
    int err;

    cursor = ctx->packets->next;
    while (ONT_ERR_OK == err)
    {
        if ((NULL == cursor) && (ONT_JTEXT_STATUS_IDLE == ctx->status))
            break;

        if (ONT_JTEXT_STATUS_IDLE == ctx->status)
        {
            err = ctx->channel->fn_write(ctx->channel->channel, cursor->pkt, cursor->size);
            if (ONT_ERR_OK != err)
                break;

            cursor = cursor->next;
        }
        else if (ONT_JTEXT_STATUS_SENDING == ctx->status)
        {
            err = ctx->channel->fn_process(ctx->channel->channel);
            if (ONT_ERR_OK != err)
                break;
        }
    }
    return err;
}

int ont_jtext_create(ont_device_t **dev)
{
    ont_jtext_t *ctx;

    ctx = (ont_jtext_t*)ont_platform_malloc(sizeof(ont_jtext_t));
    if (NULL == ctx)
        return ONT_ERR_NOMEM;

    memset(ctx, 0, sizeof(ont_jtext_t));
    ctx->last_pkt = ctx->packets;
    ctx->status = ONT_JTEXT_STATUS_IDLE;

    *dev = &ctx->dev;
    return ONT_ERR_OK;
}

void ont_jtext_destroy(ont_device_t *dev)
{
    if (dev)
    {
        ont_jtext_t *ctx = (ont_jtext_t*)dev;
        ont_jtext_clear_packets(ctx);
        ont_platform_free(ctx->auth_code);
        ctx->channel->fn_deinitilize(ctx->channel->channel);
        ont_platform_free(ctx);
    }
}

int ont_jtext_connect(ont_device_t *dev, const char *auth_info)
{
    int err;
    ont_jtext_t *ctx = (ont_jtext_t*)dev;

    err = ont_tcp_channel_create(ctx->channel, dev->ip, dev->port,
                                 ONT_SOCK_RECV_BUF_SIZE,
                                 ONT_SOCK_SEND_BUF_SIZE,
                                 ctx, dev->keepalive);
    if (ONT_ERR_OK != err)
        return err;

    err = ctx->channel->fn_initilize(ctx->channel->channel,
                                     ont_jtext_read_packet);
    if (ONT_ERR_OK != err)
        return err;

    if (NULL == ctx->auth_code)
    {
        const char *ac;

        ONT_LOG_DEBUG0("Send 'REGISTER' message to the server...");
        err = ont_jtext_register(ctx);
        if (ONT_ERR_OK != err)
            return err;
        ac = ctx->auth_code;
        ONT_LOG_DEBUG1("REGISTER device successfully, AuthCode is '%s'", ac);
    }

    err = ont_tcp_channel_create(ctx->channel, dev->ip, dev->port,
                                 ONT_SOCK_RECV_BUF_SIZE,
                                 ONT_SOCK_SEND_BUF_SIZE,
                                 ctx, dev->keepalive);
    if (ONT_ERR_OK != err)
        return err;

    err = ctx->channel->fn_initilize(ctx->channel->channel,
                                     ont_jtext_read_packet);
    if (ONT_ERR_OK != err)
        return err;

    err = ctx->channel->fn_connect(ctx->channel->channel, &dev->exit);
    if (ONT_ERR_OK != err)
        return err;

    err = ont_jtext_auth(ctx);
    if (ONT_ERR_OK == err)
        ont_jtext_update_ping_time(ctx);
    return err;
}

int ont_jtext_send_datapoints(ont_device_t *dev,
                              const char *payload,
                              size_t bytes)
{
    int err;
    ont_jtext_t *ctx = (ont_jtext_t*)dev;

    err = ont_packet_pack_uplinkdata_pkt(ctx, (char)0xf0, payload, bytes);

    if (ONT_ERR_OK != err)
        return err;

    err = ont_jtext_send(ctx);

    ont_jtext_clear_packets(ctx);
    ont_jtext_update_ping_time(ctx);
    return err;
}

ont_device_cmd_t *ont_jtext_get_cmd(ont_device_t *dev)
{
    (void)dev;
    return NULL;
}

int ont_jtext_reply_cmd(ont_device_t *dev, const char *cmdid,
                        const char *resp, size_t bytes)
{
    (void)dev;
    (void)cmdid;
    (void)resp;
    (void)bytes;
    return ONT_ERR_OK;
}

int ont_jtext_keepalive(ont_device_t *dev)
{
    ont_jtext_t *ctx = (ont_jtext_t*)dev;
    int err;

    err = ctx->channel->fn_process(ctx->channel->channel);
    if (ONT_ERR_OK != err)
        return err;

    if (ont_platform_time() >= ctx->next_ping_time)
    {
        err = ont_packet_pack_heartbeat_pkt(ctx);
        if (ONT_ERR_OK != err)
            return err;

        err = ont_jtext_send(ctx);
        if (ONT_ERR_OK != err)
            return  err;

        ont_jtext_update_ping_time(ctx);
    }

    return ONT_ERR_OK;
}

#endif /* ONT_PROTOCOL_JTEXT */

