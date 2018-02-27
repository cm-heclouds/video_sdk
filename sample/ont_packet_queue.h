#ifndef _ONT_PACKET_QUEUE_H
#define _ONT_PACKET_QUEUE_H

#include "rvod.h"
#include "ont_onvif_if.h"
#include "video_rvod_mp4.h"

# ifdef __cplusplus
extern "C" {
# endif


typedef struct _RTMPMetaSpsPpsData{
    unsigned char  latestSps[1000];
    uint32_t  sps_len;
    unsigned char  latestPps[100];
    uint32_t  pps_len;
}RTMPMetaSpsPpsData;

typedef struct _RTMPAudioHeaderData{
    unsigned char audioTag;
    unsigned int audiotype;
}RTMPAudioHeaderData;

typedef struct _RTMPVideoAudioCtl{
    int isKeyFram;
    int isAudioHeader;

    unsigned int next_videosampleid;
    unsigned int next_audiosampleid;
}RTMPVideoAudioCtl;

/**RTMP packed data */
typedef struct _RTMPPackData {
    char *data;
    unsigned size;
    unsigned deltaTs;
    int codecType;

    /*for live*/
    unsigned long sndts;/*send timestamp*/

    RTMPMetaSpsPpsData  *mspd;
    RTMPAudioHeaderData *ahd;
    RTMPVideoAudioCtl   *ctl;
}RTMPPackData;

typedef struct _t_rtmp_mode_ctx {
    ont_onvif_playctx   *onvif_ctx;
    t_rtmp_mp4_ctx      *mp4_ctx;
    int                 type; /*LIVE_MODE or RVOD_MODE*/
} t_rtmp_mode_ctx;


int RTMPPackEnqueue(void *ctx, unsigned char* pFrameBuf, int size,
        unsigned int deltaTs, int codecType, void *data, RTMPVideoAudioCtl *ctl);
int RTMPPackDequeue(void *ctx);
int RTMPPackClearqueue(void *ctx);


int ont_set_rtmp_packet_handle(ont_onvif_playctx *ctx, HandlerProc proc, void *arg);

void _rtmp_packet_handle_proc(void* cliendData, int mask);

int ont_disable_rtmp_packet_handle(ont_onvif_playctx *ctx);

#define CODEC_H264        1
#define CODEC_MPEG4A      11

#define LIVE_MODE 1
#define RVOD_MODE 2

# ifdef __cplusplus
}
# endif


#endif
