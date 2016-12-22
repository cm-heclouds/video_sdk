#include "ont/device.h"
#include <string.h>
#include <ont/error.h>
#include <ont/log.h>
#include "ont_channel.h"
#include "ont_tcp_channel.h"
#include "parser/ont_pkt_formatter.h"

#ifdef ONT_PROTOCOL_EDP
#include "ont_edp.h"
#endif

#ifdef ONT_PROTOCOL_JTEXT
#include "ont_jtext.h"
#endif

#ifdef ONT_PROTOCOL_MODBUS
#include "ont_modbus.h"
#endif

#ifdef ONT_PROTOCOL_MQTT
#include "ont_mqtt.h"
#endif


static const uint16_t HTTP_TIMEOUT = 30;
static const char ONT_DEVICE_VERSION = 1;

const ont_device_rt_t *ont_device_get_rt(ont_device_type_t type)
{
#ifdef ONT_PROTOCOL_EDP
    if (ONTDEV_EDP == type)
    {
        static ont_device_rt_t edp_rt = {
            ont_edp_create,
            ont_edp_destroy,
            ont_edp_connect,
            ont_edp_send_datapoints,
            ont_edp_get_cmd,
            ont_edp_reply_cmd,
            ont_edp_keepalive
        };
        return &edp_rt;
    }
#endif /* ONT_PROTOCOL_EDP */

#ifdef ONT_PROTOCOL_JTEXT
    if (ONTDEV_JTEXT == type)
    {
        static ont_device_rt_t jtext_rt = {
            ont_jtext_create,
            ont_jtext_destroy,
            ont_jtext_connect,
            ont_jtext_send_datapoints,
            ont_jtext_get_cmd,
            ont_jtext_reply_cmd,
            ont_jtext_keepalive
        };
        return &jtext_rt;
    }
#endif /* ONT_PROTOCOL_JTEXT */

#ifdef ONT_PROTOCOL_MODBUS
    if (ONTDEV_MODBUS == type)
    {
        static ont_device_rt_t modbus_rt = {
            ont_modbus_create,
            ont_modbus_destroy,
            ont_modbus_connect,
            ont_modbus_send_datapoints,
            ont_modbus_get_cmd,
            ont_modbus_reply_cmd,
            ont_modbus_keepalive
        };
        return &modbus_rt;
    }
#endif /* ONT_PROTOCOL_MODBUS*/

#ifdef ONT_PROTOCOL_MQTT
    if (ONTDEV_MQTT == type)
    {
        static ont_device_rt_t mqtt_rt = {
            ont_mqtt_create,
            ont_mqtt_destroy,
            ont_mqtt_connect,
            ont_mqtt_send_datapoints,
            ont_mqtt_get_cmd,
            ont_mqtt_reply_cmd,
            ont_mqtt_keepalive
        };
        return &mqtt_rt;
    }
#endif /* ONT_PROTOCOL_MQTT */

    return NULL;
}

#define ont_device_status_transform(dev, s) do {                        \
        ONT_LOG_DEBUG2("Device status transform: %d  ->  %d", dev->status, s); \
        dev->status = s;                                                \
    } while(0)

static int ont_device_parse_addr(const char *first, const char *end,
                                 char *host, uint16_t *port)
{
    const char *cursor;
    const char *port_pos;
    const char seperator = ':';
    int seperator_found = 0;

    cursor = first;
    for (cursor = first; *cursor && (cursor < end); ++cursor)
    {
        if (seperator == *cursor)
        {
            ++seperator_found;
            port_pos = cursor + 1;
            break;
        }
    }

    if ((1 != seperator_found) || ('\0' == *port_pos))
        return ONT_ERR_BADPARAM;

    memcpy(host, first, port_pos - first - 1);
    host[port_pos - first - 1] = '\0';

    *port = 0;
    for (cursor = port_pos; *cursor && (cursor < end); ++cursor)
    {
        uint16_t tmp = *port;

        if ((*cursor < '0') || (*cursor > '9'))
            return ONT_ERR_BADPARAM;

        *port *= 10;
        *port += *cursor - '0';
        if (tmp > *port) /* overflow */
            return ONT_ERR_BADPARAM;
    }

    return ONT_ERR_OK;
}

