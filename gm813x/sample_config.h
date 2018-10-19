#ifndef _SAMPLE_CONFIG_H_
#define _SAMPLE_CONFIG_H_

#include "ont_list.h"

struct _device_profile {
	uint64_t productid;
	uint64_t deviceid;
	char id[512];
	char key[1024];
};

struct _onvif_channel {
	uint32_t channelid;
	char url[1024];
	char user[128];
	char pass[128];
	char title[255];
	char desc[255];
};

struct _onvif_rvod {
	int channelid;
	char location[1024];
	char beginTime[128];
	char endTime[128];
	char videoDesc[1024];
};

int  cfg_initilize(const char *cfgpath);

struct _device_profile *cfg_get_profile();

int  cfg_get_channel_num();
struct _onvif_channel *cfg_get_channels();
struct _onvif_channel *cfg_get_channel(int channelid);

int  cfg_get_rvod_num(int channel);
int cfg_get_rvod_nums();
struct _onvif_rvod *cfg_get_rvods();
struct _onvif_rvod *cfg_get_rvod(int channelid, char *begin, char *end);
int  GetLocalMac(char mac_addr[30]);





#endif