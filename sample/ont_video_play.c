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

#ifdef WIN32
#include <winsock.h>
#include <windows.h>

static int gettimeofday(struct timeval *tp, void *tzp)
{
    time_t clock;
    struct tm tm;
    SYSTEMTIME wtm;
    GetLocalTime(&wtm);
    tm.tm_year     = wtm.wYear - 1900;
    tm.tm_mon     = wtm.wMonth - 1;
    tm.tm_mday     = wtm.wDay;
    tm.tm_hour     = wtm.wHour;
    tm.tm_min     = wtm.wMinute;
    tm.tm_sec     = wtm.wSecond;
    tm.tm_isdst    = -1;
    clock = mktime(&tm);
    tp->tv_sec = (long)clock;
    tp->tv_usec = wtm.wMilliseconds * 1000;
    return (0);
}
#else
#include <sys/time.h>
#endif

ont_list_t *g_list = NULL;

#define VARAIBLE_CHECK if(!g_list) { g_list = ont_list_create(); play_env = ont_onvifdevice_create_playenv();}

static void *play_env = NULL;

typedef  struct
{
	char playflag[32];
	char publishurl[255];
	char deviceid[16];
	int channel;
	char rvod;
	char closeflag; //0, nothing; 1 server closed,
	void *playctx;
}t_playlist;

int startfindFlag(t_playlist *d1, t_playlist *d2);

void ont_keepalive_resp_callback(ont_device_t *dev)
{
	ONT_LOG0(ONTLL_INFO, "keepalive response");
}

int32_t user_defind_cmd(ont_device_t *dev, ont_device_cmd_t *cmd)
{
	const char *_resp = "nothing to response";
	/*response*/
	if (cmd->need_resp)
	{
		ont_device_reply_user_defined_cmd(dev, 0, cmd->id, _resp, strlen(_resp));
	}
	else
	{
		ont_device_reply_user_defined_cmd(dev, ONT_RET_NOT_SUPPORT, NULL, NULL, 0);
	}
	return ONT_ERR_OK;
}

static int compareliveFlag(t_playlist *d1, t_playlist *d2)
{
	if (d1->channel == d2->channel)
	{
		return 0;
	}
	return -1;
}