/*
 * return non-zero if get line successfully.
 */
static int http_get_line(const char *first, const char *last, const char **line_last)
{
    const char *cursor;

    for (cursor = first; cursor < last; ++cursor)
    {
        if (('\r' == *cursor) && ('\n' == *(cursor + 1)))
        {
            *line_last = cursor;
            return 1;
        }
    }

    return 0;
}

static int http_get_content_len(const char *first, const char *end, size_t *len)
{
    const char *cursor;

    for (cursor = first; cursor < end; ++cursor)
    {
        if (':' == *cursor)
        {
            const char *key = "Content-Length";
            if (memcmp(key, first, strlen(key)) != 0)
                return 0;

            ++cursor;
            if (' ' == *(cursor))
                ++cursor;

            *len = 0;
            for (; cursor < end; ++cursor)
            {
                size_t old = *len;

                if ((*cursor < '0') || (*cursor > '9'))
                    return -1;

                *len *= 10;
                *len += *cursor - '0';
                if (*len < old) /* overflow */
                    return -1;
            }

            return 1;
        }
    }

    return 0;
}

static int ont_device_find_key(const char *s,
                               const char **key_first,
                               const char **key_end)
{
    const char *cursor = s;

    while ('"' != *cursor)
    {
        if ('\0' == *cursor)
            return -1;

        ++cursor;
    }

    *key_first = ++cursor;

    while ('"' != *cursor)
    {
        if ('\0' == *cursor)
            return -1;

        ++cursor;
    }
    *key_end = cursor++;

    return (int)(cursor - s);
}

/**
 * Just parse the response simply, ingore the JSON format.
 */
static int ont_device_handle_register_response(ont_device_t *dev,
                                               const char *resp,
                                               size_t size)
{
    const char *cursor;
    const char *key_first;
    const char *key_end;
    int skip;

    if ((size < 1) || ('{' != resp[0]))
        return ONT_ERR_FAILED_TO_REGISTER;

    cursor = resp + 1;

    while (*cursor)
    {
        skip = ont_device_find_key(cursor, &key_first, &key_end);
        if (skip < 0)
            return ONT_ERR_FAILED_TO_REGISTER;

        cursor += skip;
        /* data segment found. */
        if ((key_end - key_first == 4) && (memcmp(key_first, "data", 4) == 0))
        {
            while (*cursor && (!dev->key || (0 == dev->device_id)))
            {
                skip = ont_device_find_key(cursor, &key_first, &key_end);
                if (skip < 0)
                    return ONT_ERR_FAILED_TO_REGISTER;

                cursor += skip;
                if ((key_end - key_first == 3) && (memcmp(key_first, "key", 3) == 0))
                {
                    skip = ont_device_find_key(cursor, &key_first, &key_end);
                    if (skip < 0)
                        return ONT_ERR_FAILED_TO_REGISTER;

                    cursor += skip;
                    dev->key = (char*)ont_platform_malloc(key_end - key_first + 1);
                    if (!dev->key)
                        break;
                    memcpy(dev->key, key_first, key_end - key_first);
                    dev->key[key_end - key_first] = '\0';
                }
                else if ((key_end - key_first == 9) &&
                         (memcmp(key_first, "device_id", 9) == 0))
                {
                    for (; *cursor && (',' != *cursor) && ('}' != *cursor); ++cursor)
                    {
                        uint32_t old = dev->device_id;

                        if ((*cursor < '0') || (*cursor > '9'))
                            continue;
                        dev->device_id *= 10;
                        dev->device_id += *cursor - '0';
                        if (dev->device_id < old) { /* overflow */
                            dev->device_id = 0;
                            break;
                        }
                    }
                }
            }

            break;
        }
    }

    if (!dev->key || (0 == dev->device_id))
        return ONT_ERR_FAILED_TO_REGISTER;

    return ONT_ERR_OK;
}

