#include "ont_channel.h"
#include "ont_base_common.h"
#include "ont_buffer.h"
#include "ont/error.h"
#include "string.h"
#include "ont_tcp_channel.h"

int ont_tcp_channel_send(ont_socket_t * socket, const char *buf, size_t buf_size, size_t *bytes_snd)
{
	int iret = 0;

	const char * buf_left_addr = buf;
	unsigned int bytessnd = 0;
	size_t left_size = buf_size;
	do
	{
		bytessnd = 0;
		iret = ont_platform_tcp_send(socket, buf_left_addr, left_size, &bytessnd);
		if (iret)
		{
			break;
		}
		buf_left_addr += bytessnd;
		left_size -= bytessnd;
		if (left_size == 0)
		{
			break;
		}
	} while (bytessnd/* == 0*/);

	*bytes_snd = buf_size - left_size;

	return iret;
}

int ont_tcp_channel_check_timeout(tcp_channel_t *ch, int32_t now)
{
	if (now - ch->last_active_time >= ch->timeout)
	{
		return 1;
	}
	return 0;
}

int ont_tcp_channel_initilize(void* channel, cb_on_read_packet fn_read)
{
	tcp_channel_t* ctx = NULL;
	ont_channel_interface_t * tcp_ch = NULL;

	ONT_RETURN_IF_NIL2(channel, fn_read, ONT_ERR_BADPARAM);
	ctx = (tcp_channel_t*)channel;

	if (ctx->ch_status == tcp_ch_status_initilize)
	{
		return ONT_ERR_OK;
	}
	
	tcp_ch = ctx->channel;
	ONT_RETURN_IF_NIL(tcp_ch, ONT_ERR_BADPARAM);
	tcp_ch->fn_cb_read_packet = fn_read;

	ctx->ch_status = tcp_ch_status_initilize;
	return ONT_ERR_OK;
}

int ont_tcp_channel_disconnect(void* channel)
{
	tcp_channel_t* ctx = NULL;
	int iret = 0;
	ONT_RETURN_IF_NIL(channel, ONT_ERR_BADPARAM);
	ctx = (tcp_channel_t*)channel;

	if (ctx->ch_status == tcp_ch_status_disconnect)
	{
		return ONT_ERR_OK;
	}

	if (ctx->ch_status == tcp_ch_status_connect)
	{
		iret = ont_platform_tcp_close(ctx->socket);
		ONT_RETURN_ON_ERR(iret, iret);

		ctx->connect_req_time = 0;
		ctx->ch_status = tcp_ch_status_disconnect;
	}
	
	return ONT_ERR_OK;
}

int ont_tcp_channel_connect(void* channel, volatile int *exit)
{
	tcp_channel_t* ctx = NULL;
	int32_t now = 0;
	int iret = 0;
	ctx = (tcp_channel_t*)channel;

	ONT_RETURN_IF_NIL(ctx, ONT_ERR_BADPARAM);

	now = ont_platform_time();
	if (ctx->connect_req_time == 0)
	{
		iret = ont_platform_tcp_create(&ctx->socket);
		ONT_RETURN_ON_ERR(iret, iret);
		ctx->ch_status = tcp_ch_status_connect;
	}

	ctx->connect_timeout = ctx->timeout;
	ctx->connect_req_time = ctx->last_active_time = now;
	ctx->exit = exit;

	/* check tcp connect  */
	do
	{
		ctx->conn_status = ont_platform_tcp_connect(ctx->socket, ctx->addr, ctx->port);

		if (*(ctx->exit))
			return ONT_ERR_DISCONNECTED_BY_USER;

		if (ctx->conn_status == ONT_ERR_OK ||
			ctx->conn_status == ONT_ERR_SOCKET_ISCONN)
		{
			return ONT_ERR_OK;
		}

		if (ctx->conn_status == ONT_ERR_SOCKET_OP_FAIL ||
			ctx->conn_status == ONT_ERR_BADPARAM)
		{
			break;
		}

		ont_platform_sleep(100);
		now = ont_platform_time();
		if (now - ctx->connect_req_time >= ctx->connect_timeout)
		{
			ctx->conn_status = ONT_ERR_TIMEOUT;
			break;
		}

		if (ctx->conn_status == ONT_ERR_SOCKET_INPROGRESS)
		{
			continue;
		}
		else
		{
			break;
		}
	} while (1);

	return ctx->conn_status;
}


