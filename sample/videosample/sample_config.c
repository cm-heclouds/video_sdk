#include "sample_config.h"
#include "cJSON.h"
#include <ont/log.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

struct sample_onvif_cfg
{
	struct _device_profile  profile;

	struct _onvif_channel*  channels;

	struct _onvif_rvod*  rovds;

	device_onvif_cluster_t *cluster;

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
	char *dstaddr = ont_platform_malloc(filesize+4);
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
	if (!item)
	{
	    printf("config file error\n");
		getchar();
		exit(0);
	}
	ont_platform_snprintf(onvif_cfg.profile.key, sizeof(onvif_cfg.profile.key), cJSON_GetObjectItem(item, "pass")->valuestring);
	onvif_cfg.profile.productid = cJSON_GetObjectItem(item, "productid")->valueint;
	onvif_cfg.cluster = ont_onvif_device_cluster_create();


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
		onvif_cfg.channels[i].channelid = cJSON_GetObjectItem(item, "channel_id")->valueint;
		ont_platform_snprintf(onvif_cfg.channels[i].url, sizeof(onvif_cfg.channels[i].url), url);
		ont_platform_snprintf(onvif_cfg.channels[i].user, sizeof(onvif_cfg.channels[i].user), user);
		ont_platform_snprintf(onvif_cfg.channels[i].pass, sizeof(onvif_cfg.channels[i].pass), pass);
	}


	/*
	 * Get rvods 
	 */
	int k = cJSON_GetArraySize(cJSON_GetObjectItem(json, "rvod"));
	onvif_cfg.rovds = ont_platform_malloc(k * sizeof(struct _onvif_rvod));
		

	int  channel_id;
	char *location;
	char *beginTime, *endTime, *videodesc;
	int year1, month1, day1, hour1, min1, sec1;
	int year2, month2, day2, hour2, min2, sec2;
	int t = 0;

	for (i = 0; i < k; i++)
	{
		item = cJSON_GetArrayItem(cJSON_GetObjectItem(json, "rvod"), i);

		channel_id = cJSON_GetObjectItem(item, "channel_id")->valueint;
		location = cJSON_GetObjectItem(item, "location")->valuestring;

		beginTime = cJSON_GetObjectItem(item, "beginTime")->valuestring;
		endTime = cJSON_GetObjectItem(item, "endTime")->valuestring;
		videodesc = cJSON_GetObjectItem(item, "videoTitle")->valuestring;
		sscanf(beginTime, "%d-%d-%d %d:%d:%d", &year1, &month1, &day1, &hour1, &min1, &sec1);
		sscanf(endTime, "%d-%d-%d %d:%d:%d", &year2, &month2, &day2, &hour2, &min2, &sec2);
		int tm1 = year1 * 10000 + month1 * 100 + day1;
		int tm2 = year2 * 10000 + month2 * 100 + day2;
		if (tm1 > tm2)
		{
			ONT_LOG1(ONTLL_ERROR, "invalid time parameter for %s", videodesc);
			continue;
		}
		if (tm1 == tm2)
		{
			tm1 = hour1 * 3600 + min1 * 60 + sec1;
			tm2 = hour2 * 3600 + min2 * 60 + sec2;
			if (tm1 >= tm2)
			{
				ONT_LOG1(ONTLL_ERROR, "invalid time parameter for %s", videodesc);
				continue;
			}
		}

        onvif_cfg.rovdnum = t;
        if (cfg_get_rvod(channel_id, beginTime, endTime))
        {
			ONT_LOG3(ONTLL_ERROR, "rvod config [channel_id : %d, beginTime : %s , endTime :%s ] already existing.", channel_id, beginTime, endTime);
			continue;
        }
        
		onvif_cfg.rovds[t].channelid = channel_id;
		ont_platform_snprintf(onvif_cfg.rovds[t].location, sizeof(onvif_cfg.rovds[t].location), location);
		ont_platform_snprintf(onvif_cfg.rovds[t].beginTime, sizeof(onvif_cfg.rovds[t].beginTime), beginTime);
		ont_platform_snprintf(onvif_cfg.rovds[t].endTime, sizeof(onvif_cfg.rovds[t].endTime), endTime);
		ont_platform_snprintf(onvif_cfg.rovds[t].videoDesc, sizeof(onvif_cfg.rovds[t].videoDesc), videodesc);
		t++;
	}
	onvif_cfg.rovdnum = t;
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
    int i =0;
	for (; i < onvif_cfg.channelnum;i++)
	{
		if (onvif_cfg.channels[i].channelid == channelid)
		{
			return &onvif_cfg.channels[i];
		}
	}
	return NULL;
}

int  cfg_get_rvod_num(int channelid)
{
	int i = 0;
	int ret=0;
	for (; i < onvif_cfg.rovdnum; i++)
	{
		if (onvif_cfg.rovds[i].channelid == channelid)		
		{
			ret++;
		}
	}
	return ret;
}

int cfg_get_rvod_nums()
{
	return onvif_cfg.rovdnum;
}

struct _onvif_rvod * cfg_get_rvods()
{
	return onvif_cfg.rovds;
}

struct _onvif_rvod * cfg_get_rvod(int channelid, char * begin, char* end)
{
    int i = 0;
	for (; i < onvif_cfg.rovdnum; i++)
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

device_onvif_cluster_t* cfg_get_cluster( void )
{
	return onvif_cfg.cluster;
}