static int ont_device_handle_retrieve_acceptor_response(ont_device_t *dev,
                                                        const char *resp,
                                                        size_t size)
{
    int err;

    /* address format: "xxx.xxx.xxx.xxx:ppppp" */
    if (size > sizeof(dev->ip) + 5)
    {
        ONT_LOG1(ONTLL_FATAL, "Invalid address recieved from "
                 "bootstrap API: length=%d", (int)size);
        return ONT_ERR_INTERNAL;
    }

    err = ont_device_parse_addr(resp, resp + size, dev->ip, &dev->port);
    if (ONT_ERR_OK == err)
        ont_device_status_transform(dev, ONTDEV_STATUS_RETRIEVED_ACCEPTOR);

    return err;
}

static int ont_device_handle_http_response(void *ctx, const char *buf,
                                           size_t buf_size, size_t *read_size)
{
    ont_device_t *dev = (ont_device_t*)ctx;
    const char *end = buf + buf_size;
    const char *cursor;
    size_t content_len = 0;
    int err;

    cursor = buf;

    *read_size = 0;
    while (1)
    {
        const char *line_first, *line_last;
        line_first = cursor;
        if (!http_get_line(cursor, end, &line_last))
            return ONT_ERR_OK;

        if (line_last == cursor)
        {
            cursor += 2; /* skip \r\n */
            break;
        }

        if ((*line_first < 'A') && (*line_first > 'Z'))
            return ONT_ERR_PROTOCOL;

        if (http_get_content_len(line_first, line_last, &content_len) < 0)
            return ONT_ERR_PROTOCOL;


        cursor = line_last + 2; /* skip "\r\n" */
    }

    if (!content_len || (content_len + cursor > end))
        return ONT_ERR_OK;

    switch (dev->status)
    {
    case ONTDEV_STATUS_REGISTERING:
        err = ont_device_handle_register_response(dev, cursor, content_len);
        break;

    case ONTDEV_STATUS_RETRIEVING_ACCEPTOR:
        err = ont_device_handle_retrieve_acceptor_response(dev, cursor, content_len);
        break;

    default:
        err = ONT_ERR_INTERNAL;
    }

    *read_size = cursor + content_len - buf;

    return  err;
}

static int ont_device_register(ont_device_t *dev,
                               const char *reg_code,
                               const char *auth_info,
                               const ont_device_rt_t *rt)
{
    int err;
    size_t head_min_len, host_len, rgc_len, sn_len, head_len;
    size_t body_min_len, http_body_len, buf_size, name_len;
    char *http_req;
    ont_channel_interface_t channel[1];

    const char *http_req_head =
        "POST /register_de?register_code=%s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n";

    const char *body =
        "{"
            "\"sn\":\"%s\","
            "\"title\":\"%s\""
        "}";


    err = ont_tcp_channel_create(channel, dev->ip, dev->port,
                                 ONT_SOCK_RECV_BUF_SIZE,
                                 ONT_SOCK_SEND_BUF_SIZE,
                                 dev, HTTP_TIMEOUT);
    if (ONT_ERR_OK != err)
        return err;

    err = channel->fn_initilize(channel->channel,
                                ont_device_handle_http_response);
    if (ONT_ERR_OK != err)
        return err;

    err = channel->fn_connect(channel->channel, &dev->exit);
    if (ONT_ERR_OK != err)
        return err;

    ONT_LOG_DEBUG2("Connect to the server '%s:%u' successfully.",
                   dev->ip, dev->port);

    head_min_len = strlen(http_req_head) - 3 * 2;
    host_len = strlen(dev->ip);
    rgc_len = strlen(reg_code);

    name_len = strlen(dev->name);
    sn_len = strlen(auth_info);
    body_min_len = strlen(body) - 2 * 2;

    http_body_len = sn_len + name_len + body_min_len;

    /* 5 for 4 bytes number and 1 byte terminator. */
    buf_size = head_min_len + host_len + rgc_len + http_body_len + 5;
    http_req = (char*)ont_platform_malloc(buf_size);

    ont_platform_snprintf(http_req, buf_size, http_req_head, reg_code,
                          dev->ip, http_body_len);
    head_len = strlen(http_req);
    ont_platform_snprintf(http_req + head_len, buf_size - head_len ,
                          body, auth_info, dev->name);

    err = channel->fn_write(channel->channel, http_req, strlen(http_req));
    ont_platform_free(http_req);
    if (ONT_ERR_OK != err)
        return err;

    while (1)
    {
        err = channel->fn_process(channel->channel);
        if (ONT_ERR_OK != err)
            break;

        if (0 != dev->device_id)
            break;

        ont_platform_sleep(30);
    }

    channel->fn_deinitilize(channel->channel);
    return err;
}

