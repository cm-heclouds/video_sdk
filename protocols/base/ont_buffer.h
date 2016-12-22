#ifndef  _ONT_BUFFER_H_
#define  _ONT_BUFFER_H_

#include "ont/platform.h"

# ifdef __cplusplus
extern "C" {
# endif

/**
* buffer struct
*/
typedef struct ont_buffer
{
	char * begin;			/** Begin pointer of buffer */  
	
	char * end;				/** End pointer of buffer   */

	char * current;			/**	Current pointer of buffer  */
}ont_buffer_t;

/**
 * [ont_buffer_create description]
 * @param  buf       [description]
 * @param  queuesize [description]
 * @return           [description]
 */
int  ont_buffer_create( ont_buffer_t * buf, const size_t buffersize);

/**
 * [ont_buffer_destory description]
 * @param  buf [description]
 * @return     [description]
 */
int  ont_buffer_destory( ont_buffer_t * buf);

/**
 * [ont_buffer_get_free_space description]
 * @param  buf           [description]
 * @param  free_buf      [description]
 * @param  free_buf_size [description]
 * @return               [description]
 */
int  ont_buffer_get_free_space( ont_buffer_t * buf, char ** free_buf, size_t * free_buf_size);

/**
 * [ont_buffer_add_used_space description]
 * @param  buf       [description]
 * @param  used_size [description]
 * @return           [description]
 */
int  ont_buffer_add_used_space( ont_buffer_t * buf, const size_t used_size);

/**
 * [ont_buffer_get_used_space description]
 * @param  buf           [description]
 * @param  used_buf      [description]
 * @param  used_buf_size [description]
 * @return               [description]
 */
int  ont_buffer_get_used_space( ont_buffer_t * buf, char ** used_buf, size_t * used_buf_size);

/**
 * [ont_buffer_free_used_space description]
 * @param  buf  [description]
 * @param  size [description]
 * @return      [description]
 */
int  ont_buffer_free_used_space( ont_buffer_t * buf, const size_t size);


# ifdef __cplusplus
}      /* extern "C" */
# endif


#endif /* _ONT_BUFFER_H_ */
