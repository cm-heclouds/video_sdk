#include <stdlib.h>
#include <string.h>
#include "platform.h"
#include "device.h"
#include "rtmp_if.h"
#include "rvod.h"
#include "rtmp.h"
#include "rtmp_sys.h"
#include "rtmp_log.h"
#include "amf.h"

t_rtmp_vod_ctx *rtmp_rvod_createctx(void *dev, void *user_data, t_ont_rvod_callbacks *vodcb)
{
    /*ont_platform_malloc*/
    t_rtmp_vod_ctx *ctx = (t_rtmp_vod_ctx*)ont_platform_malloc(sizeof(t_rtmp_vod_ctx));
    memset(ctx, 0x00, sizeof(t_rtmp_vod_ctx));
    ctx->dev = dev;
    ctx->user_data =user_data;
    memcpy(&ctx->callback, vodcb, sizeof(*vodcb));
    
    return ctx;
}

int rtmp_rvod_destoryctx(t_rtmp_vod_ctx* ctx)
{
    if (!ctx)
    {
        return -1;
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
    t_rtmp_vod_ctx *c = ctx;
    c->status = 2;
    c->callback.stop(c);
}

static void  rtmp_rvod_pause_notify(void* ctx, int paused, double ts)
{
    if (!ctx)
    {
        return;
    }
    t_rtmp_vod_ctx *c = ctx;
    c->callback.pause(c, paused, ts);
}

int  rtmp_rvod_seek_notify(t_rtmp_vod_ctx* ctx, double ts)
{
    /*seek to the specified type*/
    if (!ctx)
    {
        return -1;
    }
    t_rtmp_vod_ctx *c = ctx;
    c->callback.seek(c, ts);
    return 0;
}


int rtmp_rvod_start(t_rtmp_vod_ctx* ctx,  const char *pushurl, int timeout)
{
    RTMP *rtmp;
    char  dev_buf[32];
    ont_device_t *dev;
    if (!ctx)
    {
        return -1;
    }
    if (ctx->rtmp)
    {
        return -1;
    }
    dev= (ont_device_t *)ctx->dev;
	ont_platform_snprintf(dev_buf, sizeof(dev_buf), "%lld", dev->device_id);
	rtmp = rtmp_create_publishstream(pushurl, timeout, dev_buf);
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
    ctx->status =1;
	return 0;
}


int rtmp_rvod_stop(t_rtmp_vod_ctx* ctx)
{
    if (!ctx || !ctx->rtmp)
    {
        return -1;
    }
    RTMP *rtmp = ctx->rtmp;
    RTMP_Close(rtmp);
    RTMP_Free(rtmp);
    return 0;
}
