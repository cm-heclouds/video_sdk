#include <stdlib.h>
#include <string.h>
#include "ont/platform.h"
#include "ont/device.h"
#include "librtmp/rtmp.h"
#include "librtmp/rtmp_sys.h"
#include "librtmp/log.h"
#include "librtmp/amf.h"

#include "ont/video.h"
#include "ont/video_rvod.h"
#include "ont/video_cmd.h"
#include "ont/log.h"
#include "ont_channel.h"
#include "ont_tcp_channel.h"
#include "cJSON/cJSON.h"



static int _ont_videocmd_stream_ctrl(ont_device_t *dev, cJSON *cmd, t_ont_dev_video_callbacks *callback)
{
	ONT_LOG2(ONTLL_INFO, "channel %d, level %d ", cJSON_GetObjectItem(cmd, "channel_id")->valueint,
		cJSON_GetObjectItem(cmd, "level")->valueint);

	return callback->stream_ctrl(dev,
        cJSON_GetObjectItem(cmd, "channel_id")->valueint,
        cJSON_GetObjectItem(cmd, "level")->valueint);
}

static int _ont_videocmd_stream_live_play(ont_device_t *dev, cJSON *cmd, t_ont_dev_video_callbacks *callback)
{
	char devid[32];
	ont_platform_snprintf(devid, 32, "%d", dev->device_id);
	ONT_LOG3(ONTLL_INFO, "live channel %d ttl %d,url%s", cJSON_GetObjectItem(cmd, "channel_id")->valueint, cJSON_GetObjectItem(cmd, "ttl")->valueint,
		cJSON_GetObjectItem(cmd, "pushUrl")->valuestring);

	return callback->live_start(dev,
		cJSON_GetObjectItem(cmd, "channel_id")->valueint,
		cJSON_GetObjectItem(cmd, "pushUrl")->valuestring,
		devid);
}

static int _ont_videcmd_stream_live_start(ont_device_t *dev, cJSON *cmd, t_ont_dev_video_callbacks *callback)
{
	char devid[32];
	ont_platform_snprintf(devid, 32, "%d", dev->device_id);
	ONT_LOG3(ONTLL_INFO, "live channel %d ttl %d,url%s", cJSON_GetObjectItem(cmd, "channel_id")->valueint, cJSON_GetObjectItem(cmd, "ttl")->valueint,
		cJSON_GetObjectItem(cmd, "pushUrl")->valuestring);

	return callback->live_start(dev,
		cJSON_GetObjectItem(cmd, "channel_id")->valueint,
		cJSON_GetObjectItem(cmd, "pushUrl")->valuestring,
		devid);
}

static int _ont_videcmd_stream_make_keyframe(ont_device_t *dev, cJSON *cmd, t_ont_dev_video_callbacks *callback)
{
    return callback->make_keyframe(dev, cJSON_GetObjectItem(cmd, "channel_id")->valueint);
}

static int _ont_videcmd_stream_vodstart(ont_device_t *dev, cJSON *cmd, t_ont_dev_video_callbacks *callback)
{
	t_ont_video_file file;
	char devid[32];
	cJSON *item = NULL;

	ont_platform_snprintf(devid, 32, "%d", dev->device_id);
	ont_platform_snprintf(file.begin_time, sizeof(file.begin_time), "%s", cJSON_GetObjectItem(cJSON_GetObjectItem(cmd, "mediaIndex"), "beginTime")->valuestring);
	ont_platform_snprintf(file.end_time, sizeof(file.end_time), "%s", cJSON_GetObjectItem(cJSON_GetObjectItem(cmd, "mediaIndex"), "endTime")->valuestring);

    file.channel = cJSON_GetObjectItem(cmd, "channel_id")->valueint;
    
	item = cJSON_GetObjectItem(cmd, "playTag");
	item = cJSON_GetObjectItem(cmd, "pushUrl");
	item = cJSON_GetObjectItem(cmd, "ttl");

	ONT_LOG4(ONTLL_INFO, "channel %d vod start %s hb %d, url %s", cJSON_GetObjectItem(cmd, "channel_id")->valueint, cJSON_GetObjectItem(cmd, "playTag")->valuestring, cJSON_GetObjectItem(cmd, "ttl")->valueint,
		cJSON_GetObjectItem(cmd, "pushUrl")->valuestring);

    

	return callback->vod_start(dev,
                        	   &file,
                               cJSON_GetObjectItem(cmd, "playTag")->valuestring,
                        	   cJSON_GetObjectItem(cmd, "pushUrl")->valuestring);
}

static int _ont_videcmd_ptz_ctrl(ont_device_t *dev, cJSON *cmd, t_ont_dev_video_callbacks *callback)
{
	ONT_LOG4(ONTLL_INFO, "live channel %d, cmd %d, stop%d, speed %d",
		cJSON_GetObjectItem(cmd, "channel_id")->valueint,
		cJSON_GetObjectItem(cmd, "cmd")->valueint,
		cJSON_GetObjectItem(cmd, "stop")->valueint,
		cJSON_GetObjectItem(cmd, "speed")->valueint);

	return callback->ptz_ctrl(dev,
                            cJSON_GetObjectItem(cmd, "channel_id")->valueint,
                            cJSON_GetObjectItem(cmd, "stop")->valueint,
                            (t_ont_video_ptz_cmd)cJSON_GetObjectItem(cmd, "cmd")->valueint,
                            cJSON_GetObjectItem(cmd, "speed")->valueint);
}


