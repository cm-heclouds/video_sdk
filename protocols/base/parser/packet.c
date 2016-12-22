#include <string.h>
#include "packet.h"
#include "types.h"
#include "ont/platform.h"
#include "ont/error.h"

static int8_t is_little_endian()
{
    uint16_t val = 0x01;
    if (*(uint8_t*)(&val) == 1)
    {
        return 0;
    }

    return 1;
}

static char* ont_parser_strndup(const char* src, size_t n)
{
    char* dst = NULL;
    if (!src)
    {
        return NULL;
    }

    dst = (char*)ont_platform_malloc(n);
    memcpy(dst, src, n);
    return dst;
}

static char* ont_parser_strdup(const char* src)
{
    char* dst = NULL;
    size_t n = 0;
    if (!src){
        return NULL;
    }
    n = strlen(src);
    dst = (char*)ont_platform_malloc(n);
    memcpy(dst, src, n);
    return dst;
}

#define next_op()                                   \
    *op = p->op;                                    \
    *node = (*node)->next;

static int ont_parser_init_ref_node(ont_parser_list_node_t* node)
{
    ont_parser_uint8_t* p = NULL;
    while (node)
    {
        p = (ont_parser_uint8_t*)node;
        if (p->ref_beg > p->ref_end || p->ref_end - p->ref_beg > 7)
        {
            return ONT_ERR_REFERENCE_OVER_RANGE;
        }
        p->value = 0;
        node = p->sibling.ref_node;
    }

    return ONT_ERR_OK;
}

#define def_init_func(type)                                             \
    int init_##type(ont_parser_list_node_t** node,                      \
                        struct ont_parser_node_op_t** op){              \
        ont_parser_##type *p = (ont_parser_##type *)(*node);            \
        int rc = 0;                                                     \
        p->own = 0;                                                     \
        p->value = 0;                                                   \
        rc = ont_parser_init_ref_node(p->sibling.ref_node);             \
        ONT_EXPECT_ERR_OK(rc);                                          \
                                                                        \
        next_op();                                                      \
        return ONT_ERR_OK;                                              \
    }

def_init_func(uint8_t)
def_init_func(uint16_t)
def_init_func(uint32_t)

#ifdef ONT_PLATFORM_FLOATING_POINT_SUPPORT
int init_double_t(ont_parser_list_node_t** node, struct ont_parser_node_op_t** op)
{
    ont_parser_double_t*p = (ont_parser_double_t*)(*node);
    if (p->sibling.ref_node)
    {
        return ONT_ERR_REFERENCE_WRONG_TYPE;
    }

    p->own = 0;
    p->value = 0;
    next_op();
    return ONT_ERR_OK;
}
#endif /* ONT_PLATFORM_FLOATING_POINT_SUPPORT */

int init_compress_uint32_t(ont_parser_list_node_t** node, struct ont_parser_node_op_t** op)
{
    ont_parser_compress_uint32_t*p = (ont_parser_compress_uint32_t*)(*node);
    if (p->sibling.ref_node)
    {
        return ONT_ERR_REFERENCE_WRONG_TYPE;
    }

    p->own = 0;
    p->value = 0;
    next_op();
    return ONT_ERR_OK;
}

int init_string_t(ont_parser_list_node_t** node, struct ont_parser_node_op_t** op)
{
    ont_parser_string_t*p = (ont_parser_string_t*)(*node);
    if (p->sibling.ref_node)
    {
        return ONT_ERR_REFERENCE_WRONG_TYPE;
    }

    p->own = 0;
    p->value = NULL;
    next_op();
    return ONT_ERR_OK;
}

int init_bytes_t(ont_parser_list_node_t** node, struct ont_parser_node_op_t** op)
{
    ont_parser_binary_t* p = (ont_parser_binary_t*)(*node);
    if (p->sibling.ref_node)
    {
        return ONT_ERR_REFERENCE_WRONG_TYPE;
    }

    p->own = 0;
    p->value = NULL;
    p->len = 0;

    next_op();
    return ONT_ERR_OK;
}