static int ont_device_retrieve_server(ont_device_t *dev,
                                      const char *server,
                                      uint16_t port)
{
    char *http_req;
    size_t len;
    int err;
    ont_channel_interface_t channel[1];
    const char *http_req_head =
        "GET /s?t=%d HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "\r\n";


    /* 4 bytes for "%d" "%s" and 2 bytes for type. */
    len = strlen(http_req_head) - 4 + strlen(server) + 2;
    http_req = (char*)ont_platform_malloc(len + 1);
    if (!http_req)
        return ONT_ERR_NOMEM;

    ont_platform_snprintf(http_req, len + 1, http_req_head, dev->type, server);
    do
    {
        err = ont_tcp_channel_create(channel, dev->ip, dev->port,
                                     ONT_SOCK_RECV_BUF_SIZE,
                                     ONT_SOCK_SEND_BUF_SIZE,
                                     dev, HTTP_TIMEOUT);
        
        if (ONT_ERR_OK != err)
            break;

        err = channel->fn_initilize(channel->channel,
                                    ont_device_handle_http_response);
        if (ONT_ERR_OK != err)
            break;

        err = channel->fn_connect(channel->channel, &dev->exit);
        if (ONT_ERR_OK != err)
            break;

        err = channel->fn_write(channel->channel, http_req, strlen(http_req));
        if (ONT_ERR_OK != err)
            break;

        while (1)
        {
            err = channel->fn_process(channel->channel);
            if (ONT_ERR_OK != err)
                break;

            if (ONTDEV_STATUS_RETRIEVED_ACCEPTOR == dev->status)
                break;

            ont_platform_sleep(30);
        }
    }while(0);
    ont_platform_free(http_req);
    channel->fn_deinitilize(channel->channel);
    
    return err;
}

static int ont_device_save_status(ont_device_t *dev)
{
    int err = ONT_ERR_OK;

#ifdef ONT_PLATFORM_PERSISTENCE_SUPPORT
    /*
     * file format:
     *   | total len(2 bytes) |
     *   | version(1 bytes)   |
     *   | device id(4 bytes) |
     *   | key len(2 bytes)   |
     *   | key                |
     */
    uint16_t total_len, key_len;
    char *status, *cursor;
    const uint16_t fix_size = 2 + 1 + 4 + 2;

    key_len = (uint16_t)strlen(dev->key);
    total_len = key_len + fix_size;
    status = (char*)ont_platform_malloc(total_len);
    if (!status)
        return ONT_ERR_NOMEM;

    cursor = status;

    memcpy(cursor, &total_len, 2);
    cursor += 2;

    *(cursor++) = ONT_DEVICE_VERSION;

    memcpy(cursor, &dev->device_id, 4);
    cursor += 4;

    memcpy(cursor, &key_len, 2);
    cursor += 2;

    memcpy(cursor, dev->key, key_len);
    err = ont_platform_save_device_status(status, total_len);
    ont_platform_free(status);
#endif /* ONT_PLATFORM_PERSISTENCE_SUPPORT */
    return err;
}

