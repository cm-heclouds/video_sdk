#include <stdlib.h>
#include <string.h>
#include "ont/platform.h"
#include "librtmp/rtmp.h"
#include "librtmp/rtmp_sys.h"
#include "librtmp/log.h"
#include "librtmp/amf.h"
#include "ont/video.h"
#include "ont/video_rvod.h"


t_rtmp_vod_ctx *rtmp_rvod_createctx()
{
    /*ont_platform_malloc*/
    t_rtmp_vod_ctx *ctx = (t_rtmp_vod_ctx*)ont_platform_malloc(sizeof(t_rtmp_vod_ctx));
    memset(ctx, 0x00, sizeof(t_rtmp_vod_ctx));
    return ctx;
}

int rtmp_rvod_destoryctx(t_rtmp_vod_ctx* ctx)
{
    if (!ctx)
    {
        return -1;
    }
    if (ctx->sps)
    {
        ont_platform_free(ctx->sps);
    }
    if (ctx->pps)
    {
        ont_platform_free(ctx->pps);
    }
    if (ctx->video_buf)
    {
        ont_platform_free(ctx->video_buf);
    }
    if (ctx->audio_buf)
    {
        ont_platform_free(ctx->audio_buf);
    }
    if (ctx->audiocfg) /**/
    {
        ont_platform_free(ctx->audiocfg);
    }
    ont_platform_free(ctx);
    return 0;
}

int rtmp_rvod_send_videoeof(t_rtmp_vod_ctx* ctx)
{
    RTMP *rtmp;

    static  AVal av_level = AVC("status");
    static  AVal av_playstatus_code = AVC("NetStream.Play.Complete");
    static  AVal av_status_code = AVC("NetStream.Play.Stop");
    static  AVal av_desc = AVC("Stopped");

    if (!ctx || !ctx->rtmp)
    {
        return -1;
    }
    rtmp = ctx->rtmp;

    RTMP_SendCtrl(rtmp, 1, 0, 0);
    /*send play status*/
    RTMP_SendPlayStatus(rtmp, &av_level, &av_playstatus_code, 0, 0);
    RTMP_SendStatus(rtmp, &av_level, &av_status_code, &av_desc);

    return 0;
}


static void  rtmp_rvod_stop_notify(void* ctx)
{
    if (!ctx)
    {
        return;
    }
    RTMP_LogPrintf("rtmp stopeed\n");
    t_rtmp_vod_ctx *c = ctx;
    c->stopeed = 1;

}

static void  rtmp_rvod_pause_notify(void* ctx, int paused, double ts)
{
    RTMP_LogPrintf("rtmp pause %d ts %lf\n", paused, ts);
    if (!ctx)
    {
        return;
    }
    t_rtmp_vod_ctx *c = ctx;
    c->paused = paused;

    if (paused)
    {
        c->start_timestamp = ts;
    }
    else
    {
		c->start_timestamp = ts;
        c->epoch = RTMP_GetTime();
    }
}


int rtmp_rvod_start(t_rtmp_vod_ctx* ctx, const char *publishurl, const char*deviceid)
{
    RTMP *rtmp;

    if (!ctx)
    {
        return -1;
    }
    if (ctx->rtmp)
    {
        return -1;
    }
	rtmp = rtmp_create_publishstream(publishurl, 30, deviceid);
    ctx->rtmp = rtmp;

    if (!rtmp)
    {
        /*create connection error*/
        return -1;
    }

    /*set the notificaiton function*/
    rtmp->pause_notify = rtmp_rvod_pause_notify;
    rtmp->seek_notify = rtmp_rvod_seek_notify;
    rtmp->stop_notify = rtmp_rvod_stop_notify;

    rtmp->rvodCtx = ctx;

    ctx->epoch = RTMP_GetTime();
    ctx->start_timestamp = 0;
	ont_platform_sleep(300);/* for rvod, need sleep a while to avoid server drop frame*/
    rtmp_send_metadata(rtmp, &ctx->meta);

    ctx->next_videosampleid = 1; /*start from 1*/
    ctx->next_audiosampleid = 1;
	return 0;
}


int rtmp_rvod_stop(t_rtmp_vod_ctx* ctx)
{
    if (!ctx || !ctx->rtmp)
    {
        return -1;
    }
    RTMP *rtmp = ctx->rtmp;
    rtmp_rvod_closefile(ctx);
    RTMP_Close(rtmp);
    RTMP_Free(rtmp);
    return 0;
}
