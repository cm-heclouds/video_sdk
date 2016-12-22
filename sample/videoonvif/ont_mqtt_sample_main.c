#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <ont/mqtt.h>
#include <ont/log.h>
#include <ont/video.h>
#include <ont/video_rvod.h>
#include "device_onvif.h"
#include "cJSON.h"
#include "sample_config.h"
#include "status_sync.h"

#ifdef ONT_OS_POSIX
#include <signal.h>
#endif

#ifdef WIN32
#include "windows.h"
#include "Dbghelp.h"
#include "tchar.h"
#include "assert.h"
#pragma comment(lib, "Dbghelp.lib")
#endif

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

int ont_add_onvif_one_device(const char*url, const char*usr, const char*passwd);


int ont_add_onvif_devices()
{
	int i;
	int j = cfg_get_channel_num();
	struct _onvif_channel * channels = cfg_get_channels();
	for (i = 0; i < j; i++)
	{
		if (ont_onvifdevice_adddevice(channels[i].url, channels[i].user, channels[i].pass) != 0)
		{
			ONT_LOG0(ONTLL_ERROR, "Failed to login the onvif server");
		}
	}
	return j;
}

void sync_chnanle_num()
{

}
void sync_channel_rvod(ont_device_t *dev)
{
	int i;
	int j = cfg_get_rvod_num();
	file_record record;
	struct _onvif_rvod * rvods = cfg_get_rvods();

	t_ont_video_file file;
	for (i = 0; i < j; i++)
	{
		if (rvods[i].isupload == 1)
		{
			continue;
		}
		record.chnnum = rvods[i].channelid;
		memcpy(&record.begintime, rvods[i].beginTime, sizeof(record.begintime));
		memcpy(&record.endtime, rvods[i].endTime, sizeof(record.endtime));
		if (!file_synced(&record))
		{
			ont_platform_snprintf(file.begin_time, sizeof(file.begin_time), rvods[i].beginTime);
			ont_platform_snprintf(file.end_time, sizeof(file.end_time), rvods[i].endTime);
			ont_platform_snprintf(file.descrtpion, sizeof(file.descrtpion), rvods[i].videoDesc);
			ont_video_dev_fileinfo_upload(dev, rvods[i].channelid, &file, 1);
		}
	}

}

void CreateDumpFile(LPCWSTR lpstrDumpFilePathName, EXCEPTION_POINTERS *pException)
{

	TCHAR szFull[260];
	TCHAR szDrive[3];
	TCHAR szDir[256];
	GetModuleFileName(NULL, szFull, sizeof(szFull) / sizeof(char));
	_tsplitpath(szFull, szDrive, szDir, NULL, NULL);
	strcpy(szFull, szDrive);
	strcat(szFull, szDir);
	strcat(szFull, "//");
	strcat(szFull, lpstrDumpFilePathName);

	// 创建Dump文件  
	//  
	HANDLE hDumpFile = CreateFile(szFull, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	// Dump信息  
	//  
	MINIDUMP_EXCEPTION_INFORMATION dumpInfo;
	dumpInfo.ExceptionPointers = pException;
	dumpInfo.ThreadId = GetCurrentThreadId();
	dumpInfo.ClientPointers = TRUE;

	// 写入Dump文件内容  
	//  
	MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, MiniDumpNormal, &dumpInfo, NULL, NULL);

	CloseHandle(hDumpFile);
}

LONG ApplicationCrashHandler(EXCEPTION_POINTERS *pException)
{
	//FatalAppExit(-1, "*** Unhandled Exception! ***");

	CreateDumpFile("video_sample.dmp", pException);

	return EXCEPTION_EXECUTE_HANDLER;
}

struct testDump
{
	int a;
	int b;
};

int main( int argc, char *argv[] )
{
	SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)ApplicationCrashHandler);

	//ont_onvif_device_discovery();
    ont_device_t *dev=NULL;
	int err;
	int delta = 0;

	ont_device_cmd_t *cmd = NULL;
	int chnnum = 0;
	cfg_initilize("config.json");
	int product_id = cfg_get_profile()->productid;

#ifdef ONT_OS_POSIX
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;//设定接受到指定信号后的动作为忽略
	sa.sa_flags = 0;
	if (sigemptyset(&sa.sa_mask) == -1 || //初始化信号集为空
	    sigaction(SIGPIPE, &sa, 0) == -1)
    { 
        //屏蔽SIGPIPE信号
	    exit(1);
	}
#endif
    ont_log_set_logger(NULL, sample_log);
	ont_log_set_log_level(ONTLL_DEBUG);
    err = ont_device_create(product_id, ONTDEV_MQTT, "MQTT-VIDEOTEST-DEV", &dev);
    if (ONT_ERR_OK != err) {
        ONT_LOG1(ONTLL_ERROR, "Failed to create device instance, error=%d", err);
        return -1;
    }
	dev->device_id = cfg_get_profile()->deviceid;
    err = ont_device_connect(dev,
							&cfg_get_profile()->svraddr[0],
							 cfg_get_profile()->svrport,
							 "qmkNvMyxo50lyLK9",
							 &cfg_get_profile()->key[0],
                             120);
    if (ONT_ERR_OK != err)
    {
        ont_device_destroy(dev);

        ONT_LOG1(ONTLL_ERROR, "Failed to connect to the server, error=%d", err);
        return -1;
    }

	chnnum = ont_add_onvif_devices();
	if ( !status_synced(chnnum))
	{
		ont_video_dev_set_channels(dev, chnnum);
	}

	sync_channel_rvod(dev);

   //ont_onvifdevice_ptz(0, 1, 1, 2);

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
