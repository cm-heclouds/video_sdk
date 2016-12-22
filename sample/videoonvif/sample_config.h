#ifndef _SAMPLE_CONFIG_H_
#define _SAMPLE_CONFIG_H_

#include "ont_list.h"

struct _device_profile
{
	char  svraddr[1024];
	unsigned short  svrport;

	unsigned int  deviceid;
	unsigned int  productid;
	char  key[1024];
};

struct _onvif_channel
{
	int   channelid;
	char  url[1024];
	char  user[128];
	char  pass[128];
};

struct _onvif_rvod
{
	int   channelid;
	int   isupload;
	char  location[1024];

	char  dst[64];
	char  beginTime[128];
	char  endTime[128];
	char  videoDesc[1024];
};

int  cfg_initilize(const char * cfgpath);

struct _device_profile * cfg_get_profile();

int  cfg_get_channel_num();
struct _onvif_channel * cfg_get_channels();
struct _onvif_channel * cfg_get_channel(int channelid);

int  cfg_get_rvod_num();
struct _onvif_rvod * cfg_get_rvods();
struct _onvif_rvod * cfg_get_rvod(int channelid, char * begin, char* end);


#endif