#include "sample_config.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdarg.h>

struct sample_onvif_cfg
{
	struct _device_profile  profile;

	struct _onvif_channel*  channels;

	struct _onvif_rvod*  rovds;

	int channelnum;

	int rovdnum;
};

struct sample_onvif_cfg onvif_cfg;

int  cfg_initilize(const char * cfgpath)
{
	int filesize = 0;
#ifdef ONT_DEBUG
	FILE *hfile = fopen(cfgpath, "r");
#else
	FILE *hfile = fopen("config.json", "r");
#endif
	int i = 0;
	cJSON *json = NULL;
	char *url, *user, *pass;

	char *end = NULL;
	if (!hfile)
	{
		printf("not found the config file[config.json]\n");
		getchar();
		exit(0);
	}
	fseek(hfile, 0, SEEK_END);
	filesize = ftell(hfile);
	fseek(hfile, 0, SEEK_SET);
	char *dstaddr = ont_platform_malloc(filesize);
	while (!feof(hfile))
	{
		fread(dstaddr + i, 1, 1, hfile);
		i++;
	}
	dstaddr[i] = '\0';

	json = cJSON_ParseWithOpts(dstaddr, &end, 0);

	cJSON *item;
	/*
	* Get device profile
	*/
	item = cJSON_GetObjectItem(json, "profile");
	ont_platform_snprintf(onvif_cfg.profile.svraddr, sizeof(onvif_cfg.profile.svraddr), cJSON_GetObjectItem(item, "accsvr")->valuestring);
	ont_platform_snprintf(onvif_cfg.profile.key, sizeof(onvif_cfg.profile.key), cJSON_GetObjectItem(item, "pass")->valuestring);
	onvif_cfg.profile.svrport = cJSON_GetObjectItem(item, "accport")->valueint;
	onvif_cfg.profile.deviceid = cJSON_GetObjectItem(item, "deviceid")->valueint;
	onvif_cfg.profile.productid = cJSON_GetObjectItem(item, "productid")->valueint;


	/*
	 * Get onvif channels
	 */
	int j = cJSON_GetArraySize(cJSON_GetObjectItem(json, "onvif"));
	onvif_cfg.channelnum = j;
	onvif_cfg.channels = ont_platform_malloc(j * sizeof(struct _onvif_channel));

	
	for (i = 0; i < j; i++)
	{
		item = cJSON_GetArrayItem(cJSON_GetObjectItem(json, "onvif"), i);
		url = cJSON_GetObjectItem(item, "url")->valuestring;
		user = cJSON_GetObjectItem(item, "user")->valuestring;
		pass = cJSON_GetObjectItem(item, "passwd")->valuestring;

		onvif_cfg.channels[i].channelid = i+1;
		ont_platform_snprintf(onvif_cfg.channels[i].url, sizeof(onvif_cfg.channels[i].url), url);
		ont_platform_snprintf(onvif_cfg.channels[i].user, sizeof(onvif_cfg.channels[i].user), user);
		ont_platform_snprintf(onvif_cfg.channels[i].pass, sizeof(onvif_cfg.channels[i].pass), pass);
	}


	/*
	 * Get rvods 
	 */
	int k = cJSON_GetArraySize(cJSON_GetObjectItem(json, "rvod"));
	onvif_cfg.rovdnum = k;
	onvif_cfg.rovds = ont_platform_malloc(k * sizeof(struct _onvif_rvod));
		

	int  channel_id, isupload;
	char *location;
	char *dst, *beginTime, *endTime, *videodesc;

	for (i = 0; i < k; i++)
	{
		item = cJSON_GetArrayItem(cJSON_GetObjectItem(json, "rvod"), i);

		channel_id = cJSON_GetObjectItem(item, "channel_id")->valueint;
		location = cJSON_GetObjectItem(item, "location")->valuestring;
		isupload = cJSON_GetObjectItem(item, "isupload")->valueint;

		dst = cJSON_GetObjectItem(item, "dst")->valuestring;
		beginTime = cJSON_GetObjectItem(item, "beginTime")->valuestring;
		endTime = cJSON_GetObjectItem(item, "endTime")->valuestring;
		videodesc = cJSON_GetObjectItem(item, "videoDesc")->valuestring;


		onvif_cfg.rovds[i].channelid = channel_id;
		onvif_cfg.rovds[i].isupload = isupload;
		ont_platform_snprintf(onvif_cfg.rovds[i].location, sizeof(onvif_cfg.rovds[i].location), location);

		ont_platform_snprintf(onvif_cfg.rovds[i].dst, sizeof(onvif_cfg.rovds[i].dst), dst);
		ont_platform_snprintf(onvif_cfg.rovds[i].beginTime, sizeof(onvif_cfg.rovds[i].beginTime), beginTime);
		ont_platform_snprintf(onvif_cfg.rovds[i].endTime, sizeof(onvif_cfg.rovds[i].endTime), endTime);
		ont_platform_snprintf(onvif_cfg.rovds[i].videoDesc, sizeof(onvif_cfg.rovds[i].videoDesc), videodesc);
	}

	cJSON_Delete(json);
	fclose(hfile);
	ont_platform_free(dstaddr);
	return 0;
}

struct _device_profile * cfg_get_profile()
{
	return &onvif_cfg.profile;
}

int  cfg_get_channel_num()
{
	return onvif_cfg.channelnum;
}


struct _onvif_channel * cfg_get_channels()
{
	return onvif_cfg.channels;
}

struct _onvif_channel * cfg_get_channel(int channelid)
{
	for (int i = 0; i < onvif_cfg.channelnum;i++)
	{
		if (onvif_cfg.channels[i].channelid == channelid)
		{
			return &onvif_cfg.channels[i];
		}
	}
	return NULL;
}

int  cfg_get_rvod_num()
{
	return onvif_cfg.rovdnum;
}

struct _onvif_rvod * cfg_get_rvods()
{
	return onvif_cfg.rovds;
}

struct _onvif_rvod * cfg_get_rvod(int channelid, char * begin, char* end)
{
	for (int i = 0; i < onvif_cfg.rovdnum; i++)
	{
		if (onvif_cfg.rovds[i].channelid == channelid
			&& strcmp(begin, onvif_cfg.rovds[i].beginTime) == 0
			&& strcmp(end, onvif_cfg.rovds[i].endTime) == 0
			)
		{
			return &onvif_cfg.rovds[i];
		}
	}
	return NULL;
}
