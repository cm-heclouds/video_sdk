#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <rtmp_if.h>
#include <log.h>
#include <device.h>
#include "ont_list.h"
#include <fcntl.h>
#include <pthread.h>
#include "librtmp_send264.h"
#include "gmlib.h"

#define MAX_BUF_SIZE   20

#include "send_queue.h"

ont_list_t  *_list = NULL;

pthread_mutex_t _mx_rtmp = PTHREAD_MUTEX_INITIALIZER; /* control access to the concentrator */



int g813x_PackEnqueue(gm_enc_bitstream_t *bs, int type)
{
	t_PackBufferData *pkdata;

	if (ont_list_size(_list) > MAX_BUF_SIZE) {
		printf("drop, type %d, ts %d\n", type, bs->timestamp);
		free(bs->bs_buf);
		return -1;
	}
	pkdata = (t_PackBufferData *)ont_platform_malloc(sizeof(t_PackBufferData));
	if (pkdata == NULL) {

		return -1;
	}

	pkdata->data = bs->bs_buf;
	pkdata->size = bs->bs_len;
	pkdata->ts = bs->timestamp;
	pkdata->type = type;
	pkdata->keyframe = bs->keyframe;
	pthread_mutex_lock(&_mx_rtmp);
	ont_list_insert(_list, pkdata);
	pthread_mutex_unlock(&_mx_rtmp);

	/* return zero, i guess */
	return 0;
}



int g813x_send(t_PackBufferData *data)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
#if 0 /* unused variable */
	int last = tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
	if (data->type == 1) {
		GM8136_Send(data->data, data->size, data->keyframe, data->ts);
	} else {
		/* to avoid warning by using type cast */
		GM8136_SendAAC((unsigned char *)data->data, data->size, data->ts);
	}
	gettimeofday(&tv, NULL);
	//printf("send time is :%d type %d size:%d, key %d, ts %d\n", (tv.tv_sec*1000 + tv.tv_usec/1000)-last, data->type, data->size, data->keyframe, data->ts);

	/* return zero, i guess */
	return 0;
}



int g813x_init()
{
	if (!_list) {
		_list = ont_list_create();
	}
	return 0;
}

int g813x_RTMPPackDequeue(void *ctx)
{
	t_PackBufferData *_data;

	do {
		_data = NULL;

		pthread_mutex_lock(&_mx_rtmp);
		/* to avoid warning by using type cast */
		ont_list_pop_front(_list, (void **)&_data);
		pthread_mutex_unlock(&_mx_rtmp);

		if (_data) {
			g813x_send(_data);
			ont_platform_free(_data->data);
			ont_platform_free(_data);
		}
	} while (_data);

	/* return zero, i guess */
	return 0;
}

int g813x_clearqueue(void *ctx)
{
	t_PackBufferData *_data;

	do {
		_data = NULL;
		/* to avoid warning by using type cast */
		ont_list_pop_front(_list, (void **)&_data);
		if (_data) {
			ont_platform_free(_data->data);
			ont_platform_free(_data);
		}
	} while (_data);

	/* return zero, i guess */
	return 0;
}

