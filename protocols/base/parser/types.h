#ifndef ONT_SRC_COMM_PARSER_TYPES_H_
#define ONT_SRC_COMM_PARSER_TYPES_H_

#include "ont/platform.h"
#include "ont/error.h"
#include "ont/config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ont_parser_list_node_t {
    struct ont_parser_list_node_t *next;
    struct ont_parser_list_node_t *ref_node;
}ont_parser_list_node_t;

typedef struct ont_parser_node_op_t{
    int (*init)(ont_parser_list_node_t** node, struct ont_parser_node_op_t** op);
    int (*len)(ont_parser_list_node_t** node, struct ont_parser_node_op_t** op, size_t* len);
    int (*serialize)(char* data, size_t* len, ont_parser_list_node_t** node, struct ont_parser_node_op_t** op);
    int (*deserialize)(const char* data, size_t* len, int8_t own, ont_parser_list_node_t** node, struct ont_parser_node_op_t** op);
    int (*destroy)(ont_parser_list_node_t** node, struct ont_parser_node_op_t** op);
}ont_parser_node_op_t;

/**
 * sibling : next node in list
 * filter_node : the param of filter callback
 * name : the name of the node
 * op : callbacks
 * own : the destroy() will free it if it's 1, otherwise it's 0
 */

#define add_ont_parser_type(type, type_name)                \
    typedef struct{                                         \
        ont_parser_list_node_t sibling;                     \
        ont_parser_node_op_t* op;                           \
        int32_t (*filter)(ont_parser_list_node_t* node);    \
        ont_parser_list_node_t* filter_node;                \
        int8_t own;                                         \
        int8_t ref_beg;                                     \
        int8_t ref_end;                                     \
        type value;                                         \
    }ont_parser_##type_name##_t;

add_ont_parser_type(uint8_t, uint8)
add_ont_parser_type(uint16_t, uint16)
add_ont_parser_type(uint32_t, uint32)
#ifdef ONT_PLATFORM_FLOATING_POINT_SUPPORT
add_ont_parser_type(double, double)
#endif /* ONT_PLATFORM_FLOATING_POINT_SUPPORT */
add_ont_parser_type(const char*, string)
add_ont_parser_type(uint32_t, compress_uint32)

#define add_ont_parser_type_with_len(type_name)             \
    typedef struct{                                         \
        ont_parser_list_node_t sibling;                     \
        ont_parser_node_op_t* op;                           \
        int32_t (*filter)(ont_parser_list_node_t* node);    \
        ont_parser_list_node_t* filter_node;                \
        int8_t own;                                         \
        const char* value;                                  \
        size_t len;                                         \
    }ont_parser_##type_name##_t;

add_ont_parser_type_with_len(bytes)
add_ont_parser_type_with_len(binary)
add_ont_parser_type_with_len(raw)

#undef add_ont_parser_type_with_len
#undef add_ont_parser_type

typedef struct{
    ont_parser_list_node_t sibling;
    ont_parser_node_op_t* op;
}ont_parser_helper_t;

typedef enum{
    kOntParserErrSerialize,
    kOntParserErrDeserialize,
    kOntParserErrNotReachTheEnd,
    kUnisdkOpTypeCount
}ont_parser_err_type;

#define ONT_EXPECT_ERR_OK(rc)                   \
    if (rc != ONT_ERR_OK){                      \
        return rc;                              \
    }

#ifdef __cplusplus
}      /* extern "C" */
#endif

#endif
