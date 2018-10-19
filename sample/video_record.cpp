#ifdef ONT_VIDEO_RVOD_MP4SUPPORTED


#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include <log.h>
#include <rtmp.h>
#include <device.h>
#include "mp4v2/mp4v2.h"

#include "rvod.h"
#include "platform_cmd.h"
#include "ont_packet_queue.h"
#include "sample_config.h"
#include "video_record.h"
#include "video_rvod_mp4.h"
#include "ont_bytes.h"

#ifdef WIN32
#define RECORD_PATH "e:\\mp4record"
#else
#define RECORD_PATH "mp4record.mp4"
#endif

static int _g_record_num = 0;
t_dev_record_ctx *_g_records = NULL;

static t_dev_record_ctx *_ont_record_get_channel(void *dev, int32_t channel)
{
	int i = 0;
	for (; i < _g_record_num; i++) {
		if (_g_records[i].channel == channel) {
			return &_g_records[i];
		}
	}
	return NULL;
}


int ont_record_channel_cmd_update(void *dev, int32_t channel, int32_t status, int32_t seconds, char url[255])
{
	if (_g_records == NULL) {
		_g_record_num = cfg_get_channel_num();
		_g_records = (t_dev_record_ctx *)ont_platform_malloc(sizeof(t_dev_record_ctx) * _g_record_num);
		memset(_g_records, 0x00, sizeof(t_dev_record_ctx)*_g_record_num);
	}
	t_dev_record_ctx *rc = _ont_record_get_channel(dev, channel);
	if (!rc) {
		rc = &_g_records[0];
	}
	rc->channel = channel;
	rc->status = status;
	if (status == 2) {
		return 0;
	}
	struct ont_timeval tv;
	ont_gettimeofday(&tv, NULL);

	rc->record_starttime = tv.tv_sec - 1; /*redundance 1 seconds*/
	rc->record_endtime = tv.tv_sec + 1 + seconds;
	ont_platform_snprintf(rc->url, sizeof(rc->url), "%s", url);
	return 0;
}


int ont_record_intime(void *dev, int32_t channel)
{
	t_dev_record_ctx *rc = _ont_record_get_channel(dev, channel);
	if (!rc) {
		return -1;
	}
	if (rc->stop != 0) {
		rc->stop = 0;
		return -1;
	}
	struct ont_timeval tv;
	ont_gettimeofday(&tv, NULL);

	if (tv.tv_sec >= rc->record_starttime && tv.tv_sec <= rc->record_endtime) {
		return 0;
	}
	return -1;
}


int ont_record_check(void *dev, int32_t channel, ont_onvif_playctx *onvif_ctx)
{

	if (onvif_ctx->rtmp_client) {
		return -1;
	}
	t_dev_record_ctx *rc = _ont_record_get_channel(dev, channel);
	if (!rc) {
		return -1;
	}

	struct ont_timeval tv;
	ont_gettimeofday(&tv, NULL);

	if (tv.tv_sec >= rc->record_starttime && tv.tv_sec <= rc->record_endtime) {
		if (rc->state < 2) {
			return 0;
		}
		return -1;
	} else {
		return -1;
	}

}


static int _ont_record_append_sps_pps( void *dev, int32_t channel, int32_t fps, int32_t width, int32_t height, const char *sps, int32_t sps_len, const char *pps, int32_t pps_len)
{
	t_dev_record_ctx *rc = _ont_record_get_channel(dev, channel);
	if (!rc) {
		return -1;
	}
	MP4SetTimeScale(rc->hMP4File, 1000);

	rc->video = MP4AddH264VideoTrack(rc->hMP4File, 1000, 1000 / fps, width, height, sps[1], sps[2], sps[3], 3);

	MP4AddH264SequenceParameterSet(rc->hMP4File, rc->video, (const uint8_t *)sps, sps_len);
	MP4AddH264PictureParameterSet(rc->hMP4File, rc->video, (const uint8_t *)pps, pps_len);

	return 0;
}

