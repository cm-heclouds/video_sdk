#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include <log.h>
#include <rtmp.h>
#include <device.h>

#include "rvod.h"
#include "platform_cmd.h"
#include "ont_packet_queue.h"
#include "mp4v2/mp4v2.h"
#include "video_record.h"
#include "sample_config.h"

/* 调用未声明函数，函数值默认返回int类型。
 * 解决x64 OS && sizeof(int) = 4，调用返回值为指针类型的函数的未声明问题。
 * */
#include "onvif/ont_env.h"

ont_list_t *g_list = NULL;

#define VARAIBLE_CHECK if(!g_list) { g_list = ont_list_create(); play_env = ont_onvifdevice_create_playenv();}

static void *play_env = NULL;

typedef  struct {
	char playflag[32];
	char publishurl[512];
	char deviceid[16];
	int channel;
	char type;//1 rtmp publish, 2 rvod ctx, 3 local record
	char closeflag; //0, nothing; 1 server closed,
	void *playctx;
} t_playlist;

int startfindFlag(t_playlist *d1, t_playlist *d2);

void ont_keepalive_resp_callback(ont_device_t *dev)
{
	RTMP_Log(RTMP_LOGINFO, "keepalive response");
}

int32_t user_defind_cmd(ont_device_t *dev, ont_device_cmd_t *cmd)
{
	const char *_resp = "nothing to response";
	/*response*/
	if (cmd->need_resp) {
		ont_device_reply_user_defined_cmd(dev, 0, cmd->id, _resp, strlen(_resp));
	} else {
		ont_device_reply_user_defined_cmd(dev, ONT_RET_NOT_SUPPORT, NULL, NULL, 0);
	}
	return ONT_ERR_OK;
}

int32_t api_defind_msg(ont_device_t *dev, char *msg, size_t msg_len)
{
	int ret = 0;

	ret = ont_device_deal_api_defined_msg(dev, msg, msg_len);

	return ret;
}

int32_t plat_resp_dev_push_stream_msg(ont_device_t *dev, ont_plat_resp_dev_ps_t *msg)
{
	int ret = 0;

	ret = ont_device_deal_plat_resp_push_stream_msg(dev, msg);

	return ret;
}

static int compareliveFlag(t_playlist *d1, t_playlist *d2)
{
	if (d1->channel == d2->channel) {
		return 0;
	}
	return -1;
}

int ont_video_live_stream_start_level(void *dev, int channel, const char *push_url, const char *deviceid, int level)
{
	ont_onvif_playctx *ctx = NULL;
	t_playlist *data = NULL;
	t_playlist data2 = { 0 };
	ont_list_node_t *node;
	struct ont_timeval tv;
	ont_gettimeofday(&tv, NULL);
#if 0 /* unused variable */
	unsigned long ts = (tv.tv_usec / 1000 + tv.tv_sec * 1000);
#endif
	VARAIBLE_CHECK;

	data2.channel = channel;
	node = ont_list_find(g_list, &data2, (compare_data)compareliveFlag);
	if (node) {
		RTMP_Log(RTMP_LOGINFO, "live already start %d", channel);
		return 1;
	}

	do {
		ctx = ont_onvifdevice_live_stream_start(play_env, cfg_get_cluster(), channel, push_url, deviceid, level);
		if (!ctx) {
			break;
		}
		data = ont_platform_malloc(sizeof(t_playlist));
		memset(data, 0x00, sizeof(t_playlist));
		data->closeflag = 0;
		data->type = 1;
		data->playctx = ctx;
		data->channel = channel;
		//ctx->last_sndts = ts;
		ont_platform_snprintf(data->publishurl, sizeof(data->publishurl), "%s", push_url);
		ont_platform_snprintf(data->deviceid, sizeof(data->deviceid), "%s", deviceid);
		ont_list_insert(g_list, data);
		return 0;
	} while (0);
	//error
	return -1;
}


