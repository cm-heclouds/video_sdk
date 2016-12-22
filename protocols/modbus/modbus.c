#include "utils.h"
#include "ont_list.h"
#include "ont_modbus.h"
#include "ont_tcp_channel.h"
#include "ont/platform.h"
#include "ont/modbus.h"

#ifdef ONT_PROTOCOL_MODBUS

typedef struct ont_modbus_t {
    ont_device_t            dev;
    ont_channel_interface_t chl;
    ont_list_t             *cmd;
} ont_modbus_t;

typedef struct ont_modbus_ctx {
    int32_t id[2];
    char key[40];
} ont_modbus_ctx;

typedef struct ont_creat_ds_t {
    int32_t err[2];
} ont_creat_ds_t;

#define MODBUS2DEV(x)               (&(x)->dev)
#define DEV2MODBUS(x)               ((ont_modbus_t*)(x))

#define READ_COIL_STATUS              1 /* 读线圈状态 */
#define READ_INPUT_STATUS             2 /* 读输入状态 */
#define READ_HOLDING_REGISTERS        3 /* 读保持寄存器 */
#define READ_INPUT_REGISTERS          4 /* 读输入寄存器 */
#define FORCE_SINGLE_COIL             5 /* 强制单个线圈 */
#define PRESET_SINGLE_REGISTER        6 /* 预置单个寄存器 */
#define READ_EXCEPTION_STATUS         7 /* 读取异常状态 */
#define DIAGNOSTICS                   8 /* 诊断 */
#define FETCH_COMM_EVENT_COUNTER     11 /* 取通讯事件计数器 */
#define FETCH_COMM_EVENT_LOG         12 /* 取通讯事件日志 */
#define FORCE_MULTIPLE_COILS         15 /* 强制多个线圈 */
#define PRESET_MULTIPLE_REGISTERS    16 /* 预置多个寄存器 */
#define REPORT_SLAVE_ID              17 /* 报告从机ID */
#define READ_GENERAL_REFERENCE       20 /* 读通用参考值 */
#define WRITE_GENERAL_REFERENCE      21 /* 写通用参考值 */
#define MASK_WRITE_4X_REGISTER       22 /* Mask Write 4X Register */
#define READ_WRITE_4X_REGISTERS      23 /* Read/Write 4X Registers */
#define READ_FIFO_QUEUE              24 /* Read FIFO 队列 */

#define REPLY_ACK       0 
#define REPLY_HEARTBEAT 1 
#define REPLY_DATA      2 

#define BIG16(x) ((((uint16_t)(x)&0xff00)>>8) | \
    (((uint16_t)(x)& 0x00ff) << 8))

#define BIG32(x) ((((uint32_t)(x)&0xff000000)>>24)| \
    (((uint32_t)(x)& 0x00ff0000) >> 8) | \
    (((uint32_t)(x)& 0x0000ff00) << 8) | \
    (((uint32_t)(x)& 0x000000ff) << 24))

#define BIG64(x) \
{ \
    uint32_t *__x = (uint32_t*)x; \
    uint32_t __t = BIG32( __x[0] ); \
    __x[0] = BIG32( __x[1] ); \
    __x[1] = __t; \
}

