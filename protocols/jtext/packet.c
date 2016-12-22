#include "packet.h"
#include <string.h>
#include "types.h"


#ifdef ONT_PROTOCOL_JTEXT

static size_t ont_packet_pack_header(ont_jtext_t *ctx, uint16_t msg_id,
                                     uint16_t subpkt_sn, uint16_t subpkt_count,
                                     uint16_t body_len, char *header)
{
    uint16_t property;
    uint16_t sn;

    char *cursor = header;

    /* message id. */
    *(cursor++) = (char)(msg_id >> 8);
    *(cursor++) = (char)msg_id;

    /* property */
    property = body_len & 0x03FF;
    if (subpkt_count > 1)
        property |= 0x2000;
    *(cursor++) = (char)(property >> 8);
    *(cursor++) = (char)property;

    memcpy(cursor, ctx->phone_number, 6);
    cursor += 6;

    sn = ctx->serial_number++;
    *(cursor++) = (char)(sn >> 8);
    *(cursor++) = (char)sn;

    if (subpkt_count > 1)
    {
        (*cursor++) = (char)(subpkt_count >> 8);
        (*cursor++) = (char)subpkt_count;

        (*cursor++) = (char)(subpkt_sn >> 8);
        (*cursor++) = (char)subpkt_sn;
    }

    return cursor - header;
}

static char ont_packet_calc_check_code(const char *first, size_t size, char code)
{
    const char *cursor;
    const char *end = first + size;

    for (cursor = first; cursor != end; ++cursor)
        code ^= *cursor;
    return code;
}

static size_t ont_packet_transform(const char *first, size_t size, char *out)
{
    char *cursor = out;
    const char *end = first + size;

    for(; first != end; ++first)
    {
        if(0x7e != *first)
        {
            if (out)
                *cursor = *first;
            ++cursor;

            if(0x7d == *first)
            {
                if (out)
                    *cursor = 0x01;
                ++cursor;
            }
        }
        else
        {
            if (out)
            {
                *(cursor++) = 0x7d;
                *(cursor++) = 0x02;
            }
            else
            {
                cursor += 2;
            }
        }
    }

    return cursor - out;
}

static int ont_packet_recover(const char *data, size_t size, char *buf)
{
    const char *end = data + size;
    char *out = buf;
    const char *p;

    for (p = data; p != end; ++p)
    {
        if(0x7d != *p)
            *(out++) = *p;
        else
        {
            if(++p == end)
                return -1;

            if(0x01 == *p)
                *(out++) = 0x7d;
            else if(0x02 == *p)
                *(out++) = 0x7e;
            else
                return -1;
        }
    }

    return (int)(out - buf);
}

static size_t ont_packet_calc_len(const char *header, size_t header_len,
                                  ont_extent_t *bodies, size_t count)
{
    char check_code;
    size_t len, i;

    len = 2; /* for two tags '0x7e' */
    check_code = 0;

    len += ont_packet_transform(header, header_len, NULL);
    check_code = ont_packet_calc_check_code(header, header_len, check_code);

    for (i = 0; i < count; ++i)
    {
        len += ont_packet_transform(bodies[i].base, bodies[i].size, NULL);
        check_code = ont_packet_calc_check_code(bodies[i].base,
                                                bodies[i].size,
                                                check_code);
    }

    len += ont_packet_transform(&check_code, 1, NULL);

    return len;
}

static int ont_packet_pack(ont_jtext_t *ctx,  uint16_t msg_id,
                           uint16_t subpkt_sn, uint16_t subpkt_count,
                           ont_extent_t *bodies, size_t count)
{
    char header[16];
    size_t header_len;
    char *cursor;
    char check_code;
    ont_packet_t *pkt;
    size_t pkt_size, i, body_len;

    body_len = 0;
    for (i = 0; i < count; ++i)
    {
        body_len += bodies[i].size;
    }

    header_len = ont_packet_pack_header(ctx, msg_id, subpkt_sn, subpkt_count,
                                        body_len, header);

    pkt_size = ont_packet_calc_len(header, header_len, bodies, count);

    pkt = (ont_packet_t*)ont_platform_malloc(sizeof(ont_packet_t) + pkt_size);
    if (NULL == pkt)
        return ONT_ERR_NOMEM;

    pkt->pkt = (char*)pkt + sizeof(ont_packet_t);
    pkt->size = pkt_size;
    pkt->next = NULL;
    ctx->last_pkt->next = pkt;
    ctx->last_pkt = pkt;

    cursor = pkt->pkt;
    check_code = 0;

    *(cursor++) = 0x7e;

    cursor += ont_packet_transform(header, header_len, cursor);
    check_code = ont_packet_calc_check_code(header, header_len, check_code);

    for (i = 0; i < count; ++i)
    {
        cursor += ont_packet_transform(bodies[i].base, bodies[i].size, cursor);
        check_code = ont_packet_calc_check_code(bodies[i].base,
                                                bodies[i].size,
                                                check_code);
    }

    cursor += ont_packet_transform(&check_code, 1, cursor);

    *(cursor++) = 0x7e;
    return ONT_ERR_OK;
}


