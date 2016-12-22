#ifndef _ONT_ONVIF_IF_H_
#define _ONT_ONVIF_IF_H_


# ifdef __cplusplus
extern "C" {
# endif

struct ont_onvif_playctx {
    void *rtmp_client; /*RTMP handle.*/
    void *rtsp_client;
	void *device_client;
	unsigned long startts;
    int channel;
    char eventLoopWatchVariable;
	RTMPMetadata meta;
    unsigned char *tempBuf;
    unsigned tempBufSize;
};


void* ont_onvifdevice_live_stream_start(int channel, const  char *push_url, const char *deviceid, int level);
int ont_onvifdevice_stream_ctrl(void *ctx, int level);
int ont_onvifdevice_live_stream_singlestep(void *ctx);

void ont_onvifdevice_live_stream_stop(void *ctx);

# ifdef __cplusplus
}
# endif


#endif
