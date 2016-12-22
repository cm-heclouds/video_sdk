#ifndef  _ONT_CHANNEL_H_
#define  _ONT_CHANNEL_H_

#include "ont/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * [int description]
 * @param  user_ctx		[user_ctx of the channel creator]
 * @param  buf			[address of data received]
 * @param  buf_size		[size of the data received]
 * @param  read_size	[when the buf is greater or equal one or more completed packet ,set read_size = read packet size ,or set it 0 ]
 * @return				[description]
 */
typedef  int(*cb_on_read_packet)(void *user_ctx,const char* buf, size_t buf_size, size_t * read_size);

/**
 * [int description]
 * @param  user_ctx [description]
 * @return          [description]
 */
typedef  int(*cb_on_close)(void *user_ctx);

/**
 * [int description]
 * @param  fn_close [description]
 * @return          [description]
 */
typedef  int(*ont_channel_initilize)(void* channel, cb_on_read_packet fn_read);

/**
 * [int description]
 * @param  channel [description]
 * @return         [description]
 */
typedef  int(*ont_channel_connect)(void* channel, volatile int *exit);

/**
* [ont_channel_disconnect description]
* @param  channel [description]
* @return         [description]
*/
typedef  int(*ont_channel_disconnect)(void* channel);

/**
 * [int description]
 * @param  channel [description]
 * @return         [description]
 */
typedef  int(*ont_channel_deinitilize)(void* channel);

/**
 * [int description]
 * @param  size [description]
 * @return      [description]
 */
typedef  int(*ont_channel_write)(void* channel, const char* buf, const size_t size);

/**
* [int description]
* @param  size [description]
* @return      [description]
*/
typedef  size_t(*ont_channel_left_write_buf_size)(void* channel);
/**
 * [ont_channel_process]
 * @param  channel [description]
 * @return         [ONT_ERR_OK|ONT_ERR_TIMEOUT|retcode:{read errcode if read complete package}]
 */
typedef  int(*ont_channel_process)(void* channel);


typedef struct {
	void * 	channel;

	ont_channel_initilize  			fn_initilize;
	ont_channel_deinitilize 		fn_deinitilize;
	ont_channel_connect				fn_connect;
	ont_channel_disconnect			fn_disconnect;
	ont_channel_write				fn_write;
	ont_channel_left_write_buf_size fn_left_write_buf_size;
	ont_channel_process			    fn_process;

	cb_on_read_packet           fn_cb_read_packet;
}ont_channel_interface_t;



#ifdef __cplusplus
}      /* extern "C" */ 
#endif

#endif /* _CHANNEL_H_ */