static int _ont_record_append_audio_sepciacfg( void *dev, int32_t channel, const char *data, int32_t len)
{
	t_dev_record_ctx *rc = _ont_record_get_channel(dev, channel);
	if (!rc) {
		return -1;
	}
	static uint32_t      aac_sample_rates[] = {
		96000, 88200, 64000, 48000,
		44100, 32000, 24000, 22050,
		16000, 12000, 11025,  8000,
		7350,     0,     0,     0
	};

	int idx = (data[0] & 0xe) | ((data[1] & 0x80) >> 7);

	rc->audio = MP4AddAudioTrack(rc->hMP4File, aac_sample_rates[idx], 1024, MP4_MPEG4_AUDIO_TYPE);

	MP4SetTrackESConfiguration(rc->hMP4File, rc->audio, (const uint8_t *)data, len);

	/* add return statement later */
	return 0;
}


t_dev_record_ctx *ont_record_start(void *dev, int32_t channel, void *onvif_ctx)
{
	char path[64];

	struct ont_timeval tv;
	ont_gettimeofday(&tv, NULL);

	t_dev_record_ctx *rc = _ont_record_get_channel(dev, channel);
	if (rc) {
		ont_platform_snprintf(path, sizeof(path), "%s%d_%d.mp4", RECORD_PATH, channel, ont_platform_time());
		rc->hMP4File = MP4Create(path, 0);
		rc->onvif_ctx = onvif_ctx;
		rc->state = 2;
		rc->recorded_starttime = tv.tv_sec;
		ont_platform_snprintf(rc->path, sizeof(path), "%s", path);
		return rc;
	} else {
		return NULL;
	}
}


#if 0 /* unused variable */
static char _h264_delimiter[] = { 0x00, 0x00, 0x00, 0x01 };
#endif

int ont_record_append_video( void *dev, int32_t channel,  uint64_t ts, char *data, int32_t len, uint8_t isKeyframe)
{
	t_dev_record_ctx *rc = _ont_record_get_channel(dev, channel);
	if (!rc) {
		return -1;
	}
	/*skip FLV header*/
	data += 5;
	//memcpy(data, _h264_delimiter, 4);
	//uint32_t* p = (uint32_t*)data;
	ont_encodeInt32(data, len - 9);

	MP4WriteSample(rc->hMP4File, rc->video, (const uint8_t *)data, len - 5, MP4_INVALID_DURATION, 0, isKeyframe ? true : false);
	static int count = 0;
	count++;
	RTMP_Log(RTMP_LOGINFO, "append %d, key %d, count %d\n", len, isKeyframe, count);

	/* add return statement later */
	return 0;
}



int ont_record_append_audio( void *dev, int32_t channel, uint64_t ts, const char *data, uint32_t len)
{
	t_dev_record_ctx *rc = _ont_record_get_channel(dev, channel);
	if (!rc) {
		return -1;
	}

	MP4WriteSample(rc->hMP4File, rc->audio, (const uint8_t *)data, len, MP4_INVALID_DURATION, 0, 0);

	/* add return statement later */
	return 0;
}


