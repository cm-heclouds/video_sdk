#ifndef ONT_VIDEO_RVOD_MP4SUPPORTED

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include <log.h>
#include <rtmp.h>
#include <device.h>

#include "rvod.h"
#include "platform_cmd.h"
#include "ont_packet_queue.h"
#include "sample_config.h"

#include <mp4v2/mp4v2.h>
#include <mp4v2/platform.h>
#include <mp4v2/general.h>
#include "video_record.h"


int ont_record_channel_cmd_update(void *dev, int32_t channel, int32_t status, int32_t seconds, char url[255])
{
	return -1;
}

int32_t ont_record_check(void *dev, int32_t channel, ont_onvif_playctx *onvif_ctx)
{
	return -1;
}

t_dev_record_ctx *ont_record_start(void *dev, int32_t channel, void *onvif_ctx)
{
	return NULL;
}

int ont_record_append_sps_pps( void *dev, int32_t channel, int32_t fps, int32_t width, int32_t height, const char *sps, int32_t sps_len, const char *pps, int32_t pps_len)
{
	return -1;
}

int ont_record_append_video( void *dev, int32_t channel,  uint64_t ts, char *data, char *len, uint8_t isKeyframe)
{
	return -1;
}

int ont_record_append_audio_sepciacfg( void *dev, int32_t channel, const char *data, int32_t len)
{
	return -1;
}

int ont_record_append_audio( void *dev, int32_t channel, uint64_t ts, char *data, char *len)
{
	return -1;
}

int ont_record_stop(void *dev, int32_t channel)
{
	return -1;
}

int ont_record_mp4_send_media_singlestep(t_rtmp_vod_ctx *rctx)
{
	return -1;
}

t_rtmp_vod_ctx *ont_record_send_start(void *dev, int32_t channel)
{
	return NULL;
}

int ont_record_intime(void *dev, int32_t channel)
{
	return -1;
}

int  ont_record_singlestep(t_dev_record_ctx *rc)
{
	return -1;
}

int ont_record_send_check(void *dev, int32_t channel)
{
	return -1;
}

uint64_t MP4ConvertFromTrackTimestamp(
    MP4FileHandle hFile,
    MP4TrackId    trackId,
    MP4Timestamp  timeStamp,
    uint32_t      timeScale)
{
	return 1;
}

#endif
