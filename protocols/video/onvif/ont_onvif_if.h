#ifndef _ONT_ONVIF_IF_H_
#define _ONT_ONVIF_IF_H_


# ifdef __cplusplus
extern "C" {
# endif

typedef struct ont_onvif_playctx_t {
    void *rtmp_client; /*RTMP handle.*/
    void *rtsp_client;
	void *play_env;
	unsigned long startts;
	unsigned long last_sndts;/*latest send timestamp*/
    int channel;
	RTMPMetadata meta;
    unsigned char *tempBuf;
    unsigned tempBufSize;
    int state; /*0 normal, 1. need stop*/
}ont_onvif_playctx;

void *ont_onvifdevice_create_playenv();

void ont_onvifdevice_delete_playenv(void *env);


void* ont_onvifdevice_live_stream_start(void *env, int channel, const  char *push_url, const char *deviceid, int level);

int ont_onvifdevice_stream_ctrl(void *playctx, int level);
int ont_onvifdevice_live_stream_singlestep(void *env, int maxdelay);

void ont_onvifdevice_live_stream_stop(void *playctx);

# ifdef __cplusplus
}
# endif


#endif
