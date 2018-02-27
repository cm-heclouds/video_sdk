#ifndef _VIDEO_RVOD_MP4_H_
#define _VIDEO_RVOD_MP4_H_



typedef struct _t_rtmp_mp4_ctx
{
	t_rtmp_vod_ctx * rvodctx;
    void * fileHanle; 
    RTMPMetadata meta;
    unsigned int videoTrackId;
    unsigned int audioTrackId;
    unsigned int videosamplemaxsize;
    unsigned int audiosamplemaxsize;
    unsigned int videoSamples;
    unsigned int audioSamples;
    unsigned int hasAudio;
    unsigned int audioCodecid; /*support AAC*/
    double audioDataRate;
    unsigned int audioSampleRate;
    unsigned char audioHeaderTag;
    char *pps;
    unsigned int ppsLen;
    char *sps;
    unsigned int spsLen;
    char  *audiocfg;
    unsigned int audioconfigLen;

    unsigned int paused;
    unsigned int seeked[2]; /*0 video, 1 audio*/
    unsigned int stoped;

    unsigned int start_timestamp;
    unsigned int last_videotimestamp;
    unsigned int last_audiotimestamp;
    unsigned int epoch;
    unsigned int next_videosampleid;
    unsigned int next_audiosampleid;
    unsigned char *video_buf;
    unsigned char *audio_buf;

    void    *packDataList;
    unsigned int sendmeta[2]; /*0 video, 1 audio*/
    int state; /*0 normal, 1. need stop*/
}t_rtmp_mp4_ctx;


void  rtmp_rvod_mp4_pause_notify(void* ctx, int paused, double ts);

int  rtmp_rvod_mp4_seek_notify(void *ctx, double ts);

void  rtmp_rvod_mp4_stop_notify(void *ctx);



int rtmp_rvod_mp4_parse(t_rtmp_mp4_ctx *ctx, const char *file);
int rtmp_rvod_mp4_start(t_rtmp_mp4_ctx *ctx);

int rtmp_rvod_mp4_clear(t_rtmp_mp4_ctx *ctx);

int rtmp_rvod_mp4_send_media_singlestep(t_rtmp_vod_ctx* ctx);

int rtmp_rvod_select(t_rtmp_vod_ctx * ctx);

#endif
