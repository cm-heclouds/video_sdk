#include "parser.h"

int ont_parser_len(ont_parser_helper_t* head, size_t* len)
{
    ont_parser_node_op_t* op = head->op;
    ont_parser_list_node_t* node = head->sibling.next;
    size_t val_len = 0;
    int rc = 0;
    *len = 0;

    while (node)
    {
        val_len = 0;
        rc = op->len(&node, &op, &val_len);
        ONT_EXPECT_ERR_OK(rc);
        *len += val_len;
    }

    return ONT_ERR_OK;
}

int ont_parser_serialize(char* data, size_t len, ont_parser_helper_t* head)
{
    ont_parser_node_op_t* op = head->op;
    ont_parser_list_node_t* node = head->sibling.next;
    size_t elem_len = len;
    size_t offset = 0;
    int rc = 0;

    while (node)
    {
        rc = op->serialize(data+offset, &elem_len, &node, &op);
        ONT_EXPECT_ERR_OK(rc);
        offset += elem_len;
        elem_len = len - offset;
    }

    return ONT_ERR_OK;
}

int ont_parser_deserialize(const char* data, size_t len,
                           ont_parser_helper_t* head, int8_t own)
{
    ont_parser_node_op_t* op = head->op;
    ont_parser_list_node_t* node = head->sibling.next;
    size_t elem_len = len;
    size_t offset = 0;
    int rc = 0;

    while (node)
    {
        rc = op->deserialize(data+offset, &elem_len, own, &node, &op);
        ONT_EXPECT_ERR_OK(rc);

        offset += elem_len;
        elem_len = len - offset;
    }

    if (offset < len)
    {
        return ONT_ERR_DESERIALIZE_DATA_LEFT;
    }

    return ONT_ERR_OK;
}


int ont_parser_destroy(ont_parser_helper_t* head)
{
    ont_parser_node_op_t* op = head->op;
    ont_parser_list_node_t* node = head->sibling.next;
    int rc = 0;

    while (node)
    {
        rc = op->destroy(&node, &op);
        ONT_EXPECT_ERR_OK(rc);
    }

    return ONT_ERR_OK;
}
