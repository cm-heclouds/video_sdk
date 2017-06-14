#ifndef _ONT_VIDEO_RVOD_H_
#define _ONT_VIDEO_RVOD_H_
#include "config.h"

#ifdef ONT_PROTOCOL_VIDEO

# ifdef __cplusplus
extern "C" {
# endif

#define M_RTMP_CLIENTBUFLEN 3000

/**********************************/
/*远程录像回放暂停                 */
/*ctx,    rtmp回放指针             */
/*paused, 暂停或者恢复播放         */
/*ts, 暂停或者播放时的时间戳        */
/**********************************/
typedef void(*rtmp_pause_notify)(void* ctx, int paused, double ts);

/**********************************/
/*远程录像拖动到指定路劲            */
/*cxt, rtmp回放指针                */
/*ts,  拖动到需要播放的位置        */
/**********************************/
typedef void(*rtmp_seek_notify)(void* cxt, double ts);

/**********************************/
/*通知回放客户端播放完成，停止播放*/
/*cxt, rtmp回放指针                */
/**********************************/
typedef void(*rtmp_stop_notify)(void* cxt);

typedef struct _t_rtmp_vod_ctx
{
    void * fileHanle;
    void * rtmp;
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
    unsigned int stopeed;
    
    unsigned int start_timestamp;
    unsigned int last_videotimestamp;
    unsigned int last_audiotimestamp;
    unsigned int epoch;
    unsigned int next_videosampleid;
    unsigned int next_audiosampleid;
    unsigned char *video_buf;
    unsigned char *audio_buf;
}t_rtmp_vod_ctx;



t_rtmp_vod_ctx *rtmp_rvod_createctx();
int rtmp_rvod_destoryctx(t_rtmp_vod_ctx* ctx);
int rtmp_rvod_start(t_rtmp_vod_ctx* ctx, const char* publishurl, const char *deviceid);
int rtmp_rvod_stop(t_rtmp_vod_ctx* ctx);
int rtmp_rvod_send_videoeof(t_rtmp_vod_ctx* ctx); 

/************参考MP4文件录像播放实现自定义录像文件播放******************/
/*解析文件*/
int rtmp_rvod_parsefile(t_rtmp_vod_ctx* ctx, const char *file);

/*关闭文件*/
int rtmp_rvod_closefile(t_rtmp_vod_ctx* ctx);

/* 实现文件的拖动功能 */
void  rtmp_rvod_seek_notify(void *ctx, double ts);

/*发送录像音视频数据*/
int rtmp_rvod_send_media_singlestep(t_rtmp_vod_ctx* ctx);

/***********************************************************************/

# ifdef __cplusplus
}
# endif


#endif /*ONT_PROTOCOL_VIDEO*/

#endif