int init_binary_t(ont_parser_list_node_t** node, struct ont_parser_node_op_t** op)
{
    ont_parser_bytes_t* p = (ont_parser_bytes_t*)(*node);
    if (p->sibling.ref_node)
    {
        return ONT_ERR_REFERENCE_WRONG_TYPE;
    }

    p->own = 0;
    p->value = NULL;
    p->len = 0;

    next_op();
    return ONT_ERR_OK;
}

int init_raw_t(ont_parser_list_node_t** node, struct ont_parser_node_op_t** op)
{
    ont_parser_raw_t* p = (ont_parser_raw_t*)(*node);
    if (p->sibling.ref_node)
    {
        return ONT_ERR_REFERENCE_WRONG_TYPE;
    }

    p->own = 0;
    p->value = NULL;
    p->len = 0;

    next_op();
    return ONT_ERR_OK;
}

#undef def_init_func

#define def_destroy_func(type)                                  \
    int destroy_##type(ont_parser_list_node_t** node,           \
                       struct ont_parser_node_op_t** op){       \
        ont_parser_##type *p = (ont_parser_##type *)(*node);    \
        p->value = 0;                                           \
        next_op();                                              \
        return ONT_ERR_OK;                                      \
    }

def_destroy_func(uint8_t)
def_destroy_func(uint16_t)
def_destroy_func(uint32_t)

#ifdef ONT_PLATFORM_FLOATING_POINT_SUPPORT
def_destroy_func(double_t)
#endif /* ONT_PLATFORM_FLOATING_POINT_SUPPORT */

def_destroy_func(compress_uint32_t)

int destroy_string_t(ont_parser_list_node_t** node, struct ont_parser_node_op_t** op)
{
    ont_parser_string_t*p = (ont_parser_string_t*)(*node);
    if (p->own)
    {
        if (p->value)
        {
            ont_platform_free((void*)p->value);
        }
    }
    p->value = NULL;
    p->own = 0;

    next_op();
    return ONT_ERR_OK;
}

int destroy_bytes_t(ont_parser_list_node_t** node, struct ont_parser_node_op_t** op)
{
    ont_parser_binary_t* p = (ont_parser_binary_t*)(*node);
    if (p->own)
    {
        if (p->value)
        {
            ont_platform_free((void*)p->value);
        }
    }
    p->value = NULL;
    p->len = 0;
    p->own = 0;

    next_op();
    return ONT_ERR_OK;
}

int destroy_binary_t(ont_parser_list_node_t** node, struct ont_parser_node_op_t** op)
{
    ont_parser_bytes_t* p = (ont_parser_bytes_t*)(*node);
    if (p->own)
    {
        if (p->value)
        {
            ont_platform_free((void*)p->value);
        }
    }
    p->value = NULL;
    p->len = 0;
    p->own = 0;

    next_op();
    return ONT_ERR_OK;
}

int destroy_raw_t(ont_parser_list_node_t** node, struct ont_parser_node_op_t** op)
{
    ont_parser_raw_t* p = (ont_parser_raw_t*)(*node);
    if (p->own)
    {
        if (p->value)
        {
            ont_platform_free((void*)p->value);
        }
    }
    p->value = NULL;
    p->len = 0;
    p->own = 0;

    next_op();
    return ONT_ERR_OK;
}

#undef def_init_func

#define def_len_func(type, length)                              \
    int len_##type(ont_parser_list_node_t** node,               \
                   ont_parser_node_op_t** op, size_t* len){     \
        ont_parser_##type *p = (ont_parser_##type *)(*node);    \
        *len = 0;                                               \
        if (!p->filter || !p->filter(p->filter_node)){          \
            *len = length;                                      \
        }                                                       \
                                                                \
        next_op();                                              \
        return ONT_ERR_OK;                                      \
    }

def_len_func(uint8_t, 1)
def_len_func(uint16_t, 2)
def_len_func(uint32_t, 4)
#ifdef ONT_PLATFORM_FLOATING_POINT_SUPPORT
def_len_func(double_t, 8)
#endif /* ONT_PLATFORM_FLOATING_POINT_SUPPORT */

int len_string_t(ont_parser_list_node_t** node, struct ont_parser_node_op_t** op, size_t* len)
{
    ont_parser_string_t*p = (ont_parser_string_t*)(*node);
    *len = 0;
    if (!p->filter || !p->filter(p->filter_node))
    {
        if (p->value)
        {
            *len = strlen(p->value) + 1;
        }
        else{
            *len = 1;
        }
    }

    next_op();
    return ONT_ERR_OK;
}

