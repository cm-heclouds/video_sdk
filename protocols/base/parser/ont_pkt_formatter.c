#include <string.h>
#include "parser.h"
#include "ont_pkt_formatter.h"
#include "ont/platform.h"
#include "ont/error.h"

#define DEF_DPS_SIZE 64

ont_parser_init_begin(fix_header_t, type, uint8)
ont_parser_queue_up(remain_len, compress_uint32, NULL, none)
ont_parser_init_end()

static int ont_pkt_time_filter(ont_parser_list_node_t* node)
{
    ont_parser_uint8_t* p = (ont_parser_uint8_t*)node;
    if (p->value != 0)
    {
        return 0;
    }

    return 1;
}

ont_parser_init_begin(formatter_time_t, type, uint8)
ont_parser_queue_up(tm, uint32, ont_pkt_time_filter, type);
ont_parser_init_end()

ont_parser_init_begin(formatter_uint32_t, type, uint8)
ont_parser_queue_up(ds, string, NULL, none);
ont_parser_queue_up(value, uint32, NULL, none);
ont_parser_init_end()

#ifdef ONT_PLATFORM_FLOATING_POINT_SUPPORT
ont_parser_init_begin(formatter_double_t, type, uint8)
ont_parser_queue_up(ds, string, NULL, none);
ont_parser_queue_up(value, double, NULL, none);
ont_parser_init_end()
#endif /* ONT_PLATFORM_FLOATING_POINT_SUPPORT */

ont_parser_init_begin(formatter_string_t, type, uint8)
ont_parser_queue_up(ds, string, NULL, none);
ont_parser_queue_up(value, string, NULL, none);
ont_parser_init_end()

ont_parser_init_begin(formatter_binary_t, type, uint8)
ont_parser_queue_up(ds, string, NULL, none);
ont_parser_queue_up(value, binary, NULL, none);
ont_parser_init_end()


ont_parser_init_begin(formatter_json_t, type, uint8)
ont_parser_queue_up(ds, string, NULL, none);
ont_parser_queue_up(value, string, NULL, none);
ont_parser_init_end()

int ont_pkt_formatter_init(ont_pkt_formatter_t* formatter)
{
    int rc = 0;

    rc = ont_parser_init_fix_header_t(&formatter->header);
    ONT_EXPECT_ERR_OK(rc);

    formatter->buf = NULL;
    ont_pkt_formatter_reset(formatter, 1);

    formatter->cap = DEF_DPS_SIZE;
    formatter->buf = (char*)ont_platform_malloc(formatter->cap);
    if (!formatter->buf)
    {
        return ONT_ERR_NOMEM;
    }
    memset(formatter->buf, 0, formatter->cap);

    return ONT_ERR_OK;
}

int ont_pkt_formatter_realloc(ont_pkt_formatter_t* formatter, size_t len)
{
    char* tmp = NULL;

    if (formatter->cap < len)
    {
        tmp = (char*)ont_platform_malloc(len);
        if (!tmp)
        {
            return ONT_ERR_NOMEM;
        }

        memset(tmp, 0, len);
        if (formatter->buf)
        {
            memcpy(tmp, formatter->buf, formatter->offset);
            ont_platform_free(formatter->buf);
        }

        formatter->cap = len;
        formatter->buf = tmp;
        tmp = NULL;
    }

    return ONT_ERR_OK;
}

void ont_pkt_formatter_reset(ont_pkt_formatter_t* formatter, int8_t free_mem)
{
    formatter->header.type.value = 0;
    formatter->header.remain_len.value = 0;
    formatter->offset = 0;
    formatter->dp8.first_dp = 1;
    formatter->dp8.count = 0;
    formatter->dp8.counter_offset = 0;
    formatter->dp8.tm = 0;
    formatter->result.data = NULL;
    formatter->result.len = 0;

    if (free_mem)
    {
        if (formatter->buf)
        {
            ont_platform_free(formatter->buf);
        }
        formatter->buf = NULL;
        formatter->cap = 0;
    }
    else{
        if (formatter->cap && formatter->buf)
        {
            memset(formatter->buf, 0, formatter->cap);
        }
    }
}

static int ont_pkt_formatter_add_dp8_header(ont_pkt_formatter_t* formatter)
{
    formatter_time_t pkt;
    int rc = 0;
    size_t len = 0;

    if (formatter->dp8.first_dp)
    {
        rc = ont_pkt_formatter_realloc(formatter, 10);
        ONT_EXPECT_ERR_OK(rc);

        *(formatter->buf+formatter->offset) = 0x08;
        ++formatter->offset;

        rc = ont_parser_init_formatter_time_t(&pkt);
        ONT_EXPECT_ERR_OK(rc);

        pkt.type.value = (formatter->dp8.tm == 0) ? 0 : 1;
        pkt.tm.value = (uint32_t)formatter->dp8.tm;

        rc = ont_parser_serialize(formatter->buf + formatter->offset,
                                  formatter->cap - formatter->offset, &pkt.head);
        ONT_EXPECT_ERR_OK(rc);
        rc = ont_parser_len(&pkt.head, &len);
        ONT_EXPECT_ERR_OK(rc);
        formatter->dp8.counter_offset = formatter->offset + len;
        formatter->offset = formatter->dp8.counter_offset + 1;  /* for formatter->count */
        *(formatter->buf+formatter->dp8.counter_offset) = 0;
        formatter->dp8.first_dp = 0;
    }

    return ONT_ERR_OK;
}