int ont_video_live_stream_start(void *dev, int channel)
{
	/* try the 4 streams.*/
	int level = 4;
	do {
		if (ont_video_live_stream_start_level(dev, channel, NULL, NULL, level) >= 0) {
			break;
		} else {
			level--;
		}
	} while (level > 0);
	if (level <= 0) {
		RTMP_Log(RTMP_LOGERROR, "live channel start err, %d", channel);
	}
	return 0;
}

int ont_video_live_stream_rtmp_publish(ont_device_t *dev, int32_t channel, uint8_t protype, uint16_t ttl_min, const char *push_url)
{
	VARAIBLE_CHECK;
	ont_list_node_t *node;
	t_playlist data, *finddata;
	char       devid[64];
	VARAIBLE_CHECK;
	data.channel = channel;
	RTMP_Log(RTMP_LOGINFO, "channnel %d publish %s", channel, push_url);

	node = ont_list_find(g_list, &data, (compare_data)compareliveFlag);
	if (!node) {
		RTMP_Log(RTMP_LOGINFO, "channnel %d not found ", channel);
		return ONT_RET_CHANNEL_NOTEXIST;
	}
	finddata = ont_list_data(node);
	ont_onvif_playctx *r = (ont_onvif_playctx *)finddata->playctx;
	if (r->rtmp_client) {
		RTMP_Log(RTMP_LOGINFO, "channel %d already start ", channel);
		return ONT_RET_CHANNEL_ALREADY_START;
	}
	ont_platform_snprintf(devid, sizeof(devid), "%lld", dev->device_id);
	ont_onvifdevice_live_stream_play(r, push_url, devid);

	if ( r->rtmp_client ) {
		finddata->closeflag = 0;
	}
	return ONT_RET_SUCCESS;;
}


int startfindFlag(t_playlist *d1, t_playlist *d2)
{
	if (strcmp(d1->playflag, d2->playflag) == 0) {
		return 0;
	}
	return -1;
}

int32_t ont_vod_start_notify(ont_device_t *dev, int32_t channel, uint8_t protype, ont_video_file_t *fileinfo, const char *playflag, const char *pushurl, uint16_t ttl)
{
	VARAIBLE_CHECK;
	t_playlist data2 = { 0 };
	ont_list_node_t *node;
	t_rtmp_mp4_ctx  *mp4ctx;
	t_ont_rvod_callbacks vodcallback = {
		rtmp_rvod_mp4_pause_notify,
		rtmp_rvod_mp4_seek_notify,
		rtmp_rvod_mp4_stop_notify
	};
	RTMP_Log(RTMP_LOGINFO, "rovd channnel %d publish %s \n file %s-%s", channel, pushurl, fileinfo->begin_time, fileinfo->end_time);

	ont_platform_snprintf(data2.playflag, sizeof(data2.playflag), "%d_%s", fileinfo->channel, playflag);
	node = ont_list_find(g_list, &data2, (compare_data)startfindFlag);
	t_playlist *data = NULL;
	if (node) {
		RTMP_Log(RTMP_LOGINFO, "vod already start %s ", playflag);
		return 0;
	}
	data = ont_platform_malloc(sizeof(t_playlist));
	memset(data, 0x00, sizeof(t_playlist));
	data->closeflag = 0;
	data->type = 2;
	mp4ctx = ont_platform_malloc(sizeof(t_rtmp_mp4_ctx));
	memset(mp4ctx, 0x00, sizeof(t_rtmp_mp4_ctx));

	t_rtmp_vod_ctx *ctx = rtmp_rvod_createctx(dev, mp4ctx, &vodcallback);
	mp4ctx->rvodctx = ctx;

	do {
		struct _onvif_rvod *vod = cfg_get_rvod(fileinfo->channel, fileinfo->begin_time, fileinfo->end_time);
		if (!vod) {
			RTMP_Log(RTMP_LOGERROR, "vod not found channel %d, begin %s, end %s ", fileinfo->channel, fileinfo->begin_time, fileinfo->end_time);
			break;
		}

		if (rtmp_rvod_mp4_parse(mp4ctx, vod->location) < 0) {
			break;
		}

		if (rtmp_rvod_start(ctx, pushurl, 10) < 0) {
			break;
		}
		rtmp_rvod_mp4_start(mp4ctx);

		ont_platform_snprintf(data->playflag, sizeof data->playflag, "%d_%s", fileinfo->channel, playflag);
		data->playctx = ctx;
		ont_platform_snprintf(data->publishurl, sizeof(data->publishurl), "%s", pushurl);
		ont_platform_snprintf(data->deviceid, sizeof(data->deviceid), "%d", ((ont_device_t *)dev)->device_id);
		ont_list_insert(g_list, data);
		return 0;
	} while (0);

	if (mp4ctx) {
		rtmp_rvod_mp4_clear(mp4ctx);
		ont_platform_free(mp4ctx);
	}

	ont_platform_free(data);
	rtmp_rvod_destoryctx(ctx);

	return ONT_RET_SUCCESS;
}

