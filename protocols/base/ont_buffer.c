#include "ont_buffer.h"
#include "ont_base_common.h"
#include "ont/error.h"

int  ont_buffer_create( ont_buffer_t * buf, const size_t buffersize)
{
	ONT_RETURN_IF_NIL2(buf, buffersize, ONT_ERR_BADPARAM);

	buf->begin = ont_platform_malloc(buffersize);
    if (!buf->begin)
    {
		return ONT_ERR_NOMEM;
    }

	buf->end = buf->begin + buffersize;
    buf->current = buf->begin;

	return ONT_ERR_OK;
}

int  ont_buffer_destory( ont_buffer_t * buf)
{
	ONT_RETURN_IF_NIL(buf, ONT_ERR_BADPARAM);
	if (buf->begin)
	{
		ont_platform_free(buf->begin);
		buf->begin = NULL;
	}
	return ONT_ERR_OK;
}

int  ont_buffer_get_free_space( ont_buffer_t * buf, char ** free_buf, size_t * free_buf_size)
{
	ONT_RETURN_IF_NIL3(buf, free_buf, free_buf_size, ONT_ERR_BADPARAM);

	*free_buf_size = buf->end - buf->current;
    *free_buf = buf->current;

	return ONT_ERR_OK;
}

int  ont_buffer_add_used_space( ont_buffer_t * buf, const size_t used_size)
{
	ONT_RETURN_IF_NIL(buf, ONT_ERR_BADPARAM);

	if (buf->current + used_size > buf->end)
	{
		return ONT_ERR_BADPARAM;
	}
	buf->current += used_size;
	return ONT_ERR_OK;
}

int  ont_buffer_get_used_space( ont_buffer_t * buf, char ** used_buf, size_t * used_buf_size)
{	
	ONT_RETURN_IF_NIL3(buf, used_buf, used_buf_size, ONT_ERR_BADPARAM);

	*used_buf_size = buf->current - buf->begin;
    *used_buf = buf->begin;

	return ONT_ERR_OK;
}

int  ont_buffer_free_used_space( ont_buffer_t * buf, const size_t size)
{
	char *byte_to_copy = NULL;
	char *it = NULL;
	ONT_RETURN_IF_NIL2(buf, size, ONT_ERR_BADPARAM);
	if (buf->current - size < buf->begin)
	{
		return ONT_ERR_BADPARAM;
	}

	byte_to_copy = buf->begin + size;
	it = buf->begin;
	for (; byte_to_copy != buf->current; ++byte_to_copy, ++it)
		*it = *byte_to_copy;

	buf->current -= size;

	return ONT_ERR_OK;
}