static int _ont_videcmd_rvod_rsp(ont_device_t *dev, cJSON *rsp, const char *uuid)
{
	const char *resp = cJSON_PrintUnformatted(rsp);

	int ret = ont_device_reply_cmd(dev, uuid, resp, strlen(resp));
	/*response size is based on ONT_SOCK_SEND_BUF_SIZE and the protocol limited of the server*/
	if (ret < 0)
	{
		ONT_LOG2(ONTLL_ERROR, "reply error %d, response size is %d", ret, strlen(resp));
        return -1;
	}
	return 0;
}

static int _ont_videcmd_rvod_query(ont_device_t *dev, cJSON *cmd, const char *uuid, t_ont_dev_video_callbacks *callback)
{
	char err_resp[64];
	cJSON *json_rsp;
	cJSON  *rvods_rsp;
	cJSON  *item_tmp;
	int i;
    int num=0;
    int totalnum=0;
	int ret = 0;
	int channelid = cJSON_GetObjectItem(cmd, "channel_id")->valueint;
	int page = cJSON_GetObjectItem(cmd, "page")->valueint;
	int max = cJSON_GetObjectItem(cmd, "per_page")->valueint;
    t_ont_video_file *files=NULL;
    
	int start_index = (page - 1)*max;

	if (page < 1 || max < 1 || channelid <0 ||
		callback->query(dev, channelid, start_index, max, &files, &num, &totalnum) !=0 
		|| files==NULL)
	{
		ont_platform_snprintf(err_resp, sizeof(err_resp), "{\"all_count\":%d,\"cur_count\":0,\"page:\":%d,  \"rvods\":[]}", totalnum, page);
		ont_device_reply_cmd(dev, uuid, err_resp, strlen(err_resp));
		return -1;
	}

    
	json_rsp = cJSON_CreateObject();
	cJSON_AddItemToObject(json_rsp, "all_count", cJSON_CreateNumber(totalnum));
	cJSON_AddItemToObject(json_rsp, "cur_count", cJSON_CreateNumber(num));
	cJSON_AddItemToObject(json_rsp, "page", cJSON_CreateNumber(page));

	rvods_rsp = cJSON_CreateArray();
	for (i = 0; i < num; i++)
	{
		
		item_tmp = cJSON_CreateObject();
		cJSON_AddItemToObject(item_tmp, "channel_id", cJSON_CreateNumber(files[i].channel));
		cJSON_AddItemToObject(item_tmp, "video_title", cJSON_CreateString(files[i].descrtpion));
		cJSON_AddItemToObject(item_tmp, "beginTime",  cJSON_CreateString(files[i].begin_time));
		cJSON_AddItemToObject(item_tmp, "endTime", cJSON_CreateString(files[i].end_time));

		/*insert into response*/
		cJSON_AddItemToArray(rvods_rsp, item_tmp);
	}
    
	cJSON_AddItemToObject(json_rsp, "rvods", rvods_rsp);

	ret=_ont_videcmd_rvod_rsp(dev, json_rsp, uuid);
	cJSON_Delete(json_rsp);
	ont_platform_free(files);
	return 0;
}


int ont_videocmd_handle(void *_dev, void *_cmd, t_ont_dev_video_callbacks *cb)
{
	ont_device_t *dev = (ont_device_t*)_dev;
	ont_device_cmd_t *cmd = (ont_device_cmd_t*)_cmd;
	cJSON *json = NULL;
	cJSON *videocmd = NULL;
	int cmdid = 0;
    int ret =-1;
	ONT_LOG1(ONTLL_DEBUG, "cmd is :%s", cmd->req);
	json = cJSON_Parse(cmd->req);
	do
	{
		if (cJSON_HasObjectItem(json, "type"))
		{
			if (strcmp(cJSON_GetObjectItem(json, "type")->valuestring, "video"))
			{
				break;
			}
		}
		else
		{
			break;
		}
		if (!cJSON_HasObjectItem(json, "cmdId"))
		{
			break;
		}
		cmdid = cJSON_GetObjectItem(json, "cmdID")->valueint;
		if (!cJSON_HasObjectItem(json, "cmd"))
		{
			break;
		}
		videocmd = cJSON_GetObjectItem(json, "cmd");
		switch (cmdid)
		{
		case 1:
			ret=_ont_videocmd_stream_live_play(dev, videocmd, cb);
			break;
		case 2:
			ret=_ont_videcmd_stream_vodstart(dev, videocmd, cb);
			break;
		case 5:
			ret=_ont_videcmd_stream_make_keyframe(dev, videocmd, cb);
			break;
		case 6:
			ret=_ont_videocmd_stream_ctrl(dev, videocmd, cb);
			break;
		case 7:
			ret=_ont_videcmd_ptz_ctrl(dev, videocmd, cb);
			break;
		case 10:
			ret=_ont_videcmd_rvod_query(dev, videocmd, cmd->id, cb);
			break;
		default:
			break;
		}

	} while (0);
	cJSON_Delete(json);
	return ret;
}