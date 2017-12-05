#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <ont/mqtt.h>
#include <ont/video.h>
#include <ont/video_rvod.h>
#include <ont/video_cmd.h>
#include <ont/log.h>
#include "cJSON.h"
#include "sample_config.h"

extern int ont_video_dev_query_files(void *dev, int channel, int startindex, int max, t_ont_video_file **files, int *filenum, int *totalcount);


int ont_video_dev_query_files(void *dev, int channel, int startindex, int max, t_ont_video_file **files, int *filenum, int *totalcount)
{
	int num = cfg_get_rvod_num(channel);
	int totalnum = cfg_get_rvod_nums();
    int channel_index=0;
    t_ont_video_file *record;
	struct _onvif_rvod * rvods = cfg_get_rvods();
    int i =0;
	int localindex=0;
    record  = ont_platform_malloc(max*sizeof(t_ont_video_file));
	for (i = 0; i < totalnum; i++)
	{
		if (rvods[i].channelid != channel)
		{
			continue;
		}
		channel_index++;
		if (channel_index-1 < startindex)
		{
			continue;
		}
		localindex = channel_index - startindex;
		record[localindex - 1].channel = channel;
		ont_platform_snprintf(record[localindex - 1].descrtpion, sizeof(record[localindex - 1].descrtpion), "%s", rvods[i].videoDesc);
		ont_platform_snprintf(record[localindex - 1].begin_time, sizeof(record[localindex - 1].begin_time), "%s", rvods[i].beginTime);

		ont_platform_snprintf(record[localindex - 1].end_time, sizeof(record[localindex - 1].end_time), "%s", rvods[i].endTime);
        
		if (localindex >= max)
		{
			break;
		}
	}
	if (localindex>0)
    {
        *files=record;
		*filenum = localindex;
    }
    else
    {
		ont_platform_free(record);
        *files=NULL;
        *filenum=0;      
    }
	*totalcount = num;
    
	return 0;    
}


int ont_video_dev_channel_sync(void *dev)
{
	int i = 0;
	int num = cfg_get_channel_num();
	struct _onvif_channel *_channels=cfg_get_channels();
	for (; i < num; i++)
	{
		ont_video_dev_add_channel(dev, _channels[i].channelid);
	}
	return 0;
}