static int _LivePackDequeue(t_dev_record_ctx *_ctx)
{
	t_dev_record_ctx *rc = (t_dev_record_ctx *)_ctx;
	ont_onvif_playctx *playctx;
	int ret = 0;
	if (!_ctx || !rc ) {
		return 0;
	}
	playctx = (ont_onvif_playctx *)rc->onvif_ctx;

	ont_list_t *plist;
	RTMPPackData *pkdata = NULL;


	plist = playctx ->packDataList;

	if (ont_list_size(plist) == 0) {
		return ret;
	}
	ont_list_pop_front(plist, (void **)&pkdata);
	if (!pkdata) {
		return ret;
	}
	if (playctx->last_sndts < pkdata->sndts) {
		return ret;
	}
	if (playctx->last_sndts - pkdata->sndts  > 5000) {
		RTMP_Log(RTMP_LOGINFO, "live packet dequeue, last sndts %lu, buffer header sndts %lu", playctx->last_sndts, pkdata->sndts);
		playctx->state = 1;
	}

	switch (pkdata->codecType) {
		case CODEC_H264:
			if (!rc->appendmeta[0]) {
				if (pkdata->mspd) {
					_ont_record_append_sps_pps(NULL, rc->channel, playctx->meta.frameRate,
					                           playctx->meta.width, playctx->meta.height,
					                           (const char *)pkdata->mspd->latestSps, pkdata->mspd->sps_len,
					                           (const char *)pkdata->mspd->latestPps, pkdata->mspd->pps_len);
					rc->appendmeta[0] = 1;
				}

			}

			if (rc->appendmeta[0] && ont_record_append_video(NULL, rc->channel, pkdata->deltaTs, (char *)pkdata->data, pkdata->size,  pkdata->ctl->isKeyFram) < 0) {
				rc->stop = 1;
				RTMP_Log(RTMP_LOGERROR, "append video error state is %d", playctx->state);
				goto _end;
			}
			ret = 1;
			break;
		case CODEC_MPEG4A:
			if (!rc->appendmeta[1]) {
				_ont_record_append_audio_sepciacfg(NULL, rc->channel, playctx->audio_seq, 2);
				rc->appendmeta[0] = 1;
			}

			if (ont_record_append_audio(NULL, rc->channel, pkdata->deltaTs, (const char *)pkdata->data, pkdata->size) < 0) {
				rc->stop = 1;
				RTMP_Log(RTMP_LOGINFO, "live packet dequeue, send audio data, state is %d", playctx->state);
				goto _end;
			}
			ret = 1;
			break;
		default:
			break;
	}

_end:
	if (pkdata->mspd) {
		delete (pkdata->mspd);
	}
	if (pkdata->ahd) {
		delete (pkdata->ahd);
	}
	if (pkdata->ctl) {
		delete (pkdata->ctl);
	}
	delete[](pkdata->data);
	delete (pkdata);

	return ret;
}


int  ont_record_singlestep(t_dev_record_ctx *rc)
{
	static int const maxNode = 50;
	int ret = 0;
	int i = 0;
	do {
		i++;
		ret = _LivePackDequeue(rc);
	} while (ret > 0 && i < maxNode);
	return 0;
}



int ont_record_stop(void *dev, int32_t channel)
{
	struct ont_timeval tv;
	ont_gettimeofday(&tv, NULL);

	t_dev_record_ctx *rc = _ont_record_get_channel(dev, channel);
	if (!rc) {
		return -1;
	}
	rc->state = 3;

	rc->recorded_endtime = tv.tv_sec;
	MP4Close(rc->hMP4File);

	/* add return statement later */
	return 0;
}



int ont_record_send_check(void *dev, int32_t channel)
{
	t_dev_record_ctx *rc = _ont_record_get_channel(dev, channel);
	if (!rc) {
		return -1;
	}
	if (rc->state == 3) {
		return 0;
	}
	return -1;
}


t_rtmp_vod_ctx *ont_record_send_start(void *dev, int32_t channel)
{

	char url[256];
	t_ont_rvod_callbacks vodcallback = {
		rtmp_rvod_mp4_pause_notify,
		rtmp_rvod_mp4_seek_notify,
		rtmp_rvod_mp4_stop_notify
	};

	t_rtmp_mp4_ctx *mp4ctx;
	mp4ctx = (t_rtmp_mp4_ctx *)ont_platform_malloc(sizeof(t_rtmp_mp4_ctx));
	memset(mp4ctx, 0x00, sizeof(t_rtmp_mp4_ctx));

	t_rtmp_vod_ctx *ctx = rtmp_rvod_createctx(dev, mp4ctx, &vodcallback);

	if (!ctx) {
		return NULL;
	}
	mp4ctx->rvodctx = ctx;

	t_dev_record_ctx *rc = _ont_record_get_channel(dev, channel);
	if (!rc) {
		return NULL;
	}

	if (rtmp_rvod_mp4_parse(mp4ctx, rc->path) < 0) {

		return NULL;
	}

	ont_platform_snprintf(url, sizeof(url), "%s?%lld&%lld", rc->url, rc->record_starttime, rc->record_endtime);
	if (rtmp_rvod_start(ctx, url, 10) < 0) {
		return NULL;
	}

	mp4ctx->packDataList = ont_list_create();

	rtmp_rvod_mp4_start(mp4ctx);

	rc->state = 4;

	return ctx;
}




#endif




