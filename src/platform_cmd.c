#include <stdlib.h>
#include <string.h>
#include "platform.h"
#include "device.h"
#include "rvod.h"
#include "log.h"
#include "rtmp.h"
#include "rtmp_sys.h"
#include "rtmp_log.h"
#include "amf.h"
#include "rtmp_if.h"
#include "platform_cmd.h"
#include "cJSON/cJSON.h"


static int _ont_videocmd_stream_ctrl(ont_device_t *dev, cJSON *cmd, ont_cmd_callbacks_t *callback)
{
	ONT_LOG2(ONTLL_INFO, "channel %d, level %d ", cJSON_GetObjectItem(cmd, "channel_id")->valueint,
		cJSON_GetObjectItem(cmd, "level")->valueint);

	return callback->stream_ctrl(dev,
		cJSON_GetObjectItem(cmd, "channel_id")->valueint,
		cJSON_GetObjectItem(cmd, "level")->valueint);
}


static int _ont_videcmd_ptz_ctrl(ont_device_t *dev, cJSON *cmd, ont_cmd_callbacks_t *callback)
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

	int ret=ont_device_reply_ont_cmd(dev, 0, uuid, resp, strlen(resp));
	if (ret < 0)
	{
		ONT_LOG2(ONTLL_ERROR, "reply error %d, response size is %d", ret, strlen(resp));
		return -1;
	}
	return 0;
}

static int _ont_videcmd_rvod_query(ont_device_t *dev, cJSON *cmd, const char *uuid, ont_cmd_callbacks_t *callback)
{
	char err_resp[64];
	cJSON *json_rsp;
	cJSON  *rvods_rsp;
	cJSON  *item_tmp;
	int i;
	int num = 0;
	int totalnum = 0;
	int ret = 0;
	int channelid = cJSON_GetObjectItem(cmd, "channel_id")->valueint;
	int page = cJSON_GetObjectItem(cmd, "page")->valueint;
	int max = cJSON_GetObjectItem(cmd, "per_page")->valueint;
	ont_video_file_t *files = NULL;

	int start_index = (page - 1)*max;
	ret = callback->query(dev, channelid, start_index, max, &files, &num, &totalnum);
	if (ret != 0)
	{
		return ret;
	}

	if (page < 1 || max < 1 || channelid <0 || files == NULL)
	{
		ont_platform_snprintf(err_resp, sizeof(err_resp), "{\"all_count\":%d,\"cur_count\":0,\"page:\":%d,  \"rvods\":[]}", totalnum, page);
		ont_device_reply_ont_cmd(dev, 0, uuid, err_resp, strlen(err_resp));
		return 0;
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
		cJSON_AddItemToObject(item_tmp, "beginTime", cJSON_CreateString(files[i].begin_time));
		cJSON_AddItemToObject(item_tmp, "endTime", cJSON_CreateString(files[i].end_time));

		/*insert into response*/
		cJSON_AddItemToArray(rvods_rsp, item_tmp);
	}

	cJSON_AddItemToObject(json_rsp, "rvods", rvods_rsp);

	ret = _ont_videcmd_rvod_rsp(dev, json_rsp, uuid);
	cJSON_Delete(json_rsp);
	ont_platform_free(files);
	return 0;
}


int32_t ont_videocmd_handle(void *_dev, ont_device_cmd_t *_cmd)
{
	ont_device_t *dev = (ont_device_t*)_dev;
	ont_device_cmd_t *cmd = (ont_device_cmd_t*)_cmd;
	cJSON *json = NULL;
	cJSON *videocmd = NULL;
	int cmdid = 0;
	int ret = ONT_RET_CMD_ERROR;
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
		case 6:
			ret = _ont_videocmd_stream_ctrl(dev, videocmd, dev->ont_cmd_cbs);
			break;
		case 7:
			ret = _ont_videcmd_ptz_ctrl(dev, videocmd, dev->ont_cmd_cbs);
			break;
		case 10:
			ret = _ont_videcmd_rvod_query(dev, videocmd, cmd->id, dev->ont_cmd_cbs);
			break;
		default:
			break;
		}

	} while (0);
	
	/*not handle success, return fail code*/
	if (ret != 0)
	{
		if (cmd->need_resp)
		{
			ont_device_reply_ont_cmd(dev, ret, cmd->id, NULL, 0);
		}
		else
		{
			ont_device_reply_ont_cmd(dev, ret, NULL, NULL, 0);
		}
	}
	cJSON_Delete(json);
	return ret;
}