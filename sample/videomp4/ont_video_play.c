#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <ont/mqtt.h>
#include <ont/video.h>
#include <ont/video_rvod.h>
#include <ont/log.h>
#include "ont_list.h"

typedef  struct
{
	char playflag[32];
	char publishurl[255];
	char deviceid[16];
	char rvod;
	char closeflag; //0, nothing; 1 server closed, 2, restart
	void *playctx;
}t_playlist;

ont_list_t *g_list = NULL;

#define VARAIBLE_CHECK if(!g_list) { g_list = ont_list_create();}


void ont_video_live_stream_start(void *dev, int channel, const char *push_url, const char* deviceid)
{
	VARAIBLE_CHECK;

}

void ont_video_live_stream_close(void *dev, int channel)
{
	VARAIBLE_CHECK;
}


void ont_video_live_stream_ctrl(void *dev, int channel, int stream)
{
	VARAIBLE_CHECK;
}


int startfindFlag(t_playlist *d1, t_playlist *d2)
{
	if (strcmp(d1->playflag, d2->playflag) == 0)
	{
		return 0;
	}
	return -1;
}

void ont_video_vod_stream_start(void *dev, int channel, t_ont_video_file *fileinfo, const char *playflag, const char *push_url, const char *deviceid)
{
	VARAIBLE_CHECK;
	t_playlist data2;
	ont_list_node_t *node;
	ont_platform_snprintf(data2.playflag, sizeof(data2.playflag), "%s", playflag);
	node = ont_list_find(g_list, &data2, startfindFlag);
	t_playlist *data = NULL;
	if (node)
	{
		/*just hb*/
		return;
	}
	data = ont_platform_malloc(sizeof(t_playlist));
	data->closeflag = 0;
	data->rvod = 1;
    t_rtmp_vod_ctx *ctx = rtmp_rvod_createctx();
	do 
	{
		if (rtmp_rvod_parsefile(ctx, "e:\\test2.mp4") < 0)
		{
			break;
		}

		if (rtmp_rvod_start(ctx, push_url, deviceid) < 0)
		{
			break;
		}
		ont_platform_snprintf(data->playflag, sizeof data->playflag, "%s", playflag);
		data->playctx = ctx;
		ont_platform_snprintf(data->publishurl, sizeof(data->publishurl), "%s", push_url);
		ont_platform_snprintf(data->deviceid, sizeof(data->deviceid), "%s", deviceid);
		ont_list_insert(g_list, data);
		return;
	} while (0);
	ont_platform_free(data);
	rtmp_rvod_destoryctx(ctx);
	return ;

}

int compareFlag(t_playlist *d1, t_playlist *d2)
{
	if (strcmp(d1->playflag, d2->playflag) == 0)
	{
		return 0;
	}
	return -1;
}

void ont_video_vod_stream_close(void *dev, int channel, t_ont_video_file *fileinfo, const char *playflag)
{
    ont_list_node_t *node;
	t_playlist data, *finddata;
	VARAIBLE_CHECK;
	ont_platform_snprintf(data.playflag, sizeof(data.playflag), "%s", playflag);
	node = ont_list_find(g_list, &data, compareFlag);
	if (!node)
	{
		return;
	}
	finddata = ont_list_data(node);
	rtmp_rvod_stop(finddata->playctx);
	rtmp_rvod_destoryctx(finddata->playctx);
	ont_list_remove(g_list, node, NULL);
	ont_platform_free(finddata);
}

void ont_video_stream_make_keyframe(void *dev, int channel)
{

}


void ont_video_dev_ptz_ctrl(void *dev, int channel, int mode, t_ont_video_ptz_cmd cmd, int speed)
{

}

static int delta = 0;
void singlestep(void *data, void *context)
{
	t_playlist *play = data;
	int localDelta = 0;
	if (play->rvod == 1)
	{
		localDelta = rtmp_rvod_send_media_singlestep(play->playctx);
		if (localDelta < 0)
		{
			play->closeflag = 1;
		}
	}
	if (localDelta > delta)
	{
		delta = localDelta;
	}
}


int _checkcloseFlag(t_playlist *d1, t_playlist *d2)
{
	if (d1->closeflag == 1)
	{
		return 0;
	}
	return -1;
}

static int _checkneedclose(void *dev)
{
	t_playlist data, *finddata;
	ont_list_node_t *node;
	VARAIBLE_CHECK;
	do
	{
		node = ont_list_find(g_list, &data, _checkcloseFlag);
		if (!node)
		{
			return 0;
		}
		finddata = ont_list_data(node);

		ONT_LOG1(ONTLL_INFO, "vod end %s ",finddata->playflag);

		rtmp_rvod_stop(finddata->playctx);
		rtmp_rvod_destoryctx(finddata->playctx);
		finddata->playctx = NULL;
		finddata->closeflag = 2;
		ont_list_remove(g_list, node, NULL);
		ont_platform_free(finddata);
	} while (1); 
	return 0;
}


int _checkrestartFlag(t_playlist *d1, t_playlist *d2)
{
	if (d1->closeflag == 2)
	{
		return 0;
	}
	return -1;
}

static int _checkneedrestart(void *dev)
{
	t_playlist data, *finddata;
	ont_list_node_t *node;
	VARAIBLE_CHECK;
	do
	{
		node = ont_list_find(g_list, &data, _checkrestartFlag);
		if (!node)
		{
			return 0;
		}
		finddata = ont_list_data(node);
		t_rtmp_vod_ctx *ctx = rtmp_rvod_createctx();
		ONT_LOG1(ONTLL_INFO, "vod end %s ", finddata->playflag);

		finddata->playctx = ctx;
		rtmp_rvod_start(ctx, finddata->publishurl, finddata->deviceid);

	} while (1);
	return 0;
}

int ont_video_playlist_singlestep(void *dev)
{
	VARAIBLE_CHECK;
	delta = 0;
	ont_list_foreach(g_list, singlestep, dev);
	_checkneedclose(dev);
	//_checkneedrestart(dev);
	return delta;
}