#define def_ont_pkt_formatter_add_func(pt, tv, ext)                     \
    int ont_pkt_formatter_add_##ext(ont_pkt_formatter_t* formatter,     \
                                    const char* ds, pt value){          \
        formatter_##ext##_t pkt;                                        \
        uint8_t* count = NULL;                                          \
        int rc = 0;                                                     \
        size_t len = 0;                                                 \
                                                                        \
        rc = ont_pkt_formatter_add_dp8_header(formatter);               \
        ONT_EXPECT_ERR_OK(rc);                                          \
        count = (uint8_t*)(formatter->buf +                             \
                           formatter->dp8.counter_offset);              \
        if (*count == 0xFF){                                            \
            return ONT_ERR_TOO_MANY_DATAPOINTS;                         \
        }                                                               \
        ont_parser_init_formatter_##ext##_t(&pkt);                      \
        pkt.type.value = tv;                                            \
        pkt.ds.value = ds;                                              \
        pkt.value.value = value;                                        \
        rc = ont_parser_len(&pkt.head, &len);                           \
        ONT_EXPECT_ERR_OK(rc);                                          \
        rc = ont_pkt_formatter_realloc(formatter,                       \
                                       formatter->offset + len);        \
        ONT_EXPECT_ERR_OK(rc);                                          \
        rc = ont_parser_serialize(formatter->buf + formatter->offset,   \
                                  formatter->cap -                      \
                                  formatter->offset, &pkt.head);        \
        ONT_EXPECT_ERR_OK(rc);                                          \
        rc = ont_parser_len(&pkt.head, &len);                           \
        ONT_EXPECT_ERR_OK(rc);                                          \
        formatter->offset += len;                                       \
        count = (uint8_t*)(formatter->buf +                             \
                           formatter->dp8.counter_offset);              \
        ++(*count);                                                     \
        formatter->result.data = formatter->buf;                        \
        formatter->result.len = formatter->offset;                      \
        return ONT_ERR_OK;                                              \
    }

def_ont_pkt_formatter_add_func(uint32_t, 1, uint32)
#ifdef ONT_PLATFORM_FLOATING_POINT_SUPPORT
def_ont_pkt_formatter_add_func(double, 2, double)
#endif /* ONT_PLATFORM_FLOATING_POINT_SUPPORT */
def_ont_pkt_formatter_add_func(const char*, 3, string)
def_ont_pkt_formatter_add_func(const char*, 5, json)
#undef def_ont_pkt_formatter_add_func

int ont_pkt_formatter_add_binary(ont_pkt_formatter_t* formatter, const char* ds, const char* value, uint32_t len)
{
    formatter_binary_t pkt;
    uint8_t* count = NULL;
    size_t val_len = 0;
    int rc = 0;

    rc = ont_pkt_formatter_add_dp8_header(formatter);
    ONT_EXPECT_ERR_OK(rc);

    count = (uint8_t*)(formatter->buf + formatter->dp8.counter_offset);
    if (*count == 0xFF)
    {
        return ONT_ERR_TOO_MANY_DATAPOINTS;
    }

    rc = ont_parser_init_formatter_binary_t(&pkt);
    ONT_EXPECT_ERR_OK(rc);

    pkt.type.value = 4;
    pkt.ds.value = ds;
    pkt.value.value = value;
    pkt.value.len = len;

    rc = ont_parser_len(&pkt.head, &val_len);
    ONT_EXPECT_ERR_OK(rc);
    rc = ont_pkt_formatter_realloc(formatter, formatter->offset + val_len);
    ONT_EXPECT_ERR_OK(rc);

    rc = ont_parser_serialize(formatter->buf + formatter->offset, formatter->cap - formatter->offset, &pkt.head);
    ONT_EXPECT_ERR_OK(rc);

    rc = ont_parser_len(&pkt.head, &val_len);
    ONT_EXPECT_ERR_OK(rc);
    formatter->offset += val_len;
    count = (uint8_t*)(formatter->buf + formatter->dp8.counter_offset);
    ++(*count);

    formatter->result.data = formatter->buf;
    formatter->result.len = formatter->offset;
    return ONT_ERR_OK;
}

int ont_pkt_formatter_destroy(ont_pkt_formatter_t* formatter)
{
    ont_pkt_formatter_reset(formatter, 1);
    return ONT_ERR_OK;
}

int ont_pkt_serialize_finish(ont_pkt_formatter_t* formatter, uint8_t type, ont_parser_helper_t* head)
{
    size_t len = 0;
    int rc = 0;

    ont_pkt_formatter_reset(formatter, 0);
    rc = ont_parser_len(head, &len);
    ONT_EXPECT_ERR_OK(rc);
    rc = ont_pkt_formatter_realloc(formatter, 5+len);
    ONT_EXPECT_ERR_OK(rc);

    formatter->offset = 5;
    rc = ont_parser_serialize(formatter->buf + formatter->offset,
                              formatter->cap - formatter->offset, head);
    ONT_EXPECT_ERR_OK(rc);
    
    formatter->offset += len;
    formatter->header.type.value = type;
    return ont_pkt_formatter_finish(formatter);
}

int ont_pkt_formatter_finish(ont_pkt_formatter_t* formatter)
{
    char* buf = (char*)formatter->buf;
    size_t index = 0;
    int rc = 0;

    formatter->header.remain_len.value = formatter->offset - 5;
    rc = ont_parser_len(&formatter->header.head, &index);
    ONT_EXPECT_ERR_OK(rc);
    rc = ont_parser_serialize(buf + 5 - index, index, &formatter->header.head);
    ONT_EXPECT_ERR_OK(rc);

    formatter->result.data = buf + 5 - index;
    formatter->result.len = formatter->offset - 5 + index;
    return ONT_ERR_OK;
}
