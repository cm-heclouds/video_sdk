#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <ont/mqtt.h>
#include <ont/video.h>
#include <ont/video_rvod.h>
#include <ont/log.h>
#include "cJSON.h"



void ont_video_live_stream_start(void *dev, int channel, const char *push_url, const char* deviceid);
void ont_video_live_stream_ctrl(void *dev, int channel, int stream);
void ont_video_vod_stream_start(void *dev, int channel, t_ont_video_file *fileinfo, const char *playflag, const char *push_url, const char *deviceid);
void ont_video_stream_make_keyframe(void *dev, int channel);
void ont_video_dev_ptz_ctrl(void *dev, int channel, int mode, t_ont_video_ptz_cmd cmd, int speed);

t_ont_video_dev_ctrl_callbacks _gdevCtrlCallback = {
	ont_video_live_stream_ctrl,
	ont_video_live_stream_start,
	ont_video_stream_make_keyframe,
	ont_video_vod_stream_start,
	ont_video_dev_ptz_ctrl
};



int ont_video_dev_set_channels(void *_dev, int channels)
{
	ont_device_t *dev = _dev;
	char dsname[32];
	int i = 0;
	cJSON *json = cJSON_CreateNull();

	cJSON_AddItemToObject(json, "null", cJSON_CreateString("null"));
    char *jsonValue = cJSON_PrintUnformatted(json);
	for (i = 0; i < channels; i++)
	{
		ont_platform_snprintf(dsname, sizeof(dsname), "ont_video_%d_mqtttestvideo", i + 1);
		ont_device_add_dp_object(dev, dsname, jsonValue);
	}
	ont_device_send_dp(dev);
	ont_platform_free(jsonValue);
	cJSON_Delete(json);

	return 0;
}


int ont_video_dev_fileinfo_upload(void *_dev, int channel, t_ont_video_file *list, int n)
{
	ont_device_t *dev = _dev;
	char dsname[16];
	int i = 0;
	
	ont_platform_snprintf(dsname, sizeof(dsname), "ont_video_%d_mqtttestvideo", channel);

	cJSON *json = NULL;
	char *jsonValue = NULL;
	//example
	//"beginTime" : "2016-10-19 16:30:30",
	//"endTime" : "2016-10-20 16:30:30",  
	//"vedioDesc" : "video2"
	for (i = 0; i < n; i++)
	{
		json = cJSON_CreateNull();
		cJSON_AddItemToObject(json, "dst", cJSON_CreateString("video"));
		cJSON_AddItemToObject(json, "beginTime", cJSON_CreateString(list[i].begin_time));
		cJSON_AddItemToObject(json, "endTime", cJSON_CreateString(list[i].end_time));
		cJSON_AddItemToObject(json, "vedioDesc", cJSON_CreateString(list[i].descrtpion));
		jsonValue = cJSON_PrintUnformatted(json);
		ont_device_add_dp_object(dev, dsname, jsonValue);
		ont_platform_free(jsonValue);
		cJSON_Delete(json);
	}
	ont_device_send_dp(dev);
	return 0;
}



static int _ont_videocmd_stream_ctrl(ont_device_t *dev, cJSON *cmd)
{
	_gdevCtrlCallback.make_keyframe(dev,
		cJSON_GetObjectItem(cmd, "channel_id")->valueint);
	return 0;
}

static int _ont_videcmd_stream_live_start(ont_device_t *dev, cJSON *cmd)
{
	//   "channel_id": 1,            //数字
	//   "pushUrl":   "rtmp://xxx"  //字符串 
	uint8_t devid[32];
	ont_platform_snprintf(devid, 32, "%d", dev->device_id);
	_gdevCtrlCallback.live_start(dev,
		cJSON_GetObjectItem(cmd, "channel_id")->valueint,
		cJSON_GetObjectItem(cmd, "pushUrl")->valuestring,
		devid);
	return 0;
}

static int _ont_videcmd_stream_make_keyframe(ont_device_t *dev, cJSON *cmd)
{
	_gdevCtrlCallback.make_keyframe(dev,
		cJSON_GetObjectItem(cmd, "channel_id")->valueint);
	return 0;
}

static int _ont_videcmd_stream_vodstart(ont_device_t *dev, cJSON *cmd)
{
	t_ont_video_file file;
	uint8_t devid[32];
	cJSON *item = NULL;

	ont_platform_snprintf(devid, 32, "%d", dev->device_id);

	ont_platform_snprintf(file.begin_time, sizeof(file.begin_time), "%s", cJSON_GetObjectItem(cJSON_GetObjectItem(cmd, "mediaIndex"), "beginTime")->valuestring);
	ont_platform_snprintf(file.end_time, sizeof(file.end_time), "%s", cJSON_GetObjectItem(cJSON_GetObjectItem(cmd, "mediaIndex"), "endTime")->valuestring);

	item = cJSON_GetObjectItem(cmd, "playTag");
	item = cJSON_GetObjectItem(cmd, "pushUrl");
	item = cJSON_GetObjectItem(cmd, "ttl");

	ONT_LOG2(ONTLL_INFO, "vod start %s hb %d", cJSON_GetObjectItem(cmd, "playTag")->valuestring, cJSON_GetObjectItem(cmd, "ttl")->valueint);

	_gdevCtrlCallback.vod_start(dev,
		cJSON_GetObjectItem(cmd, "channel_id")->valueint,
		&file,
		cJSON_GetObjectItem(cmd, "playTag")->valuestring,
		cJSON_GetObjectItem(cmd, "pushUrl")->valuestring,
		devid);
	return 0;
}


static int _ont_videcmd_ptz_ctrl(ont_device_t *dev, cJSON *cmd)
{
	_gdevCtrlCallback.ptz_ctrl(dev,
		cJSON_GetObjectItem(cmd, "channel_id")->valueint,
		cJSON_GetObjectItem(cmd, "stop")->valueint,
		cJSON_GetObjectItem(cmd, "cmd")->valueint,
		cJSON_GetObjectItem(cmd, "speed")->valueint);
	return 0;
}

int ont_videocmd_parse(void *dev, const char*str)
{
	cJSON *json = NULL;
	cJSON *cmd = NULL;
	int cmdid = 0;
	json = cJSON_Parse(str);
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
		cmd = cJSON_GetObjectItem(json, "cmd");
		switch (cmdid)
		{
		    case 1:
				/*--实时流推送指令--*/
				_ont_videcmd_stream_live_start(dev, cmd);
			    break;
			case 2:
				/*--历史流推送指令--*/
				_ont_videcmd_stream_vodstart(dev, cmd);
				break;
			case 5:
				/*--通知指定的通道生成关键帧指令--*/
				_ont_videcmd_stream_make_keyframe(dev, cmd);
				break;
			case 6:
				/*--码流设置指令--*/
				_ont_videocmd_stream_ctrl(dev, cmd);
				break;
			case 7:
				/*--云台控制指令--*/
				_ont_videcmd_ptz_ctrl(dev, cmd);
				break;
			default:
				break;
		}
		
	} while (0);
	cJSON_Delete(json);
	return 0;
}