int len_bytes_t(ont_parser_list_node_t** node, struct ont_parser_node_op_t** op, size_t* len)
{
    ont_parser_bytes_t*p = (ont_parser_bytes_t*)(*node);
    *len = 0;
    if (!p->filter || !p->filter(p->filter_node))
    {
        *len = p->len + 2;
    }

    next_op();
    return ONT_ERR_OK;
}

int len_binary_t(ont_parser_list_node_t** node, struct ont_parser_node_op_t** op,size_t* len)
{
    ont_parser_bytes_t*p = (ont_parser_bytes_t*)(*node);
    *len = 0;
    if (!p->filter || !p->filter(p->filter_node))
    {
        *len = p->len + 4;
    }

    next_op();
    return ONT_ERR_OK;
}

int len_raw_t(ont_parser_list_node_t** node, struct ont_parser_node_op_t** op, size_t* len)
{
    ont_parser_raw_t*p = (ont_parser_raw_t*)(*node);
    *len = 0;
    if (!p->filter || !p->filter(p->filter_node))
    {
        *len = p->len;
    }

    next_op();
    return ONT_ERR_OK;
}

int len_compress_uint32_t(ont_parser_list_node_t** node, struct ont_parser_node_op_t** op, size_t* len)
{
    ont_parser_compress_uint32_t* p = (ont_parser_compress_uint32_t*)(*node);
    uint32_t value = p->value;
    *len = 0;

    if (!p->filter || !p->filter(p->filter_node))
    {

        do{
            value /= 128;
            ++(*len);
        }while(value > 0);

        if (*len > 4)
        {
            return ONT_ERR_REMAIN_LEN_OVER_RANGE;
        }
    }
    else{
        *len = 0;
    }
    next_op();
    return ONT_ERR_OK;
}

#undef def_len_func

static void ont_parser_write_integer(char* data, char* val, size_t len)
{
    size_t i = 0;
    if (is_little_endian() == 0)
    {
        for (i=0; i<len; ++i)
        {
            *(data+len-i-1) = *val;
            ++val;
        }
    }
    else{
        for (i=0; i<len; ++i)
        {
            *(data+i) = *val;
            ++val;
        }
    }
}

#define serialize_num(data, len)                        \
    if (!p->filter || !p->filter(p->filter_node)){      \
        val_len = sizeof(p->value);                     \
        if (len < val_len){                             \
            return ONT_ERR_NOMEM;                       \
        }                                               \
                                                        \
        val = (char*)(&p->value);                       \
        ont_parser_write_integer(data, val, val_len);   \
        len = val_len;                                  \
    }                                                   \
    else{                                               \
        len = 0;                                        \
    }

#define def_ont_parser_serialize_ref(type)                              \
    int ont_parser_serialize_ref_##type(type##_t* ref,                  \
                                        ont_parser_list_node_t* node){  \
        ont_parser_uint8_t* p = NULL;                                   \
        uint8_t val = 0;                                                \
                                                                        \
        while (node){                                                   \
            p = (ont_parser_uint8_t*)node;                              \
            val = p->value;                                             \
            if (val >= (0x1 << (p->ref_end - p->ref_beg + 1))){         \
                return ONT_ERR_REFERENCE_OVER_RANGE;                    \
            }                                                           \
            *ref |= val << p->ref_beg;                                  \
            node = p->sibling.ref_node;                                 \
        }                                                               \
                                                                        \
        return ONT_ERR_OK;                                              \
    }

def_ont_parser_serialize_ref(uint8)
def_ont_parser_serialize_ref(uint16)
def_ont_parser_serialize_ref(uint32)

#define def_serialize_func(name)                                    \
    int serialize_##name##_t(char* data, size_t* len,               \
                             ont_parser_list_node_t** node,         \
                             struct ont_parser_node_op_t** op){     \
        ont_parser_##name##_t* p = (ont_parser_##name##_t*)(*node); \
        size_t val_len = 0;                                         \
        char* val = NULL;                                           \
        int rc = 0;                                                 \
        rc = ont_parser_serialize_ref_##name(&p->value,             \
                                             p->sibling.ref_node);  \
        ONT_EXPECT_ERR_OK(rc);                                      \
        serialize_num(data, *len);                                  \
        next_op();                                                  \
        return ONT_ERR_OK;                                          \
    }