int ont_tcp_channel_deinitilize(void* ch_ctx)
{
	tcp_channel_t* ctx = NULL;
	int iret = 0;
	ONT_RETURN_IF_NIL(ch_ctx,ONT_ERR_BADPARAM);
	ctx = (tcp_channel_t*)ch_ctx;
	if (ctx->ch_status == tcp_ch_status_deinitilize)
	{
	    return ONT_ERR_OK;
	}

	if (ctx->ch_status < tcp_ch_status_disconnect)
	{
	iret = ont_platform_tcp_close(ctx->socket);
		ONT_RETURN_ON_ERR(iret, iret);
	}
	
	ont_platform_free(ctx->buffer_in.begin);  
	ctx->buffer_in.begin = NULL;

	ont_platform_free(ctx->buffer_out.begin); 
	ctx->buffer_out.begin = NULL;

	ont_platform_free(ctx); 
	ctx = ch_ctx = NULL;

	return ONT_ERR_OK;
}

size_t ont_tcp_channel_left_write_buf_size(void* channel)
{
	size_t buf_size = 0;
	tcp_channel_t * ctx = NULL;
	char * buf_used = NULL;
	ctx = (tcp_channel_t*)channel;

	ont_buffer_get_used_space(&ctx->buffer_out, &buf_used, &buf_size);
	
	return buf_size;
}

int ont_tcp_channel_write(void* ch_ctx, const char* buf, const size_t size)
{
	tcp_channel_t * ctx = NULL;
	ont_buffer_t * buffer_out = NULL;
	size_t bytes_snd = 0;
	const char * buf_left_addr = NULL;
	size_t left_size = size;
	char * buf_free = NULL;
	size_t buf_free_size = 0;

	const char * src = NULL;
	char * dst = NULL;
	int iret = 0;

	ONT_RETURN_IF_NIL3(ch_ctx, buf, size, 1);
	ctx = (tcp_channel_t*)ch_ctx;
	buffer_out = &ctx->buffer_out;

	if (ctx->conn_status == ONT_ERR_SOCKET_OP_FAIL)
	{
		return ONT_ERR_SOCKET_OP_FAIL;
	}

	/* check channel buffer  */
	iret = ont_buffer_get_free_space(buffer_out, &buf_free, &buf_free_size);
	ONT_RETURN_ON_ERR(iret, ONT_ERR_BUFFER_NOT_ENOUGH);

	if (left_size > buf_free_size)
	{
		return ONT_ERR_BUFFER_NOT_ENOUGH;
	}

	iret = ont_tcp_channel_send(ctx->socket, buf, left_size, &bytes_snd);
	left_size -= bytes_snd;
	ctx->last_active_time = ont_platform_time();

	/*	left_size = 9;*/
	if (left_size == 0)
	{
		return ONT_ERR_OK;
	}

	if (iret == ONT_ERR_SOCKET_OP_FAIL)
	{
		ctx->conn_status = ONT_ERR_SOCKET_OP_FAIL;
		return iret;
	}
	
	/* cache these send fail */
	buf_left_addr = buf + bytes_snd;

	src = buf_left_addr;
	dst = buf_free;

	for (; src != buf_left_addr + left_size; ++src, ++dst)
	{
		*dst = *src;
	}

	iret = ont_buffer_add_used_space(buffer_out, left_size);
	ONT_RETURN_ON_ERR(iret,iret);
	
	return ONT_ERR_OK;
}

/*
  1. err_ok
  2. Read_timeout
  3. return read errcode if read complete package
*/