int32_t _ont_cmd_live_stream_ctrl(void *dev, int32_t channel, int32_t level)
{
	VARAIBLE_CHECK;
	ont_list_node_t *node;
	t_playlist data, *finddata;
	VARAIBLE_CHECK;
	data.channel = channel;
	node = ont_list_find(g_list, &data, (compare_data)compareliveFlag);
	if (!node) {
		return 0;
	}
	finddata = ont_list_data(node);
	ont_onvifdevice_stream_ctrl(finddata->playctx, level);
	return 0;
}

int32_t _ont_cmd_dev_ptz_ctrl(void *dev, int32_t channel, int32_t mode, t_ont_video_ptz_cmd ptz, int32_t speed)
{
	ont_onvifdevice_ptz(cfg_get_cluster(), channel, ptz, speed, 2);
	return 0;
}

static int delta = 0;
void _listsinglestep(void *data, void *context)
{
	if (!data) {
		return;
	}
	t_playlist *play = data;
	int localDelta = 0;
	if (play->type == 2) {
		rtmp_rvod_select(play->playctx);
		localDelta = rtmp_rvod_mp4_send_media_singlestep(play->playctx, 1);
		if (localDelta < 0) {
			RTMP_Log(RTMP_LOGERROR, "localDelta less than 0, rvod need to close\n");
			play->closeflag = 1;
			localDelta = 0;
		}
	} else if (play->type == 3) {
		if (ont_record_singlestep(play->playctx) < 0) {
			play->closeflag = 1; //record error, need close
		}
		localDelta = 0;
	} else if (play->type == 1 ) {
		localDelta = 1;
	}


	if (localDelta < delta) {
		delta = localDelta;
	}
}


int _checkcloseFlag(t_playlist *d1, t_playlist *d2)
{
	struct ont_timeval tv;
	ont_onvif_playctx *playctx = d1->playctx;
	t_rtmp_mp4_ctx *mp4ctx = NULL;

	ont_gettimeofday(&tv, NULL);
	unsigned long ts = (tv.tv_usec / 1000 + tv.tv_sec * 1000);

	if (d1->closeflag == 1) {
		return 0;
	}

	if (d1->type == 2) {
		mp4ctx = (t_rtmp_mp4_ctx *)((t_rtmp_vod_ctx *)d1->playctx)->user_data;
		if (mp4ctx->state) {
			return 0;
		}
	}
	if (d1->type == 1 && playctx->state) {
		RTMP_Log(RTMP_LOGINFO, "live end, state is %d", playctx->state);
		return 0; /*need closed*/
	}

	if (d1->type == 1 && playctx->rtmp_client && playctx->last_sndts > 0 && ts > playctx->last_sndts && ts - playctx->last_sndts > 5000) {

		RTMP_Log(RTMP_LOGINFO, "live end, ts %lu, diff %lu", ts, ts - playctx->last_sndts);
		return 0;
	}

	if (d1->type == 1  && playctx->rtmp_client && RTMP_IsTimedout(playctx->rtmp_client)) {
		return 0;
	}

	if (d1->type == 3) {
		/*could stop now*/
		if (playctx->rtmp_client || ont_record_intime(NULL, d1->channel) != 0) {
			return 0;/*stop record*/
		}
	}
	return -1;
}

