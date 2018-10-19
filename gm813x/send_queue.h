#ifndef _SEND_QUEUE_H
#define _SEND_QUEUE_H

# ifdef __cplusplus
extern "C" {
# endif


/**RTMP packed data */
typedef struct _PackBufferData {
	char type;
	char keyframe;
	char *data;
	unsigned int size;
	int ts;
	int first;
} t_PackBufferData;


/*1 video, 0 audio*/
int g813x_PackEnqueue(gm_enc_bitstream_t *bs, int type);

int g813x_RTMPPackDequeue(void *ctx);

int g813x_clearqueue(void *ctx);

int g813x_init();


# ifdef __cplusplus
}
# endif


#endif

