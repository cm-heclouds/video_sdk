#include <stdlib.h>
#include <string.h>
#include "ont/platform.h"
#include "ont/device.h"
#include "ont/video.h"
#include "ont/video_rvod.h"
#include "ont/video_cmd.h"
#include "ont/log.h"
#include "ont_channel.h"
#include "ont_tcp_channel.h"
#include "cJSON/cJSON.h"


int ont_video_dev_add_channel(void *_dev, int channel)
{
	char dsname[32];
	ont_device_t *dev = (ont_device_t*)_dev;
	ont_platform_snprintf(dsname, sizeof(dsname), "ont_video_%d", channel);
	ont_device_add_dp_string(dev, dsname, "");
	ont_device_send_dp(dev);
	ont_device_clear_dp(dev);
	return 0;
}
