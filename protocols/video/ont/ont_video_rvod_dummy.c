#ifndef ONT_VIDEO_RVOD_MP4SUPPORTED

#include "ont/platform.h"
#include "librtmp/rtmp.h"
#include "librtmp/rtmp_sys.h"
#include "librtmp/log.h"
#include "librtmp/amf.h"
#include "ont/video.h"
#include "ont/video_rvod.h"

void  rtmp_rvod_seek_notify(void* ctx, double ts)
{
    (void)ctx;
    (void)ts;
}


int rtmp_rvod_send_media_singlestep(t_rtmp_vod_ctx* ctx)
{
    if (!ctx || !ctx->rtmp)
    {
        return -1;
    }
    return 0;
}

int rtmp_rvod_parsefile(t_rtmp_vod_ctx* ctx, const char *file)
{
    if (!ctx)
    {
        return -1;
    }
    (void)file;
    return 0;
}

int rtmp_rvod_closefile(t_rtmp_vod_ctx* ctx)
{
    if (!ctx)
    {
        return -1;
    }
    return 0;    
}

#endif
