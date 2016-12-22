#ifndef ONT_SRC_COMM_PARSER_PACKET_H_
#define ONT_SRC_COMM_PARSER_PACKET_H_

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

int init_uint8_t(ont_parser_list_node_t** node, ont_parser_node_op_t** op);
int init_uint16_t(ont_parser_list_node_t** node, ont_parser_node_op_t** op);
int init_uint32_t(ont_parser_list_node_t** node, ont_parser_node_op_t** op);
#ifdef ONT_PLATFORM_FLOATING_POINT_SUPPORT
int init_double_t(ont_parser_list_node_t** node, ont_parser_node_op_t** op);
#endif /* ONT_PLATFORM_FLOATING_POINT_SUPPORT */
int init_string_t(ont_parser_list_node_t** node, ont_parser_node_op_t** op);
int init_bytes_t(ont_parser_list_node_t** node, ont_parser_node_op_t** op);
int init_binary_t(ont_parser_list_node_t** node, ont_parser_node_op_t** op);
int init_raw_t(ont_parser_list_node_t** node, ont_parser_node_op_t** op);
int init_compress_uint32_t(ont_parser_list_node_t** node, ont_parser_node_op_t** op);

int destroy_uint8_t(ont_parser_list_node_t** node, ont_parser_node_op_t** op);
int destroy_uint16_t(ont_parser_list_node_t** node, ont_parser_node_op_t** op);
int destroy_uint32_t(ont_parser_list_node_t** node, ont_parser_node_op_t** op);
#ifdef ONT_PLATFORM_FLOATING_POINT_SUPPORT
int destroy_double_t(ont_parser_list_node_t** node, ont_parser_node_op_t** op);
#endif /* ONT_PLATFORM_FLOATING_POINT_SUPPORT */
int destroy_string_t(ont_parser_list_node_t** node, ont_parser_node_op_t** op);
int destroy_bytes_t(ont_parser_list_node_t** node, ont_parser_node_op_t** op);
int destroy_binary_t(ont_parser_list_node_t** node, ont_parser_node_op_t** op);
int destroy_raw_t(ont_parser_list_node_t** node, ont_parser_node_op_t** op);
int destroy_compress_uint32_t(ont_parser_list_node_t** node, ont_parser_node_op_t** op);

int len_uint8_t(ont_parser_list_node_t** node, ont_parser_node_op_t** op, size_t* len);
int len_uint16_t(ont_parser_list_node_t** node, ont_parser_node_op_t** op, size_t* len);
int len_uint32_t(ont_parser_list_node_t** node, ont_parser_node_op_t** op, size_t* len);
#ifdef ONT_PLATFORM_FLOATING_POINT_SUPPORT
int len_double_t(ont_parser_list_node_t** node, ont_parser_node_op_t** op, size_t* len);
#endif /* ONT_PLATFORM_FLOATING_POINT_SUPPORT */
int len_string_t(ont_parser_list_node_t** node, ont_parser_node_op_t** op, size_t* len);
int len_bytes_t(ont_parser_list_node_t** node, ont_parser_node_op_t** op, size_t* len);
int len_binary_t(ont_parser_list_node_t** node, ont_parser_node_op_t** op, size_t* len);
int len_raw_t(ont_parser_list_node_t** node, ont_parser_node_op_t** op, size_t* len);
int len_compress_uint32_t(ont_parser_list_node_t** node, ont_parser_node_op_t** op, size_t* len);

int serialize_uint8_t(char* data, size_t* len, ont_parser_list_node_t** node, ont_parser_node_op_t** op);
int serialize_uint16_t(char* data, size_t* len, ont_parser_list_node_t** node, ont_parser_node_op_t** op);
int serialize_uint32_t(char* data, size_t* len, ont_parser_list_node_t** node, ont_parser_node_op_t** op);
#ifdef ONT_PLATFORM_FLOATING_POINT_SUPPORT
int serialize_double_t(char* data, size_t* len, ont_parser_list_node_t** node, ont_parser_node_op_t** op);
#endif /* ONT_PLATFORM_FLOATING_POINT_SUPPORT */
int serialize_string_t(char* data, size_t* len, ont_parser_list_node_t** node, ont_parser_node_op_t** op);
int serialize_bytes_t(char* data, size_t* len, ont_parser_list_node_t** node, ont_parser_node_op_t** op);
int serialize_binary_t(char* data, size_t* len, ont_parser_list_node_t** node, ont_parser_node_op_t** op);
int serialize_raw_t(char* data, size_t* len, ont_parser_list_node_t** node, ont_parser_node_op_t** op);
int serialize_compress_uint32_t(char* data, size_t* len, ont_parser_list_node_t** node, ont_parser_node_op_t** op);

int deserialize_uint8_t(const char* data, size_t* len, int8_t own, ont_parser_list_node_t** node, ont_parser_node_op_t** op);
int deserialize_uint16_t(const char* data, size_t* len, int8_t own, ont_parser_list_node_t** node, ont_parser_node_op_t** op);
int deserialize_uint32_t(const char* data, size_t* len, int8_t own, ont_parser_list_node_t** node, ont_parser_node_op_t** op);
#ifdef ONT_PLATFORM_FLOATING_POINT_SUPPORT
int deserialize_double_t(const char* data, size_t* len, int8_t own, ont_parser_list_node_t** node, ont_parser_node_op_t** op);
#endif /* ONT_PLATFORM_FLOATING_POINT_SUPPORT */
int deserialize_string_t(const char* data, size_t* len, int8_t own, ont_parser_list_node_t** node, ont_parser_node_op_t** op);
int deserialize_bytes_t(const char* data, size_t* len, int8_t own, ont_parser_list_node_t** node, ont_parser_node_op_t** op);
int deserialize_binary_t(const char* data, size_t* len, int8_t own, ont_parser_list_node_t** node, ont_parser_node_op_t** op);
int deserialize_raw_t(const char* data, size_t* len, int8_t own, ont_parser_list_node_t** node, ont_parser_node_op_t** op);
int deserialize_compress_uint32_t(const char* data, size_t* len, int8_t own, ont_parser_list_node_t** node, ont_parser_node_op_t** op);

#ifdef __cplusplus
}      /* extern "C" */
#endif

#endif