int cmd_size( const uint8_t *cmd, uint32_t size ) {
    if ( NULL == cmd || size < 2 ) {
        return -1;
    }

    switch ( cmd[1] ) {
        case READ_COIL_STATUS:
        case READ_INPUT_STATUS:
        case READ_HOLDING_REGISTERS:
        case READ_INPUT_REGISTERS:
        case FORCE_SINGLE_COIL:
        case PRESET_SINGLE_REGISTER:
        case DIAGNOSTICS:
        {
                            return 8;
        }
        break;

        case READ_EXCEPTION_STATUS:
        case FETCH_COMM_EVENT_COUNTER:
        case FETCH_COMM_EVENT_LOG:
        case REPORT_SLAVE_ID:
        {
                                return 4;
        }
        break;

        case FORCE_MULTIPLE_COILS:
        case PRESET_MULTIPLE_REGISTERS:
        {
                                          if ( size > 6 ) {
                                              return (9 + cmd[6]);
                                          }

                                          return 0;
        }
        break;

        case READ_GENERAL_REFERENCE:
        case WRITE_GENERAL_REFERENCE:
        {
                                        if ( size > 2 ) {
                                            return (5 + cmd[2]);
                                        }

                                        return 0;
        }
        break;

        case MASK_WRITE_4X_REGISTER:
        {
                                       return 10;
        }
        break;

        case READ_WRITE_4X_REGISTERS:
        {
                                        if ( size > 10 ) {
                                            return (13 + cmd[10]);
                                        }

                                        return 0;
        }
        break;

        case READ_FIFO_QUEUE:
        {
                                return 6;
        }
        break;

        default:
        {
                   return -1;
        }
        break;
    }

    return -1;
}

int ont_modbus_channel_read_packet( void *user_ctx,
                                    const char *buf,
                                    size_t buf_size,
                                    size_t *read_size ) {
    const char *tmp = NULL;
    ont_device_cmd_t *cmd = NULL;
    ont_modbus_t *dev = (ont_modbus_t*)user_ctx;

#if 1
    int size = 0;
#else
    uint16_t size = 0;
#endif

    if ( NULL == dev ||
         NULL == buf ||
         NULL == read_size ) {
        return ONT_ERR_BADPARAM;
    }

#if 1
    * read_size = 0;
    while ( buf_size >= 2 ) {
        size = cmd_size( (const uint8_t*)buf, (uint32_t)buf_size );
        if ( size <= 0 ) {
            break;
        }

        if ( buf_size < size ) {
            break;
        }

        tmp = buf;
        buf += size;
        buf_size -= size;
        *read_size += size;
        cmd = ont_platform_malloc( sizeof(ont_device_cmd_t) );
        if ( NULL == cmd ) {
            continue;
        }
        else {
            ont_memzero( cmd, sizeof(ont_device_cmd_t) );
        }

        cmd->req = ont_platform_malloc( size );
        if ( NULL == cmd->req ) {
            ont_platform_free( cmd );
            cmd = NULL;

            continue;
        }

        ont_memmove( cmd->req, tmp, size );
        cmd->size = size;
        ont_list_insert( dev->cmd, cmd );
    }
#else
    *read_size = 0;
    while ( buf_size >= sizeof(size) )
    {
        size = *(uint16_t*)buf;
        size = BIG16( size );
        if ( buf_size < size + sizeof(size) )
        {
            break;
        }

        tmp = buf;
        buf += size + sizeof(size);
        buf_size -= size + sizeof(size);
        *read_size += size + sizeof(size);
        cmd = ont_platform_malloc( sizeof(ont_device_cmd_t) );
        if ( NULL == cmd )
        {
            continue;
        }
        else
        {
            ont_memzero( cmd, sizeof(ont_device_cmd_t) );
        }

        cmd->req = ont_platform_malloc( size );
        if ( NULL == cmd->req )
        {
            ont_platform_free( cmd );
            cmd = NULL;

            continue;
        }

        ont_memmove( cmd->req, tmp + sizeof(size), size );
        cmd->size = size;
        ont_list_insert( dev->cmd, cmd );
    }
#endif

    return ONT_ERR_OK;
}

int ont_modbus_create( ont_device_t **dev ) {
    ont_modbus_t *_dev = NULL;

    if ( NULL == dev ) {
        return ONT_ERR_BADPARAM;
    }

    _dev = ont_platform_malloc( sizeof(ont_modbus_t) );
    if ( NULL == _dev ) {
        return ONT_ERR_NOMEM;
    }

    ont_memzero( _dev, sizeof(ont_modbus_t) );
    *dev = MODBUS2DEV( _dev );

    return ONT_ERR_OK;
}