int ont_video_live_stream_start_level(void *dev, int channel, const char *push_url, const char* deviceid, int level)
{
	ont_onvif_playctx *ctx = NULL;
	t_playlist *data = NULL;
	t_playlist data2 = { 0 };
	ont_list_node_t *node;
	struct timeval tv;
	gettimeofday(&tv, NULL);
	unsigned long ts = (tv.tv_usec / 1000 + tv.tv_sec * 1000);
	VARAIBLE_CHECK;

	data2.channel = channel;
	node = ont_list_find(g_list, &data2, compareliveFlag);
	if (node)
	{
		ONT_LOG1(ONTLL_INFO, "live already start %d", channel);
		return 1;
	}

	do
	{
		ctx = ont_onvifdevice_live_stream_start(play_env, cfg_get_cluster(), channel, push_url, deviceid, level);
		if (!ctx)
		{
			break;
		}
		data = ont_platform_malloc(sizeof(t_playlist));
		memset(data, 0x00, sizeof(t_playlist));
		data->closeflag = 0;
		data->rvod = 0;
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
	do
	{
		if (ont_video_live_stream_start_level(dev, channel, NULL, NULL, level) >= 0)
		{
			break;
		}
		else
		{
			level--;
		}
	} while (level>0);
	if (level <= 0)
	{
		ONT_LOG1(ONTLL_ERROR, "live channel start err, %d", channel);
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
	ONT_LOG2(ONTLL_INFO, "channnel %d publish %s", channel, push_url);

	node = ont_list_find(g_list, &data, compareliveFlag);
	if (!node)
	{
		ONT_LOG1(ONTLL_INFO, "channnel %d not found ", channel);
		return ONT_RET_CHANNEL_NOTEXIST;
	}
	finddata = ont_list_data(node);
	ont_onvif_playctx *r = (ont_onvif_playctx*)finddata->playctx;
	if (r->rtmp_client)
	{
		ONT_LOG1(ONTLL_INFO, "channel already start ", channel);
		return ONT_RET_CHANNEL_ALREADY_START;
	}
	ont_platform_snprintf(devid, sizeof(devid), "%lld", dev->device_id);
	ont_onvifdevice_live_stream_play(finddata->playctx, push_url, devid);
	return ONT_RET_SUCCESS;;
}


int startfindFlag(t_playlist *d1, t_playlist *d2)
{
	if (strcmp(d1->playflag, d2->playflag) == 0)
	{
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
	ONT_LOG4(ONTLL_INFO, "rovd channnel %d publish %s \n file %s-%s", channel, pushurl, fileinfo->begin_time, fileinfo->end_time);

	ont_platform_snprintf(data2.playflag, sizeof(data2.playflag), "%d_%s", fileinfo->channel, playflag);
	node = ont_list_find(g_list, &data2, startfindFlag);
	t_playlist *data = NULL;
	if (node)
	{
		ONT_LOG1(ONTLL_INFO, "vod already start %s ", playflag);
		return 0;
	}
	data = ont_platform_malloc(sizeof(t_playlist));
	memset(data, 0x00, sizeof(t_playlist));
	data->closeflag = 0;
	data->rvod = 1;
	mp4ctx = ont_platform_malloc(sizeof(t_rtmp_mp4_ctx));
	memset(mp4ctx, 0x00, sizeof(t_rtmp_mp4_ctx));

	t_rtmp_vod_ctx *ctx = rtmp_rvod_createctx(dev, mp4ctx, &vodcallback);
	mp4ctx->rvodctx = ctx;

	do
	{
		struct _onvif_rvod *vod = cfg_get_rvod(fileinfo->channel, fileinfo->begin_time, fileinfo->end_time);
		if (!vod)
		{
			ONT_LOG3(ONTLL_ERROR, "vod not found channel %d, begin %s, end %s ", fileinfo->channel, fileinfo->begin_time, fileinfo->end_time);
			break;
		}

		if (rtmp_rvod_mp4_parse(mp4ctx, vod->location) < 0)
		{
			break;
		}

		if (rtmp_rvod_start(ctx, pushurl, 10)< 0)
		{
			break;
		}
		rtmp_rvod_mp4_start(mp4ctx);

		ont_platform_snprintf(data->playflag, sizeof data->playflag, "%d_%s", fileinfo->channel, playflag);
		data->playctx = ctx;
		ont_platform_snprintf(data->publishurl, sizeof(data->publishurl), "%s", pushurl);
		ont_platform_snprintf(data->deviceid, sizeof(data->deviceid), "%d", ((ont_device_t*)dev)->device_id);
		ont_list_insert(g_list, data);
		return 0;
	} while (0);

	if (mp4ctx)
	{
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
	node = ont_list_find(g_list, &data, compareliveFlag);
	if (!node)
	{
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
void vodsinglestep(void *data, void *context)
 {
	if (!data)
	{
		return;
	}
	t_playlist *play = data;
	int localDelta = 0;
	if (play->rvod == 1)
	{
        rtmp_rvod_select(play->playctx);
		localDelta = rtmp_rvod_mp4_send_media_singlestep(play->playctx);
		if (localDelta < 0)
		{
			ONT_LOG0(ONTLL_ERROR, "localDelta less than 0, rvod need to close\n");
			play->closeflag = 1;
		}
	}
	else
	{
		delta = 0;
	}

	if (localDelta>0 && localDelta < delta)
	{
		delta = localDelta;
	}
}


int _checkcloseFlag(t_playlist *d1, t_playlist *d2)
{
	struct timeval tv;
	ont_onvif_playctx *playctx = d1->playctx;
    t_rtmp_mp4_ctx *mp4ctx = NULL;

    gettimeofday(&tv, NULL);
    unsigned long ts = (tv.tv_usec / 1000 + tv.tv_sec * 1000);

	if (d1->rvod && d1->closeflag == 1)
	{
		return 0;
	}

    if (d1->rvod) {
		mp4ctx = (t_rtmp_mp4_ctx*)((t_rtmp_vod_ctx *)d1->playctx)->user_data;
		if (mp4ctx->state) {
            return 0;
        }
    }
	if (!d1->rvod && playctx->state)
	{
        ONT_LOG1(ONTLL_INFO, "live end, state is %d", playctx->state);
		return 0; /*need closed*/
	}

	if (!d1->rvod && playctx->last_sndts>0 && ts>playctx->last_sndts && ts - playctx->last_sndts >5000)
    {

		ONT_LOG2(ONTLL_INFO, "live end, ts %lu, last %lu", ts, playctx->last_sndts);
        return 0;
    }
    if (!d1->rvod && playctx->rtmp_client && RTMP_IsTimedout(playctx->rtmp_client))
    {
        return 0;
    }

	return -1;
}

static int _checkneedclose(void *dev)
{
	t_playlist data, *finddata;
	ont_list_node_t *node = NULL;
	VARAIBLE_CHECK;
	do
	{
		node = ont_list_find(g_list, &data, _checkcloseFlag);
		if (!node)
		{
			return 0;
		}
		finddata = ont_list_data(node);
		if (finddata->rvod)
		{
			ONT_LOG1(ONTLL_INFO, "vod end %s ", finddata->playflag);

            rtmp_rvod_mp4_clear( ((t_rtmp_vod_ctx*)finddata->playctx)->user_data);
			ont_platform_free(((t_rtmp_vod_ctx*)finddata->playctx)->user_data);
			rtmp_rvod_stop(finddata->playctx);
			rtmp_rvod_destoryctx(finddata->playctx);

            finddata->playctx = NULL;
            finddata->closeflag = 0;
            ont_list_remove(g_list, node, NULL);
            ont_platform_free(finddata);
		}
		else
		{
            ONT_LOG1(ONTLL_INFO, "live end, channel %d", finddata->channel);
			ont_onvifdevice_live_stream_stop(finddata->playctx);
            break;
		}
		//finddata->playctx = NULL;
		//finddata->closeflag = 0;
		//ont_list_remove(g_list, node, NULL);
		//ont_platform_free(finddata);
	} while (1);
	return 0;
}

int ont_video_playlist_singlestep(void *dev)
{
	VARAIBLE_CHECK;
	delta = 10;
	_checkneedclose(dev);
	ont_list_foreach(g_list, vodsinglestep, dev);
	ont_onvifdevice_live_stream_singlestep(play_env, 10);
	return delta;
}

int32_t _ont_cmd_dev_query_files(void *dev, int32_t channel, int32_t startindex, int32_t max, ont_video_file_t **files, int32_t *filenum, int32_t *totalnum)
{
	int num = cfg_get_rvod_num(channel);
	int totalcount = cfg_get_rvod_nums();
	int channel_index = 0;
	ont_video_file_t *record;
	struct _onvif_rvod * rvods = cfg_get_rvods();
	int i = 0;
	int localindex = 0;
	record = ont_platform_malloc(max * sizeof(ont_video_file_t));
	for (i = 0; i < totalcount; i++)
	{
		if (rvods[i].channelid != channel)
		{
			continue;
		}
		channel_index++;
		if (channel_index - 1 < startindex)
		{
			continue;
		}
		localindex = channel_index - startindex;
		record[localindex - 1].channel = channel;
		ont_platform_snprintf(record[localindex - 1].descrtpion, sizeof(record[localindex - 1].descrtpion), "%s", rvods[i].videoDesc);
		ont_platform_snprintf(record[localindex - 1].begin_time, sizeof(record[localindex - 1].begin_time), "%s", rvods[i].beginTime);

		ont_platform_snprintf(record[localindex - 1].end_time, sizeof(record[localindex - 1].end_time), "%s", rvods[i].endTime);

		if (localindex >= max)
		{
			break;
		}
	}
	if (localindex>0)
	{
		*files = record;
		*filenum = localindex;
	}
	else
	{
		ont_platform_free(record);
		*files = NULL;
		*filenum = 0;
	}
	*totalnum = num;

	return 0;
}