static int ont_device_load_status(ont_device_t *dev)
{
    int err = ONT_ERR_OK;
#ifdef ONT_PLATFORM_PERSISTENCE_SUPPORT
    uint16_t total_len;
    char *status;

    err = ont_platform_load_device_status((char*)&total_len, 2);
    if (ONT_ERR_OK != err)
    {
        return (ONT_ERR_STATUS_READFAIL == err) ? ONT_ERR_OK : err;
    }

    status = (char*)ont_platform_malloc(total_len);
    if (!status)
        return ONT_ERR_NOMEM;

    err = ont_platform_load_device_status(status, total_len);
    if (ONT_ERR_OK == err)
    {
        const char *cursor = status + 2;
        uint16_t key_len;

        if (ONT_DEVICE_VERSION != *(cursor++))
            return ONT_ERR_INVALID_DEVICE_STATUS_FILE;

        memcpy(&dev->device_id, cursor, 4);
        cursor += 4;

        memcpy(&key_len, cursor, 2);
        cursor += 2;

        dev->key = (char*)ont_platform_malloc(key_len + 1);
        if (dev->key)
        {
            memcpy(dev->key, cursor, key_len);
            dev->key[key_len] = '\0';
        }
        else
        {
            ont_platform_free(status);
            return ONT_ERR_NOMEM;
        }
    }
    ont_platform_free(status);
#endif /* ONT_PLATFORM_PERSISTENCE_SUPPORT */
    return err;
}

int ont_device_create(uint32_t product_id,
                      ont_device_type_t type,
                      const char *name,
                      ont_device_t **dev)
{
    const ont_device_rt_t *rt;
    int err;
    size_t name_len;

    rt = ont_device_get_rt(type);
    if (NULL == rt)
        return ONT_ERR_BADPARAM;

    err = rt->create(dev);
    if (ONT_ERR_OK != err)
        return err;

    if (NULL == name)
        name = "";

    memset(*dev, 0, sizeof(ont_device_t));
    (*dev)->type = type;
    (*dev)->device_id = 0;
    (*dev)->product_id = product_id;

    name_len = strlen(name);
    if (name_len + 1 >= sizeof((*dev)->name))
    {
        rt->destroy(*dev);
        return ONT_ERR_BADPARAM;
    }

    memcpy((*dev)->name, name, name_len);
    (*dev)->name[name_len] = '\0';

    (*dev)->formatter =
        (ont_pkt_formatter_t*)ont_platform_malloc(sizeof(ont_pkt_formatter_t));
    if (NULL == (*dev)->formatter)
        err = ONT_ERR_NOMEM;
    else
        err = ont_pkt_formatter_init((*dev)->formatter);

    if (ONT_ERR_OK != err)
    {
        ont_pkt_formatter_destroy((*dev)->formatter);
        rt->destroy(*dev);
        return ONT_ERR_NOMEM;
    }

    ONT_LOG_DEBUG0("Initialize device successfully.");
    return ONT_ERR_OK;
}

int ont_device_connect(ont_device_t *dev,
                       const char *ip,
                       uint16_t port,
                       const char *reg_code,
                       const char *auth_info,
                       uint16_t keepalive)
{
    int err;
    const ont_device_rt_t *rt;

    if (!dev || !ip || !reg_code || !auth_info)
        return ONT_ERR_BADPARAM;

    ont_platform_snprintf(dev->ip, sizeof(dev->ip), "%s", ip);
    dev->port = port;
    dev->keepalive = keepalive;

    err = ont_device_load_status(dev);
    if (ONT_ERR_OK != err)
        return err;

    ONT_LOG_DEBUG2("Product id is %u, Device id is %u.",
                   dev->product_id,
                   dev->device_id);

    rt = ont_device_get_rt(dev->type);
    dev->exit = 0;

    if (0 == dev->device_id)
    {
        ont_device_status_transform(dev, ONTDEV_STATUS_REGISTERING);
        ONT_LOG_DEBUG0("Begin to register the device ...");
        err = ont_device_register(dev, reg_code, auth_info, rt);
        if (ONT_ERR_OK != err)
            return err;

        err = ont_device_save_status(dev);
        if (ONT_ERR_OK != err)
            return err;

        ONT_LOG_DEBUG2("Register the device successfully, device id is %u, key is '%s'.",
                       dev->device_id, dev->key);
    }

    ont_device_status_transform(dev, ONTDEV_STATUS_RETRIEVING_ACCEPTOR);
   // err = ont_device_retrieve_server(dev, ip, port);
    //ONT_LOG_DEBUG2("Retrieve the accceptor address successfully: %s:%u",
    //               dev->ip, dev->port);

    if (ONT_ERR_OK == err)
    {
        err = rt->connect(dev,auth_info);
        if (ONT_ERR_OK == err)
            ont_device_status_transform(dev, ONTDEV_STATUS_CONNECTED);
    }

    return err;
}

