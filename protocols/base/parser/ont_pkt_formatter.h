#ifndef ONT_SRC_COMM_PARSER_ONT_PKT_FORMATTER_H_
#define ONT_SRC_COMM_PARSER_ONT_PKT_FORMATTER_H_

#include "ont/platform.h"
#include "macros.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t first_dp;           /* first datapoint or not */
    uint8_t count;
    uint32_t counter_offset;
    uint32_t tm;
}ont_datapoint_type8_t;

typedef struct{
    char* data;
    uint32_t len;
}ont_pkt_formatter_result_t;

ont_parser_def_packet_begin(fix_header_t)
ont_parser_add_elem(type, uint8)
ont_parser_add_elem(remain_len, compress_uint32)
ont_parser_def_packet_end(fix_header_t)

typedef struct ont_pkt_formatter_t{
    fix_header_t header;
    uint32_t cap;
    uint32_t offset;
    char* buf;
    ont_datapoint_type8_t dp8;  /* for datapoint type 8 */
    ont_pkt_formatter_result_t result;
}ont_pkt_formatter_t;

int ont_pkt_formatter_init(ont_pkt_formatter_t* formatter);
int ont_pkt_formatter_realloc(ont_pkt_formatter_t* formatter, size_t len);
void ont_pkt_formatter_reset(ont_pkt_formatter_t* formatter, int8_t free_mem);
int ont_pkt_formatter_add_uint32(ont_pkt_formatter_t* formatter, const char* ds, uint32_t value);

#ifdef ONT_PLATFORM_FLOATING_POINT_SUPPORT
int ont_pkt_formatter_add_double(ont_pkt_formatter_t* formatter, const char* ds, double value);
#endif /* ONT_PLATFORM_FLOATING_POINT_SUPPORT */

int ont_pkt_formatter_add_string(ont_pkt_formatter_t* formatter, const char* ds, const char* value);
int ont_pkt_formatter_add_binary(ont_pkt_formatter_t* formatter, const char* ds, const char* value, uint32_t len);
int ont_pkt_formatter_add_json(ont_pkt_formatter_t* formatter, const char* ds, const char* value);
int ont_pkt_formatter_add_finish(ont_pkt_formatter_t* formatter);
int ont_pkt_formatter_destroy(ont_pkt_formatter_t* formatter);

/*
 * format the node given by head, add the type and remain length
 * by call ont_pkt_formatter_finish()
 * visit the formatted data by formatter.result.len and formatter.result.data
 */
int ont_pkt_serialize_finish(ont_pkt_formatter_t* formatter, uint8_t type, ont_parser_helper_t* head);

/*
 * add the type and remain length
 * visit the formatted data by formatter.result.len and formatter.result.data
 */
int ont_pkt_formatter_finish(ont_pkt_formatter_t* formatter);

/***  packet define ***/
/* time */
ont_parser_def_packet_begin(formatter_time_t)
ont_parser_add_elem(type, uint8)
ont_parser_add_elem(tm, uint32)
ont_parser_def_packet_end(formatter_time_t)

/* int32 */
ont_parser_def_packet_begin(formatter_uint32_t)
ont_parser_add_elem(type, uint8)
ont_parser_add_elem(ds, string)
ont_parser_add_elem(value, uint32)
ont_parser_def_packet_end(formatter_uint32_t)

/* double */
#ifdef ONT_PLATFORM_FLOATING_POINT_SUPPORT
ont_parser_def_packet_begin(formatter_double_t)
ont_parser_add_elem(type, uint8)
ont_parser_add_elem(ds, string)
ont_parser_add_elem(value, double)
ont_parser_def_packet_end(formatter_double_t)
#endif /* ONT_PLATFORM_FLOATING_POINT_SUPPORT */

/* string */
ont_parser_def_packet_begin(formatter_string_t)
ont_parser_add_elem(type, uint8)
ont_parser_add_elem(ds, string)
ont_parser_add_elem(value, string)
ont_parser_def_packet_end(formatter_string_t)

/* binary */
ont_parser_def_packet_begin(formatter_binary_t)
ont_parser_add_elem(type, uint8)
ont_parser_add_elem(ds, string)
ont_parser_add_elem(value, binary)
ont_parser_def_packet_end(formatter_binary_t)

/* json */
ont_parser_def_packet_begin(formatter_json_t)
ont_parser_add_elem(type, uint8)
ont_parser_add_elem(ds, string)
ont_parser_add_elem(value, string)
ont_parser_def_packet_end(formatter_json_t)

#ifdef __cplusplus
}      /* extern "C" */
#endif

#endif