void ont_modbus_destroy( ont_device_t *dev ) {
    ont_modbus_t *_dev = NULL;
    ont_device_cmd_t *cmd = NULL;

    if ( NULL != dev ) {
        _dev = DEV2MODBUS( dev );
        _dev->chl.fn_deinitilize( _dev->chl.channel );

        while ( ont_list_pop_front( _dev->cmd, (void**)&cmd ) ) {
            ont_device_cmd_destroy( cmd );
        }

        ont_list_destroy( _dev->cmd );
        ont_platform_free( _dev );
    }
}

int ont_modbus_connect( ont_device_t *dev, const char *auth_info ) {
    int rc = 0;
    uint8_t login[52];
    ont_modbus_t *_dev = NULL;

#if 1
    const char *phone = NULL;
    const char *end_phone = NULL;
    const char *svrpwd = NULL;
    const char *end_svrpwd = NULL;

    if ( NULL == dev || NULL == auth_info ) {
        return ONT_ERR_BADPARAM;
    }

    /* get phone and svrpwd */
    phone = ont_strchr( auth_info, '"' );
    if ( NULL == phone ) {
        return ONT_ERR_BADPARAM;
    }

    end_phone = ont_strchr( phone + 1, '"' );
    if ( NULL == end_phone ) {
        return ONT_ERR_BADPARAM;
    }

    svrpwd = ont_strchr( end_phone + 1, '"' );
    if ( NULL == svrpwd ) {
        return ONT_ERR_BADPARAM;
    }

    end_svrpwd = ont_strchr( svrpwd + 1, '"' );
    if ( NULL == end_svrpwd ) {
        return ONT_ERR_BADPARAM;
    }
#else
    ont_modbus_ctx ctx;
    uint8_t *tmp = NULL;
#endif


    _dev = DEV2MODBUS( dev );
    rc = ont_tcp_channel_create( &_dev->chl,
                                 dev->ip,
                                 dev->port,
                                 ONT_SOCK_SEND_BUF_SIZE,
                                 ONT_SOCK_RECV_BUF_SIZE,
                                 _dev,
                                 dev->keepalive );
    if ( rc ) {
        return rc;
    }

    /* connect */
    rc = _dev->chl.fn_connect( _dev->chl.channel, &dev->exit );
    if ( rc ) {
        return rc;
    }

    /* init */
    rc = _dev->chl.fn_initilize( _dev->chl.channel,
                                 ont_modbus_channel_read_packet );
    if ( rc ) {
        return rc;
    }

#if 1
    ont_memzero( login, sizeof(login) );
    ont_memmove( login + 20, phone + 1, (uint32_t)(end_phone - phone - 1) );
    ont_memmove( login + 32, svrpwd + 1, (uint32_t)(end_svrpwd - svrpwd - 1) );
    ont_itos( (char*)login + 41, (int32_t)dev->product_id );
#else
    /* modbus ctx */
    ont_memzero( &ctx, sizeof(ctx) );
    ont_memmove( ctx.id, &dev->device_id, sizeof(dev->device_id) );
    ont_strcpy( ctx.key, dev->key );

    /* login data */
    tmp = login;
    *tmp = 0xff; tmp += 1;
    *tmp = 0xef; tmp += 1;
    *tmp = 0xfe; tmp += 1;
    *tmp = 0xff; tmp += 1;
    BIG64( ctx.id );
    ont_memmove( tmp, &ctx, sizeof(ctx) );
#endif

    /* login */
    rc = _dev->chl.fn_write( _dev->chl.channel,
                             (const char*)login,
                             sizeof(login) );
    if ( rc ) {
        return rc;
    }

    /* cmd list */
    if ( _dev->cmd ) {
        ont_list_clear( _dev->cmd );
    }
    else {
        _dev->cmd = ont_list_create();
    }

    return ONT_ERR_OK;
}

int ont_modbus_send_datapoints( ont_device_t *dev,
                                const char *payload,
                                size_t bytes ) {
    return ONT_ERR_DP_NOT_SUPPORT;
}