void ont_device_disconnect(ont_device_t *dev)
{
    dev->exit = 1;
}

void ont_device_destroy(ont_device_t *dev)
{
    const ont_device_rt_t *rt;
    if (!dev)
        return;

    rt = ont_device_get_rt(dev->type);

    ont_platform_free(dev->key);

    ont_pkt_formatter_destroy(dev->formatter);
    ont_platform_free(dev->formatter);
    rt->destroy(dev);
}

int ont_device_keepalive(ont_device_t *dev)
{
    const ont_device_rt_t *rt;

    if (!dev)
        return ONT_ERR_BADPARAM;

    rt = ont_device_get_rt(dev->type);
    return rt->keepalive(dev);
}

int ont_device_add_dp_int(ont_device_t *dev, const char *datastream, int value)
{
    if (!dev || !datastream)
        return ONT_ERR_BADPARAM;

    return ont_pkt_formatter_add_uint32(dev->formatter, datastream, (uint32_t)value);
}

int ont_device_add_dp_string(ont_device_t *dev, const char *datastream,
                             const char *value)
{
    if (!dev || !datastream || !value)
        return ONT_ERR_BADPARAM;

    return ont_pkt_formatter_add_string(dev->formatter, datastream, value);
}

int ont_device_add_dp_binary(ont_device_t *dev, const char *datastream,
                             const char *value, size_t size)
{
    if (!dev || !datastream || (!value && size))
        return ONT_ERR_BADPARAM;

    return ont_pkt_formatter_add_binary(dev->formatter, datastream, value, size);
}

int ont_device_add_dp_object(ont_device_t *dev, const char *datastream,
                             const char *value)
{
    if (!dev || !datastream || !value)
        return ONT_ERR_BADPARAM;

    return ont_pkt_formatter_add_json(dev->formatter, datastream, value);
}

#ifdef ONT_PLATFORM_FLOATING_POINT_SUPPORT
int ont_device_add_dp_double(ont_device_t *dev,
                             const char *datastream,
                             double value)
{
    if (!dev || !datastream)
        return ONT_ERR_BADPARAM;

    return ont_pkt_formatter_add_double(dev->formatter, datastream, value);
}
#endif /* ONT_PLATFORM_FLOATING_POINT_SUPPORT */

int ont_device_send_dp(ont_device_t *dev)
{
    const ont_device_rt_t *rt;
    int err = ONT_ERR_OK;

    if (!dev)
        return ONT_ERR_BADPARAM;

    rt = ont_device_get_rt(dev->type);
    if (dev->formatter->result.data && dev->formatter->result.len)
    {
        err = rt->send_datapoints(dev, dev->formatter->result.data,
                                  dev->formatter->result.len);
    }

    if (ONT_ERR_OK == err)
    {
        ont_pkt_formatter_reset(dev->formatter, 1);
    }

    return err;
}

void ont_device_clear_dp(ont_device_t *dev)
{
    if (dev)
    {
        ont_pkt_formatter_destroy(dev->formatter);
    }
}

ont_device_cmd_t *ont_device_get_cmd(ont_device_t *dev)
{
    const ont_device_rt_t *rt;

    if (!dev)
        return NULL;

    rt = ont_device_get_rt(dev->type);
    return rt->get_cmd(dev);
}

int ont_device_reply_cmd(ont_device_t *dev, const char *cmdid,
                         const char *resp, size_t size)
{
    const ont_device_rt_t *rt;

    if (!dev || (!resp && size))
        return ONT_ERR_BADPARAM;

    rt = ont_device_get_rt(dev->type);
    return rt->reply_cmd(dev, cmdid, resp, size);
}

void ont_device_cmd_destroy(ont_device_cmd_t *cmd)
{
    if (!cmd)
        return;

    ont_platform_free(cmd->req);
    ont_platform_free(cmd);
}
