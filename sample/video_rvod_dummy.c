#ifndef ONT_VIDEO_RVOD_MP4SUPPORTED

#include "rtmp.h"
#include "video_rvod_mp4.h"

void  rtmp_rvod_mp4_pause_notify(void *ctx, int paused, double ts)
{

}

int  rtmp_rvod_mp4_seek_notify(void *ctx, double ts)
{
	return 0;
}

void  rtmp_rvod_mp4_stop_notify(void *ctx)
{

}

int rtmp_rvod_mp4_parse(t_rtmp_mp4_ctx *ctx, const char *file)
{
	return 0;
}

int rtmp_rvod_mp4_clear(t_rtmp_mp4_ctx *ctx)
{
	return 0;
}


int rtmp_rvod_mp4_send_media_singlestep(t_rtmp_vod_ctx *rctx, int speed)
{
	return 0;
}

int rtmp_rvod_select(t_rtmp_vod_ctx *ctx)
{
	return 0;
}

int rtmp_rvod_mp4_start(t_rtmp_mp4_ctx *ctx)
{
	return 0;
}

#endif