int ont_packet_pack_register_pkt(ont_jtext_t *ctx)
{
    char msg[37];
    char *cursor = msg;
    ont_extent_t body;

    /* province */
    *(cursor++) = 0;
    *(cursor++) = 0;

    /* city */
    *(cursor++) = 0;
    *(cursor++) = 0;

    /* manufacture */
    *(cursor++) = 0;
    *(cursor++) = (char)(ctx->dev.product_id >> 24);
    *(cursor++) = (char)(ctx->dev.product_id >> 16);
    *(cursor++) = (char)(ctx->dev.product_id >> 8);
    *(cursor++) = (char)(ctx->dev.product_id);

    memcpy(cursor, ctx->terminal_type, 20);
    cursor += 20;

    memcpy(cursor, ctx->terminal_id, 7);
    cursor += 7;

    /* color */
    *(cursor++) = 0;

    body.base = msg;
    body.size = sizeof(msg);
    return ont_packet_pack(ctx, ONT_JTEXT_MSG_REGISTER, 1, 1, &body, 1);
}

int ont_packet_pack_auth_pkt(ont_jtext_t *ctx)
{
    ont_extent_t body;
    body.base = ctx->auth_code;
    body.size = strlen(ctx->auth_code);

    return ont_packet_pack(ctx, ONT_JTEXT_MSG_AUTH, 1, 1, &body, 1);
}

int ont_packet_pack_heartbeat_pkt(ont_jtext_t *ctx)
{
    return ont_packet_pack(ctx, ONT_JTEXT_MSG_HEARTBEAT, 1, 1,
                           NULL, 0);
}

int ont_packet_pack_uplinkdata_pkt(ont_jtext_t *ctx, char type,
                                   const char *data, size_t size)
{
    size_t body_size = size + 1;
    size_t pkt_count = body_size / ONT_JTEXT_MAX_PKT_BODY_SIZE +
        (body_size % ONT_JTEXT_MAX_PKT_BODY_SIZE ? 1 : 0);
    size_t i;
    int err;
    ont_extent_t bodies[2];


    bodies[0].base = &type;
    bodies[0].size = 1;

    bodies[1].base = (char*)data;
    bodies[1].size = size % ONT_JTEXT_MAX_PKT_BODY_SIZE;

    err = ont_packet_pack(ctx, ONT_JTEXT_MSG_UPLINK, 1,
                          (uint16_t)pkt_count, bodies, 2);
    if (ONT_ERR_OK != err)
        return err;

    if (pkt_count > 1)
    {
        data += bodies[1].size;

        for (i = 2; i < pkt_count; ++i)
        {
            bodies[0].base = (char*)data;
            bodies[0].size = ONT_JTEXT_MAX_PKT_BODY_SIZE;

            err = ont_packet_pack(ctx, ONT_JTEXT_MSG_UPLINK,
                                  i, (uint16_t)pkt_count,
                                  bodies, 1);
            if (ONT_ERR_OK != err)
                return err;

            data += ONT_JTEXT_MAX_PKT_BODY_SIZE;
        }

        bodies[0].base = (char*)data;
        bodies[0].size = size;

        err = ont_packet_pack(ctx, ONT_JTEXT_MSG_UPLINK, i, (uint16_t)pkt_count,
                              bodies, 1);
    }
    return err;

}

int ont_packet_unpack(ont_jtext_t *ctx, const char *pkt, size_t size)
{
    uint16_t len;
    char check_code;
    const char *body;
    uint16_t body_len;

    if (0x7e != pkt[0])
        return ONT_ERR_PROTOCOL;

    for (len = 2; len <= size; ++len)
    {
        if (0x7e == pkt[len - 1])
            break;

        if (len > ONT_JTEXT_MAX_PKT_SIZE)
            return ONT_ERR_PROTOCOL;
    }

    if (len > size)
        return 0;

    ctx->recv_pkt_len = ont_packet_recover(pkt + 1, len - 2, ctx->recv_pkt);
    if (ctx->recv_pkt_len < 0)
        return ONT_ERR_PROTOCOL;


    check_code = ont_packet_calc_check_code(ctx->recv_pkt, ctx->recv_pkt_len - 1, 0);
    if (check_code != ctx->recv_pkt[ctx->recv_pkt_len - 1])
        return ONT_ERR_PROTOCOL;

    ctx->recv_pkt_len -= 1; /* except check code. */

    body = ont_packet_get_body(ctx->recv_pkt);
    body_len = ont_packet_get_body_len(ctx->recv_pkt);
    if (body + body_len != ctx->recv_pkt + ctx->recv_pkt_len)
        return ONT_ERR_PROTOCOL;

    return len;
}

int ont_packet_parse_msg_regresp(const char *body, uint16_t size,
                                 uint16_t *sn, char *result,
                                 const char **auth_code, uint16_t *auth_code_len)
{
    if (size < 3)
        return ONT_ERR_PROTOCOL;

    *sn = ((uint16_t)(body[0]) << 8) | body[1];
    *result = body[2];

    *auth_code = body + 3;
    *auth_code_len = size - 3;
    return ONT_ERR_OK;
}

int ont_packet_parse_msg_platresp(const char *body, uint16_t size,
                                  uint16_t *sn, uint16_t *msg_id, char *result)
{
    if (5 != size)
        return ONT_ERR_PROTOCOL;

    *sn = ((uint16_t)(body[0]) << 8) | body[1];
    *msg_id = ((uint16_t)(body[2]) << 8) | body[3];
    *result = body[4];

    return ONT_ERR_OK;
}

#endif /* ONT_PROTOCOL_JTEXT */