def_serialize_func(uint8)
def_serialize_func(uint16)
def_serialize_func(uint32)

#ifdef ONT_PLATFORM_FLOATING_POINT_SUPPORT
int serialize_double_t(char* data, size_t* len, ont_parser_list_node_t** node,
                       struct ont_parser_node_op_t** op)
{
    ont_parser_double_t* p = (ont_parser_double_t*)(*node);
    size_t val_len = 0;
    char* val = NULL;

    serialize_num(data, *len);
    next_op();
    return ONT_ERR_OK;
}
#endif /* ONT_PLATFORM_FLOATING_POINT_SUPPORT */

int serialize_string_t(char* data, size_t* len, ont_parser_list_node_t** node,
                       struct ont_parser_node_op_t** op)
{
    ont_parser_string_t*p = (ont_parser_string_t*)(*node);
    size_t val_len = 0;
    if (!p->filter || !p->filter(p->filter_node))
    {
        val_len = p->value ? strlen(p->value) : 0;

        if (*len < val_len + 1)
        {
            return ONT_ERR_NOMEM;
        }

        if (p->value)
        {
            memcpy(data, p->value, val_len);
        }

        *(data+val_len) = '\0';
        *len = val_len + 1;
    }
    else{
        *len = 0;
    }

    next_op();
    return ONT_ERR_OK;
}


int serialize_bytes_t(char* data, size_t* len, ont_parser_list_node_t** node, struct ont_parser_node_op_t** op)
{
    ont_parser_bytes_t* p = (ont_parser_bytes_t*)(*node);
    if (!p->filter || !p->filter(p->filter_node))
    {
        if (*len < p->len + 2)
        {
            return ONT_ERR_NOMEM;
        }

        ont_parser_write_integer(data, (char*)(&p->len), 2);
        if (p->len)
        {
            memcpy(data + 2, p->value, p->len);
        }

        *len = p->len + 2;
    }
    else{
        *len = 0;
    }

    next_op();
    return ONT_ERR_OK;
}

int serialize_binary_t(char* data, size_t* len, ont_parser_list_node_t** node, struct ont_parser_node_op_t** op)
{
    ont_parser_bytes_t* p = (ont_parser_bytes_t*)(*node);
    if (!p->filter || !p->filter(p->filter_node))
    {
        if (*len < p->len + 4)
        {
            return ONT_ERR_NOMEM;
        }

        ont_parser_write_integer(data, (char*)(&p->len), 4);
        if (p->len)
        {
            memcpy(data + 4, p->value, p->len);
        }

        *len = p->len + 4;
    }
    else{
        *len = 0;
    }

    next_op();
    return ONT_ERR_OK;
}

int serialize_raw_t(char* data, size_t* len, ont_parser_list_node_t** node, struct ont_parser_node_op_t** op)
{
    ont_parser_raw_t* p = (ont_parser_raw_t*)(*node);
    if (!p->filter || !p->filter(p->filter_node))
    {
        if (*len < p->len)
        {
            return ONT_ERR_NOMEM;
        }

        if (p->len)
        {
            memcpy(data, p->value, p->len);
        }

        *len = p->len;
    }
    else{
        *len = 0;
    }

    next_op();
    return ONT_ERR_OK;
}

int serialize_compress_uint32_t(char* data, size_t* len, ont_parser_list_node_t** node, struct ont_parser_node_op_t** op)
{
    ont_parser_compress_uint32_t* p = (ont_parser_compress_uint32_t*)(*node);
    uint32_t value = p->value;
    uint8_t byte = 0;
    size_t index = 0;
    if (!p->filter || !p->filter(p->filter_node))
    {
        do{
            byte = value % 128;
            value /= 128;
            if (value > 0){
                byte |= 0x80;
            }

            if (*len < 1+index){
                return ONT_ERR_NOMEM;
            }

            data[index] = byte;
            ++index;
        }while(value > 0 && index < 4);

        if (value)
        {
            return ONT_ERR_REMAIN_LEN_OVER_RANGE;
        }

        *len = index;
    }
    else{
        *len = 0;
    }

    next_op();
    return ONT_ERR_OK;
}

