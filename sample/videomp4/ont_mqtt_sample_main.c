#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <ont/mqtt.h>
#include <ont/log.h>
#include <ont/video.h>
#include <ont/video_rvod.h>

#include "cJSON.h"

#define SAMPLE_VIDEOMQTT_ADDR "172.19.3.1"
#define SAMPLE_VIDEOMQTT_PORT 10019


static void sample_log(void *ctx, ont_log_level_t ll , const char *format, ...)
{
    static const char *level[] = {
        "DEBUG", "INFO", "WARNING", "ERROR", "FATAL"
    };
    va_list ap;
	time_t timer;
	struct tm *timeinfo;
    char buffer [64];
	timer = time(NULL);
	timeinfo = localtime(&timer);
    strftime (buffer,sizeof(buffer),"%Y/%m/%d %H:%M:%S",timeinfo);

	printf("[%s] [%s]", buffer, level[ll]);

    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
    printf("\n");
}


int ont_videocmd_parse(void *dev, const char*str);
int ont_video_playlist_singlestep(void *dev);

int main( int argc, char *argv[] )
{

    ont_device_t *dev;
    int err;
    int32_t next_dp_time;
	int delta = 0;
	
	int product_id = 1359;
	ont_device_cmd_t *cmd = NULL;
		
    ont_log_set_logger(NULL, sample_log);


    err = ont_device_create(product_id, ONTDEV_MQTT, "MQTT-VIDEOTEST-DEV", &dev);
    if (ONT_ERR_OK != err) {
        ONT_LOG1(ONTLL_ERROR, "Failed to create device instance, error=%d", err);
        return -1;
    }

	dev->device_id = 213346;
    err = ont_device_connect(dev,
		                    SAMPLE_VIDEOMQTT_ADDR,
							SAMPLE_VIDEOMQTT_PORT,
							"qmkNvMyxo50lyLK9",
							"123456654321",
                             120);
    if (ONT_ERR_OK != err)
    {
        ont_device_destroy(dev);

        ONT_LOG1(ONTLL_ERROR, "Failed to connect to the server, error=%d", err);
        return -1;
    }

	ont_video_dev_set_channels(dev, 1);

	//ont_video_dev_fileinfo_upload();

	ont_video_vod_stream_start(dev, 1, NULL, NULL, "rtmp://", 11);


    next_dp_time = (int32_t)ont_platform_time() + 15;
    while (1)
    {
		if (delta)
		{
			ont_platform_sleep(delta>100?100:delta);
		}
        err = ont_device_keepalive(dev);
        if (ONT_ERR_OK != err)
        {
            ONT_LOG1(ONTLL_ERROR, "device is not alive, error=%d", err);
            break;
        }

		/*
			GET PLATFORM CMD
		*/
		cmd = ont_device_get_cmd(dev);
		if(NULL != cmd)
		{
			ont_videocmd_parse(dev, cmd->req);
            ont_device_cmd_destroy(cmd);
		}
		delta = ont_video_playlist_singlestep(dev);
		
	}

    ont_device_destroy(dev);
    return 0;
}
