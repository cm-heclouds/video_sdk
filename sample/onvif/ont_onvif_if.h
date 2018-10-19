#ifndef _ONT_ONVIF_IF_H_
#define _ONT_ONVIF_IF_H_

#include "ont_list.h"
#include "rtmp_if.h"

# ifdef __cplusplus
extern "C" {
# endif

#ifndef _ONT_HANDLER_PROC_
#define _ONT_HANDLER_PROC_
typedef void HandlerProc(void* clientData, int mask);
#endif

typedef struct ont_onvif_playctx_t {
    void *rtmp_client; /*RTMP handle.*/
    void *rtsp_client;
	unsigned char key_send;
	void *play_env;
	unsigned long startts;
    char audio_seq[2];
	unsigned long last_sndts;/*latest send timestamp*/
    int channel;
	RTMPMetadata meta;
    unsigned char *tempBuf;
    unsigned tempBufSize;
    int state; /*0 normal, 1. need stop*/
    int push_model; /* 1. push live stream to ont; 0. just discard*/
    ont_list_t *packDataList;
    unsigned int sendmeta[2]; /*0 video, 1 audio*/
    int fd; /*rtmp_client fd*/
    int rtsp_level;
    char rtsp_playurl[255];
    unsigned long last_rcvts;/*latest receive timestamp*/
    void *cluster;
}ont_onvif_playctx;


int ont_onvifdevice_live_stream_play(void *playctx, const char* push_url, const char* deviceid);
void* ont_onvifdevice_live_stream_start( void        *env,
                                         void        *cluster,
                                         int          channel,
                                         const  char *push_url,
                                         const char  *deviceid,
                                         int          level );

int ont_onvifdevice_stream_ctrl(void *playctx, int level);
int ont_onvifdevice_live_stream_singlestep(void *env, int maxdelay);

/*stop rtmp publish, keep local rtsp play*/
void ont_onvifdevice_live_stream_stop(void *playctx);


void ont_onvifdevice_rtmp_stop(void *playctx);

# ifdef __cplusplus
}
# endif


#endif