static int _checkneedclose(void *dev)
{
	t_playlist data, *finddata;
	ont_list_node_t *node = NULL;
	VARAIBLE_CHECK;
	do {
		node = ont_list_find(g_list, &data, (compare_data)_checkcloseFlag);
		if (!node) {
			return 0;
		}
		finddata = ont_list_data(node);
		if (finddata->type == 2) {
			RTMP_Log(RTMP_LOGINFO, "vod end %s ", finddata->playflag);

			rtmp_rvod_mp4_clear( ((t_rtmp_vod_ctx *)finddata->playctx)->user_data);
			ont_platform_free(((t_rtmp_vod_ctx *)finddata->playctx)->user_data);
			rtmp_rvod_stop(finddata->playctx);
			rtmp_rvod_destoryctx(finddata->playctx);
		} else if (finddata->type == 1) {
			RTMP_Log(RTMP_LOGINFO, "live end, channel %d", finddata->channel);
			ont_onvifdevice_rtmp_stop(finddata->playctx);
			finddata->closeflag = 0;
			continue;/* not remove ctx*/
		} else if (finddata->type == 3) {
			ont_record_stop(dev, finddata->channel);
		} else {
			abort();//should not hanppen
		}
		ont_list_remove(g_list, node, NULL);
		ont_platform_free(finddata);
	} while (1);
	return 0;
}


int _check_publish_status(t_playlist *d1, t_playlist *d2)
{
#if 0 /* unused variable */
	struct ont_timeval tv;
#endif
	ont_onvif_playctx *playctx = d1->playctx;
	if (d1->type != 1) {
		return -1;
	}
	if (playctx->rtmp_client) {
		return -1;
	}
	/*live and no rtmp publish */
	return 0;
}


int _check_need_record(t_playlist *d1, t_playlist *d2)
{
	if (_check_publish_status(d1, d2)) {
		return -1;
	}

	if (ont_record_check(NULL, d1->channel, d1->playctx)) {
		return -1;
	}
	return 0;
}

static int _checkneedrecord(void *dev)
{
	t_playlist data, *finddata;
	ont_list_node_t *node = NULL;
	VARAIBLE_CHECK;
	do {
		node = ont_list_find(g_list, &data, (compare_data)_check_need_record);
		if (!node) {
			return 0;
		}
		finddata = ont_list_data(node);

		/*start record*/;
		t_dev_record_ctx *rc = ont_record_start(dev, finddata->channel, finddata->playctx);
		t_playlist *rc_data = ont_platform_malloc(sizeof(t_playlist));
		memset(rc_data, 0x00, sizeof(t_playlist));
		rc_data->closeflag = 0;
		rc_data->type = 3;
		rc_data->playctx = rc;
		rc_data->channel = finddata->channel;
		ont_list_insert(g_list, rc_data);
	} while (1);
	return 0;
}


int _check_send_record(void *dev)
{
	int i = 0;
	struct _onvif_channel *chs = cfg_get_channels();
	for ( ; i < cfg_get_channel_num(); i++) {
		if (!ont_record_send_check(dev, chs[i].channelid)) {
			t_rtmp_vod_ctx *ctx = ont_record_send_start(dev, chs[i].channelid);
			if (ctx) {
				t_playlist *data = NULL;

				data = ont_platform_malloc(sizeof(t_playlist));
				memset(data, 0x00, sizeof(t_playlist));
				data->closeflag = 0;
				data->type = 2;
				data->playctx = ctx;
			}
		}

	}

	/* add return statement later */
	return 0;
}
int ont_video_playlist_singlestep(void *dev)
{
	VARAIBLE_CHECK;
	delta = 10;

	_checkneedclose(dev);

	/*check if record*/
	_checkneedrecord(dev);
	/*check if send record*/
	_check_send_record(dev);

	ont_list_foreach(g_list, _listsinglestep, dev);


	ont_onvifdevice_live_stream_singlestep(play_env, delta);


	return delta;
}