#undef def_serialize_func
#undef serialize_num

static void ont_parser_read_integer(const char* data, char* val, size_t len)
{
    size_t i = 0;
    if (is_little_endian() == 0)
    {
        for (i=0; i<len; ++i)
        {
            *val = *(data+len-i-1);
            ++val;
        }
    }
    else{
        for (i=0; i<len; ++i)
        {
            *val = *(data+i);
            ++val;
        }
    }
}

#define deserialize_num(data, len)                      \
    size_t val_len = 0;                                 \
    char* val = NULL;                                   \
    if (!p->filter || !p->filter(p->filter_node)){      \
        val_len = sizeof(p->value);                     \
        if (len < val_len){                             \
            return ONT_ERR_DESERIALIZE_INCOMPLETE;      \
        }                                               \
                                                        \
        val = (char*)&p->value;                         \
        ont_parser_read_integer(data, val, val_len);    \
        len = val_len;                                  \
    }                                                   \
    else{                                               \
        len = 0;                                        \
    }

#define def_ont_parser_deserialize_ref(type)                            \
    void ont_parser_deserialize_ref_##type(type##_t ref,                \
                                           ont_parser_list_node_t* node){ \
        ont_parser_uint8_t* p = NULL;                                   \
                                                                        \
        while (node){                                                   \
            p = (ont_parser_uint8_t*)node;                              \
            p->value = (ref >> p->ref_beg) & ((uint8_t)(0x01 << (p->ref_end - p->ref_beg + 1)) - 1); \
            node = p->sibling.ref_node;                                 \
        }                                                               \
    }

def_ont_parser_deserialize_ref(uint8)
def_ont_parser_deserialize_ref(uint16)
def_ont_parser_deserialize_ref(uint32)

#define def_deserialize_func(name)                                      \
    int deserialize_##name##_t(const char* data, size_t* len, int8_t own, \
                               ont_parser_list_node_t** node,           \
                               ont_parser_node_op_t** op){              \
        ont_parser_##name##_t* p = (ont_parser_##name##_t*)(*node);     \
        deserialize_num(data, *len);                                    \
        ont_parser_deserialize_ref_##name(p->value, p->sibling.ref_node); \
        next_op();                                                      \
        return ONT_ERR_OK;                                              \
    }

def_deserialize_func(uint8)
def_deserialize_func(uint16)
def_deserialize_func(uint32)

#ifdef ONT_PLATFORM_FLOATING_POINT_SUPPORT
int32_t deserialize_double_t(const char* data, size_t* len, int8_t own,
                             ont_parser_list_node_t** node,
                             ont_parser_node_op_t** op){
    ont_parser_double_t* p = (ont_parser_double_t*)(*node);
    deserialize_num(data, *len);
    next_op();
    return ONT_ERR_OK;
}
#endif /* ONT_PLATFORM_FLOATING_POINT_SUPPORT */

int deserialize_string_t(const char* data, size_t* len, int8_t own, ont_parser_list_node_t** node, ont_parser_node_op_t** op)
{
    ont_parser_string_t*p = (ont_parser_string_t*)(*node);
    size_t val_len = 0;
    if (!p->filter || !p->filter(p->filter_node))
    {
        val_len = strlen((char*)data);
        if (*len < val_len + 1)
        {
            return ONT_ERR_DESERIALIZE_INCOMPLETE;
        }

        if (own)
        {
            p->own = 1;
            p->value = ont_parser_strdup((const char*)data);
        }
        else{
            p->value = (char*)data;
        }

        *len  = val_len + 1;
    }
    else{
        *len = 0;
    }

    next_op();
    return ONT_ERR_OK;
}