ont_device_cmd_t* ont_modbus_get_cmd( ont_device_t *dev ) {
    ont_modbus_t *_dev = NULL;
    ont_device_cmd_t *cmd = NULL;

    if ( NULL == dev ) {
        return NULL;
    }

    _dev = DEV2MODBUS( dev );
    _dev->chl.fn_process( _dev->chl.channel );
    ont_list_pop_front( _dev->cmd, (void**)&cmd );

    return cmd;
}

int ont_modbus_reply_cmd( ont_device_t *dev,
                          const char *cmdid,
                          const char *resp,
                          size_t size ) {
    int rc = 0;
    ont_modbus_t *_dev = NULL;

#if 1
#else
    uint16_t _size = 0;
    uint8_t _type = REPLY_DATA;
#endif

    if ( NULL == dev ||
         NULL == resp ||
         0 == size ) {
        return ONT_ERR_BADPARAM;
    }

    _dev = DEV2MODBUS( dev );
#if 1
#else
    _size = BIG16( size );
    rc = _dev->chl.fn_write( _dev->chl.channel, (char*)&_size, sizeof(_size) );
    if ( rc )
    {
        return rc;
    }

    rc = _dev->chl.fn_write( _dev->chl.channel, (char*)&_type, sizeof(_type) );
    if ( rc )
    {
        return rc;
    }
#endif

    rc = _dev->chl.fn_write( _dev->chl.channel, resp, size );
    if ( rc ) {
        return rc;
    }

    return ONT_ERR_OK;
}

int ont_modbus_keepalive( ont_device_t *dev ) {
    int rc = 0;
    ont_modbus_t *_dev = NULL;

#if 1
    char hearbeat[2] = { 0x00, 0x00 };
#else
    uint16_t _size = 0;
    uint8_t _type = REPLY_HEARTBEAT;
#endif

    if ( NULL == dev ) {
        return ONT_ERR_BADPARAM;
    }

    _dev = DEV2MODBUS( dev );
#if 1
    rc = _dev->chl.fn_write( _dev->chl.channel, hearbeat, sizeof(hearbeat) );
    if ( rc ) {
        return rc;
    }
#else
    _size = BIG16( _size );
    rc = _dev->chl.fn_write( _dev->chl.channel, (char*)&_size, sizeof(_size) );
    if ( rc )
    {
        return rc;
    }

    rc = _dev->chl.fn_write( _dev->chl.channel, (char*)&_type, sizeof(_type) );
    if ( rc )
    {
        return rc;
    }
#endif

    return ONT_ERR_OK;
}

int ont_http_create_ds_read( void *user_ctx,
                             const char *buf,
                             size_t buf_size,
                             size_t *read_size ) {
    const char *str = NULL;
    int str_len = 0;
    ont_creat_ds_t *ds = (ont_creat_ds_t*)user_ctx;

    if ( NULL == ds ||
         NULL == buf ||
         NULL == read_size ||
         0 == buf_size ) {
        return ONT_ERR_BADPARAM;
    }

    *read_size = 0;
    if ( ds->err[0] < 0 ) {
        if ( ont_strcmp( buf, "HTTP/1.0", 8 ) && ont_strcmp( buf, "HTTP/1.1", 8 ) ) {
            return ONT_ERR_OK;
        }

        str_len = 8;
        str = ont_strstr_s( buf + str_len, (uint32_t)(buf_size - str_len), "\r\n" );
        if ( !str ) {
            return ONT_ERR_OK;
        }

        ds->err[0] = ont_stoi( buf + str_len );
        *read_size = str - buf + 2;
        str_len = (int)(str - buf) + 2;
    }

    while ( ds->err[0] >= 0 && ds->err[1] < 0 ) {
        str = ont_strstr_s( buf + str_len, (uint32_t)(buf_size - str_len), "\r\n" );
        if ( !str ) {
            return ONT_ERR_OK;
        }
        else if ( str != buf + str_len ) {
            *read_size = (int)(str - buf) + 2;
            str_len = (int)(str - buf) + 2;

            continue;
        }
        else {
            str_len = (int)(str - buf) + 2;
        }

        str = ont_strstr_s( buf + str_len, (uint32_t)(buf_size - str_len), "\"errno\"" );
        if ( !str ) {
            return ONT_ERR_OK;
        }

        str_len = (int)(str - buf) + 7;
        str = ont_strchr_s( buf + str_len, (uint32_t)(buf_size - str_len), ':' );
        if ( !str ) {
            return ONT_ERR_OK;
        }

        str_len = (int)(str - buf) + 1;
        str = ont_strchr_s( buf + str_len, (uint32_t)(buf_size - str_len), ',' );
        if ( !str ) {
            return ONT_ERR_OK;
        }

        ds->err[1] = ont_stoi( buf + str_len );
        *read_size = buf_size;

        return ONT_ERR_OK;
    }

    if ( ds->err[0] >= 0 && ds->err[1] >= 0 ) {
        *read_size = buf_size;
    }

    return ONT_ERR_OK;
}