int32_t _ont_cmd_dev_query_files(void *dev, int32_t channel, int32_t startindex, int32_t max, const char *startTime, const char *stopTime, ont_video_file_t **files, int32_t *filenum, int32_t *totalnum)
{
	int num = 0;
	int totalcount = cfg_get_rvod_nums();
	int channel_index = 0;
	ont_video_file_t *record;
	struct _onvif_rvod *rvods = cfg_get_rvods();
	int i = 0;
	int localindex = 0;
	record = ont_platform_malloc(max * sizeof(ont_video_file_t));
	int year1, month1, day1, hour1, min1, sec1;
	int year2, month2, day2, hour2, min2, sec2;
	int year3, month3, day3, hour3, min3, sec3;
	int tm1, tm2, tm3, tm11, tm22, tm33;

	*totalnum = 0;
	year1 = month1 = day1 = hour1 = min1 = sec1 = 0;
	year2 = month2 = day2 = hour2 = min2 = sec2 = 0;
	year3 = month3 = day3 = hour3 = min3 = sec3 = 0;

	if (startTime) {
		sscanf(startTime, "%d-%d-%d %d:%d:%d", &year1, &month1, &day1, &hour1, &min1, &sec1);
	}
	if (stopTime) {
		sscanf(stopTime, "%d-%d-%d %d:%d:%d", &year2, &month2, &day2, &hour2, &min2, &sec2);
	}
	tm1 = year1 * 10000 + month1 * 100 + day1;
	tm11 = hour1 * 3600 + min1 * 60 + sec1;

	tm2 = year2 * 10000 + month2 * 100 + day2;
	tm22 = hour2 * 3600 + min2 * 60 + sec2;

	for (i = 0; i < totalcount; i++) {
		if (rvods[i].channelid != channel) {
			continue;
		}
		/*compare time */
		if (startTime || stopTime) {
			sscanf(rvods[i].beginTime, "%d-%d-%d %d:%d:%d", &year3, &month3, &day3, &hour3, &min3, &sec3);
			tm3 = year3 * 10000 + month3 * 100 + day3;
			tm33 = hour3 * 3600 + min3 * 60 + sec3;
			if (startTime && tm3 < tm1) {
				/*not used*/
				continue;
			} else if (startTime && tm3 == tm1) {
				if (tm33 < tm11) {
					continue;
				}
			}

			if (stopTime && tm3 > tm2) {
				continue;
			} else if (stopTime && tm3 == tm2) {
				if (tm33 > tm22) {
					continue;
				}
			}

		}
		num++;
		channel_index++;
		if (channel_index - 1 < startindex) {
			continue;
		}

		if (channel_index - startindex > max) {
			continue;
		}

		localindex = channel_index - startindex;
		record[localindex - 1].channel = channel;
		record[localindex - 1].size = rvods[i].size;
		ont_platform_snprintf(record[localindex - 1].descrtpion, sizeof(record[localindex - 1].descrtpion), "%s", rvods[i].videoDesc);
		ont_platform_snprintf(record[localindex - 1].begin_time, sizeof(record[localindex - 1].begin_time), "%s", rvods[i].beginTime);
		ont_platform_snprintf(record[localindex - 1].end_time, sizeof(record[localindex - 1].end_time), "%s", rvods[i].endTime);

	}
	if (localindex > 0) {
		*files = record;
		*filenum = localindex;
	} else {
		ont_platform_free(record);
		*files = NULL;
		*filenum = 0;
	}
	*totalnum = num;

	return 0;
}


