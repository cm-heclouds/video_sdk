#ifndef ONT_SRC_COMM_PARSER_MACROS_H_
#define ONT_SRC_COMM_PARSER_MACROS_H_

#include "types.h"
#include "packet.h"

#ifdef __cplusplus
extern "C" {
#endif

extern ont_parser_node_op_t g_ont_parser_node_uint8;
extern ont_parser_node_op_t g_ont_parser_node_uint16;
extern ont_parser_node_op_t g_ont_parser_node_uint32;
#ifdef ONT_PLATFORM_FLOATING_POINT_SUPPORT
extern ont_parser_node_op_t g_ont_parser_node_double;
#endif /* ONT_PLATFORM_FLOATING_POINT_SUPPORT */
extern ont_parser_node_op_t g_ont_parser_node_string;
extern ont_parser_node_op_t g_ont_parser_node_bytes;
extern ont_parser_node_op_t g_ont_parser_node_binary;
extern ont_parser_node_op_t g_ont_parser_node_raw;
extern ont_parser_node_op_t g_ont_parser_node_compress_uint32;

#define ont_parser_def_packet_begin(pkt_name)   \
    typedef struct pkt_name{                    \
        ont_parser_helper_t head;

#define ont_parser_add_elem(name, type)             \
    ont_parser_##type##_t name;

#define ont_parser_add_ref_elem(name)           \
    ont_parser_uint8_t name;

#define ont_parser_def_packet_end(pkt_name)         \
    ont_parser_helper_t none;                       \
    }pkt_name;

#define ont_parser_init_begin(pkt_name, first_node, type)   \
    int ont_parser_init_##pkt_name(pkt_name* pkt){          \
        ont_parser_list_node_t* cur_node = NULL;            \
        ont_parser_node_op_t* cur_node_op = NULL;           \
        ont_parser_list_node_t** last_node;                 \
        ont_parser_node_op_t** last_node_op;                \
        int rc = 0;                                         \
        pkt->head.sibling.next = &pkt->first_node.sibling;  \
        pkt->head.sibling.ref_node = NULL;                  \
        pkt->head.op = &g_ont_parser_node_##type;           \
        pkt->first_node.filter = NULL;                      \
        pkt->first_node.sibling.next = NULL;                \
        pkt->first_node.sibling.ref_node = NULL;            \
        pkt->first_node.filter_node = NULL;                 \
        last_node = &pkt->first_node.sibling.next;          \
        last_node_op = &pkt->first_node.op;

#define ont_parser_queue_up(node, type, filter_func, filter_ref)    \
    pkt->node.filter = filter_func;                                 \
    pkt->node.filter_node = &pkt->filter_ref.sibling;               \
    pkt->node.sibling.next = NULL;                                  \
    pkt->node.sibling.ref_node = NULL;                              \
    *last_node = &pkt->node.sibling;                                \
    last_node = &pkt->node.sibling.next;                            \
    *last_node_op = &g_ont_parser_node_##type;                      \
    last_node_op = &pkt->node.op;

#define ont_parser_queue_up_ref_elem(node, ref, beg, end)   \
    pkt->node.filter = NULL;                                \
    pkt->node.filter_node = NULL;                           \
    pkt->node.sibling.next = NULL;                          \
    pkt->node.sibling.ref_node = pkt->ref.sibling.ref_node; \
    pkt->node.ref_beg = beg;                                \
    pkt->node.ref_end = end;                                \
    pkt->ref.sibling.ref_node = &pkt->node.sibling;

#define ont_parser_init_end()                               \
    (void) last_node;                                       \
    (void) last_node_op;                                    \
    cur_node = pkt->head.sibling.next;                      \
    cur_node_op = pkt->head.op;                             \
    while (cur_node){                                       \
        rc = cur_node_op->init(&cur_node, &cur_node_op);    \
        if (rc){                                            \
            return rc;                                      \
        }                                                   \
    }                                                       \
    return ONT_ERR_OK;                                      \
}

#ifdef __cplusplus
}      /* extern "C" */
#endif

#endif
