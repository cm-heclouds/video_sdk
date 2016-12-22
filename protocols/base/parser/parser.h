#ifndef ONT_SRC_COMM_PARSER_PARSER_H_
#define ONT_SRC_COMM_PARSER_PARSER_H_

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

int ont_parser_len(ont_parser_helper_t* head, size_t* len);
int ont_parser_serialize(char* data, size_t len, ont_parser_helper_t* head);
/*
 * own: malloc memory for string, bytes binary or not
 * 0 : not, 1 : malloc
 */
int ont_parser_deserialize(const char* data, size_t len, ont_parser_helper_t* head, int8_t own);
int ont_parser_destroy(ont_parser_helper_t* head);

#ifdef __cplusplus
}      /* extern "C" */
#endif

#endif