int ont_tcp_channel_process(void* ch_ctx)
{
	tcp_channel_t * ctx = NULL;
	char * buf_used = NULL;
	size_t buf_used_size = 0;
	int iret = 0;
	int iret_packet_err = -1;
	ont_buffer_t * buffer_out = NULL;
	
	size_t bytes_snd = 0;
	ont_buffer_t * buffer_in = NULL;
	char * buf_recv = NULL;
	size_t buf_size = 0;
	unsigned int bytes_recv = 0;

	size_t packet_read_size = 0;
	int32_t now = 0;

	ctx = (tcp_channel_t*)ch_ctx;
	ONT_RETURN_IF_NIL(ch_ctx, 1);

	if (*(ctx->exit))
		return ONT_ERR_DISCONNECTED_BY_USER;

	if (ctx->conn_status == ONT_ERR_SOCKET_OP_FAIL)
	{
		/* ctx->channel->fn_cb_close(ctx->owner_ctx); */
		return ONT_ERR_SOCKET_OP_FAIL;
	}

	/* send througth tcp */
	buffer_out = &ctx->buffer_out;
	if (!ont_buffer_get_used_space(buffer_out, &buf_used, &buf_used_size) && buf_used_size > 0)
	{
		iret = ont_tcp_channel_send(ctx->socket, (const char*)buf_used, buf_used_size, &bytes_snd);
		if (iret == ONT_ERR_SOCKET_OP_FAIL)
		{
			ctx->conn_status = ONT_ERR_SOCKET_OP_FAIL;
			/* ctx->channel->fn_cb_close(ctx->owner_ctx); */
			return ONT_ERR_SOCKET_OP_FAIL;
		}
		ont_buffer_free_used_space(buffer_out, bytes_snd);
	}


	/* ---Begin:recv from tcp--- */
	buffer_in = &ctx->buffer_in;
	ont_buffer_get_free_space(buffer_in, &buf_recv,&buf_size);
	
	bytes_recv = 0;
	iret = ont_platform_tcp_recv(ctx->socket, buf_recv, buf_size, &bytes_recv);
	if (iret == ONT_ERR_SOCKET_OP_FAIL)
	{	
		ctx->conn_status = ONT_ERR_SOCKET_OP_FAIL;
		/* ctx->channel->fn_cb_close(ctx->owner_ctx); */
		return ONT_ERR_SOCKET_OP_FAIL;
	}

	if (bytes_recv)
	{
		ont_buffer_add_used_space(buffer_in, (unsigned int)bytes_recv);
	}

	/* read callback and free used buffer */
	buf_size = 0;
	buf_recv = 0;
	ont_buffer_get_used_space(buffer_in, &buf_recv, &buf_size);

	now = ont_platform_time();
	if (buf_size)
	{
		iret_packet_err = ctx->channel->fn_cb_read_packet(ctx->owner_ctx, buf_recv, buf_size, &packet_read_size);
		if (packet_read_size)
		{
			ctx->last_active_time = now;
			ont_buffer_free_used_space(buffer_in, packet_read_size);
			packet_read_size = 0;
			return iret_packet_err;
		}
	}
	
	if (ont_tcp_channel_check_timeout(ctx,now))
	{
		return ONT_ERR_TIMEOUT;
	}
	
	/* ---End:recv from tcp--- */
	return ONT_ERR_OK;
}


int ont_tcp_channel_create(ont_channel_interface_t *self,
	const char * addr, const uint16_t port,
		const size_t in_buf_size, const size_t out_buf_size, 
		void * user_ctx, const uint16_t timeout)
{
	tcp_channel_t * ch = NULL;

	ONT_RETURN_IF_NIL2(self, addr, 1);
	self->fn_initilize = ont_tcp_channel_initilize;
	self->fn_deinitilize = ont_tcp_channel_deinitilize;
	self->fn_write = ont_tcp_channel_write;
	self->fn_process = ont_tcp_channel_process;
	self->fn_connect = ont_tcp_channel_connect;
	self->fn_disconnect = ont_tcp_channel_disconnect;
	self->fn_left_write_buf_size = ont_tcp_channel_left_write_buf_size;

	ch = ont_platform_malloc(sizeof(tcp_channel_t));
	if (!ch)
	{
		return ONT_ERR_NOMEM;
	}

	self->channel = ch;
	ch->channel = self;
	if (!self->channel)
	{
		return ONT_ERR_NOMEM;
	}

	if (ont_buffer_create(&ch->buffer_in, in_buf_size) ||
		ont_buffer_create(&ch->buffer_out, out_buf_size))
	{
		return ONT_ERR_NOMEM;
	}

	ch->addr = addr;
	ch->port = port;
	ch->owner_ctx = user_ctx;
	ch->timeout = timeout;
	ch->connect_req_time = 0;

	ch->ch_status = tcp_ch_status_create;
	return ONT_ERR_OK;
}
