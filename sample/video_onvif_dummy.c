#include <stdio.h>
#include "device_onvif.h"

#ifndef ONT_VIDEO_ONVIF_SUPPORTED

void *ont_onvifdevice_create_playenv()
{
	return NULL;
}

int ont_onvifdevice_live_stream_play(void *playctx, const char* push_url, const char* deviceid)
{
	return -1;
}

void* ont_onvifdevice_live_stream_start(void        *env,
	void        *cluster,
	int          channel,
	const  char *push_url,
	const char  *deviceid,
	int          level)
{
	return NULL;
}

int ont_onvifdevice_stream_ctrl(void *playctx, int level)
{
	return -1;
}

int ont_onvifdevice_live_stream_singlestep(void *env, int maxdelay)
{
	return -1;
}

void ont_onvifdevice_live_stream_stop(void *playctx)
{


}

int ont_onvifdevice_ptz(device_onvif_cluster_t *cluster,
	int                     channel,
	int                     cmd,
	int                     _speed, /* [1-7] */
	int                     status)
{
	return -1;
}

device_onvif_cluster_t* ont_onvif_device_cluster_create(void)
{
	return NULL;
}

int ont_onvifdevice_adddevice(device_onvif_cluster_t *cluster,
	int                     channel,
	const char             *url,
	const char             *user,
	const char             *passwd)
{
	return -1;
}

#endif