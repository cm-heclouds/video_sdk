#ifndef  __ONT_TCP_CHANNEL_H_
#define  __ONT_TCP_CHANNEL_H_ 

#include "ont_channel.h"
#include "ont_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tcp_channel
{
	ont_channel_interface_t * channel;
	ont_socket_t            * socket;
	const char				* addr;
	uint16_t                  port;
	uint16_t                  connect_timeout;
	uint32_t                  connect_req_time;
	uint16_t				  timeout;
	uint32_t                  last_active_time;
	int     				  conn_status;
	ont_buffer_t 			  buffer_in;
	ont_buffer_t 			  buffer_out;
	void					* owner_ctx;
	volatile int            * exit;
	int						  ch_status;
} tcp_channel_t;

enum tcp_channel_status
{
	tcp_ch_status_create     = 0,
	tcp_ch_status_initilize  = 1,
	tcp_ch_status_connect    = 2,
	tcp_ch_status_disconnect = 3,
	tcp_ch_status_deinitilize = 4
};

int ont_tcp_channel_initilize(void* channel, cb_on_read_packet fn_read);

int ont_tcp_channel_connect(void* channel, volatile int *exit);

int ont_tcp_channel_disconnect(void* channel);

int ont_tcp_channel_deinitilize(void* ch_ctx);

int ont_tcp_channel_write(void* ch_ctx, const char* buf, const size_t size);

size_t ont_tcp_channel_left_write_buf_size(void* channel);

int ont_tcp_channel_process(void* ch_ctx);


/**
* [ont_tcp_channel_create description]
* @param  self         [description]
* @param  addr         [description]
* @param  port         [description]
* @param  in_buf_size  [description]
* @param  out_buf_size [description]
* @param  user_ctx     [description]
* @return              [description]
*/
int ont_tcp_channel_create(ont_channel_interface_t *self,const char * addr, const uint16_t port,
						   const size_t in_buf_size, const size_t out_buf_size,
						   void * user_ctx, const uint16_t timeout);


#ifdef __cplusplus
}      /* extern "C" */
#endif

#endif