int ont_modbus_create_ds( ont_device_t *dev,
                          const char *server,
                          uint16_t port,
                          const char *id,
                          const char *cmd,
                          int interval,
                          const char *formula ) {
    int rc = 0;
    char buf[16];
    int dev_id[2];
    int body_len = 0;
    ont_modbus_t *_dev = NULL;
    ont_channel_interface_t chl;
    ont_creat_ds_t ds;

    const char *head[] =
    {
        "POST /devices/",
        "/datastreams HTTP/1.1\r\nHost:",
        "\r\nContent-Length:",
        "\r\napi-key:",
        "\r\nConnection:close\r\n\r\n"
    };

    const char *body[] =
    {
        "{\"id\":\"",
        "\",\"cmd\":\"",
        "\",\"interval\":",
        ",\"formula\":\"",
        "\"}",
        "}"
    };

    if ( NULL == dev ||
         NULL == dev->key ||
         NULL == server ||
         NULL == id ||
         NULL == cmd ||
         port == 0 ||
         interval <= 0 ) {
        return ONT_ERR_BADPARAM;
    }

    ds.err[0] = -1;
    ds.err[1] = -1;
    rc = ont_tcp_channel_create( &chl,
                                 server,
                                 port,
                                 ONT_SOCK_SEND_BUF_SIZE,
                                 ONT_SOCK_RECV_BUF_SIZE,
                                 &ds,
                                 dev->keepalive );
    if ( rc ) {
        return rc;
    }

    rc = chl.fn_initilize( chl.channel,
                           ont_http_create_ds_read );
    if ( rc ) {
        chl.fn_deinitilize( chl.channel );

        return rc;
    }

    rc = chl.fn_connect( chl.channel, &dev->exit );
    if ( rc ) {
        chl.fn_deinitilize( chl.channel );

        return rc;
    }

    /* body length */
    body_len += ont_strlen( body[0] );
    body_len += ont_strlen( id );
    body_len += ont_strlen( body[1] );
    body_len += ont_strlen( cmd );
    body_len += ont_strlen( body[2] );
    body_len += ont_strlen( ont_itos( buf, interval ) );
    if ( formula ) {
        body_len += ont_strlen( body[3] );
        body_len += ont_strlen( formula );
        body_len += ont_strlen( body[4] );
    }
    else {
        body_len += ont_strlen( body[5] );
    }

    /* send head */
    _dev = DEV2MODBUS( dev );
    ont_memzero( dev_id, sizeof(dev_id) );
    ont_memmove( dev_id, &_dev->dev.device_id, sizeof(_dev->dev.device_id) );
    rc = chl.fn_write( chl.channel, head[0], ont_strlen( head[0] ) );
    if ( rc ) {
        chl.fn_deinitilize( chl.channel );

        return rc;
    }
    ont_i64tos( buf, dev_id );
    rc = chl.fn_write( chl.channel, buf, ont_strlen( buf ) );
    if ( rc ) {
        chl.fn_deinitilize( chl.channel );

        return rc;
    }
    rc = chl.fn_write( chl.channel, head[1], ont_strlen( head[1] ) );
    if ( rc ) {
        chl.fn_deinitilize( chl.channel );

        return rc;
    }
    rc = chl.fn_write( chl.channel, server, ont_strlen( server ) );
    if ( rc ) {
        chl.fn_deinitilize( chl.channel );

        return rc;
    }
    rc = chl.fn_write( chl.channel, head[2], ont_strlen( head[2] ) );
    if ( rc ) {
        chl.fn_deinitilize( chl.channel );

        return rc;
    }
    ont_itos( buf, body_len );
    rc = chl.fn_write( chl.channel, buf, ont_strlen( buf ) );
    if ( rc ) {
        chl.fn_deinitilize( chl.channel );

        return rc;
    }
    rc = chl.fn_write( chl.channel, head[3], ont_strlen( head[3] ) );
    if ( rc ) {
        chl.fn_deinitilize( chl.channel );

        return rc;
    }
    rc = chl.fn_write( chl.channel, dev->key, ont_strlen( dev->key ) );
    if ( rc ) {
        chl.fn_deinitilize( chl.channel );

        return rc;
    }
    rc = chl.fn_write( chl.channel, head[4], ont_strlen( head[4] ) );
    if ( rc ) {
        chl.fn_deinitilize( chl.channel );

        return rc;
    }

    /* send body */
    rc = chl.fn_write( chl.channel, body[0], ont_strlen( body[0] ) );
    if ( rc ) {
        chl.fn_deinitilize( chl.channel );

        return rc;
    }
    rc = chl.fn_write( chl.channel, id, ont_strlen( id ) );
    if ( rc ) {
        chl.fn_deinitilize( chl.channel );

        return rc;
    }
    rc = chl.fn_write( chl.channel, body[1], ont_strlen( body[1] ) );
    if ( rc ) {
        chl.fn_deinitilize( chl.channel );

        return rc;
    }
    rc = chl.fn_write( chl.channel, cmd, ont_strlen( cmd ) );
    if ( rc ) {
        chl.fn_deinitilize( chl.channel );

        return rc;
    }
    rc = chl.fn_write( chl.channel, body[2], ont_strlen( body[2] ) );
    if ( rc ) {
        chl.fn_deinitilize( chl.channel );

        return rc;
    }
    ont_itos( buf, interval );
    rc = chl.fn_write( chl.channel, buf, ont_strlen( buf ) );
    if ( rc ) {
        chl.fn_deinitilize( chl.channel );

        return rc;
    }
    if ( formula ) {
        rc = chl.fn_write( chl.channel, body[3], ont_strlen( body[3] ) );
        if ( rc ) {
            chl.fn_deinitilize( chl.channel );

            return rc;
        }
        rc = chl.fn_write( chl.channel, formula, ont_strlen( formula ) );
        if ( rc ) {
            chl.fn_deinitilize( chl.channel );

            return rc;
        }
        rc = chl.fn_write( chl.channel, body[4], ont_strlen( body[4] ) );
        if ( rc ) {
            chl.fn_deinitilize( chl.channel );

            return rc;
        }
    }
    else {
        rc = chl.fn_write( chl.channel, body[5], ont_strlen( body[5] ) );
        if ( rc ) {
            chl.fn_deinitilize( chl.channel );

            return rc;
        }
    }

    do {
        if ( chl.fn_process( chl.channel ) ) {
            break;
        }

        if ( ds.err[0] >= 0 && ds.err[1] >= 0 ) {
            break;
        }
    }
    while ( 1 );

    /*close*/
    chl.fn_deinitilize( chl.channel );

    /*check*/
    if ( 200 != ds.err[0] || 0 != ds.err[1] ) {
        return ONT_ERR_CREATE_DS_FAILED;
    }

    return ONT_ERR_OK;
}

#endif /* ONT_PROTOCOL_MODBUS */