int deserialize_bytes_t(const char* data, size_t* len, int8_t own, ont_parser_list_node_t** node, ont_parser_node_op_t** op)
{
    ont_parser_bytes_t* p = (ont_parser_bytes_t*)(*node);
    uint16_t val_len = 0;
    if (!p->filter || !p->filter(p->filter_node))
    {
        if (*len < 2)
        {
            return ONT_ERR_DESERIALIZE_INCOMPLETE;
        }
        ont_parser_read_integer(data, (char*)(&val_len), 2);

        if (*len < val_len + 2)
        {
            return ONT_ERR_DESERIALIZE_INCOMPLETE;
        }

        if (own)
        {
            p->own = 1;
            p->value = ont_parser_strndup((const char*)(data + 2), val_len);
        }
        else{
            p->value = (char*)(data + 2);
        }
        p->len = val_len;
        *len  = val_len + 2;
    }
    else{
        *len = 0;
    }

    next_op();
    return ONT_ERR_OK;
}

int deserialize_binary_t(const char* data, size_t* len, int8_t own, ont_parser_list_node_t** node, ont_parser_node_op_t** op)
{
    ont_parser_bytes_t* p = (ont_parser_bytes_t*)(*node);
    uint32_t val_len = 0;
    if (!p->filter || !p->filter(p->filter_node))
    {
        if (*len < 4)
        {
            return ONT_ERR_DESERIALIZE_INCOMPLETE;
        }

        ont_parser_read_integer(data, (char*)(&val_len), 4);
        if (*len < val_len + 4)
        {
            return ONT_ERR_DESERIALIZE_INCOMPLETE;
        }

        if (own)
        {
            p->own = 1;
            p->value = ont_parser_strndup((const char*)(data + 4), val_len);
        }
        else{
            p->value = (char*)(data + 4);
        }
        p->len = val_len;
        *len  = val_len + 4;
    }
    else{
        *len = 0;
    }
    next_op();
    return ONT_ERR_OK;
}

int deserialize_raw_t(const char* data, size_t* len, int8_t own, ont_parser_list_node_t** node, ont_parser_node_op_t** op)
{
    ont_parser_raw_t* p = (ont_parser_raw_t*)(*node);
    if (!p->filter || !p->filter(p->filter_node))
    {
        if (*len < p->len)
        {
            return ONT_ERR_DESERIALIZE_INCOMPLETE;
        }

        if (p->len)
        {
            if (own)
            {
                p->own = 1;
                p->value = ont_parser_strndup((const char*)data, p->len);
            }
            else{
                p->value = (char*)data;
            }
        }
        *len  = p->len;
    }
    else{
        *len = 0;
    }
    next_op();
    return ONT_ERR_OK;
}

int deserialize_compress_uint32_t(const char* data, size_t* len, int8_t own, ont_parser_list_node_t** node, ont_parser_node_op_t** op)
{
    ont_parser_compress_uint32_t* p = (ont_parser_compress_uint32_t*)(*node);
    uint32_t multiplier = 1;
    uint8_t vint8 = 0;
    size_t i = 0;
    uint8_t count = 4;

    if (!p->filter || !p->filter(p->filter_node))
    {
        for (i=0; i<4; ++i)
        {
            if (count == 0 || *len == 0)
            {
                return ONT_ERR_DESERIALIZE_INCOMPLETE;
            }

            vint8 = *data;
            if ((i == 3) && (vint8 & 0x80))
            {
                return ONT_ERR_REMAIN_LEN_OVER_RANGE;
            }
            p->value += (uint32_t)(vint8 & 0x7f) * multiplier;
            multiplier *= 0x80;

            --count;
            --(*len);
            ++data;
            if ((vint8 & 0x80) == 0){
                break;
            }
        }
    }

    *len = 4 - count;

    next_op();
    return ONT_ERR_OK;
}

#undef def_deserialize_func
#undef deserialize_num
#undef deserialize_error
#undef next_op

#define def_node_handlers(type)                         \
    ont_parser_node_op_t g_ont_parser_node_##type = {   \
        init_##type##_t,                                \
        len_##type##_t,                                 \
        serialize_##type##_t,                           \
        deserialize_##type##_t,                         \
        destroy_##type##_t};

    def_node_handlers(uint8)
        def_node_handlers(uint16)
        def_node_handlers(uint32)
#ifdef ONT_PLATFORM_FLOATING_POINT_SUPPORT
        def_node_handlers(double)
#endif /* ONT_PLATFORM_FLOATING_POINT_SUPPORT */
        def_node_handlers(string)
        def_node_handlers(bytes)
        def_node_handlers(binary)
        def_node_handlers(raw)
        def_node_handlers(compress_uint32)
