#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "log.h"
#include "rvod.h"
#include "device.h"
#include "config.h"
#include "rtmp_if.h"
#include "security.h"
#include "ont_list.h"
#include "ont_bytes.h"
#include "status_sync.h"
#include "platform_cmd.h"
#include "sample_config.h"
#include "video_rvod_mp4.h"
#include "ont_packet_queue.h"

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



#define ONT_VIDEO_PROTOCOL_ENCRYPT 1

static void sample_log(void *ctx, ont_log_level_t ll, const char *format, ...)
{
	static const char *level[] = {
		"DEBUG", "INFO", "WARNING", "ERROR", "FATAL"
	};
	va_list ap;
	time_t timer;
	struct tm *timeinfo;
	char buffer[64];
	timer = time(NULL);
	timeinfo = localtime(&timer);
	strftime(buffer, sizeof(buffer), "%Y/%m/%d %H:%M:%S", timeinfo);

	printf("[%s] [%s]", buffer, level[ll]);

	//fprintf(log_fd, "%s [%s]", buffer,level[ll]);

	va_start(ap, format);
	vprintf(format, ap);
	//(log_fd, format, ap);
	va_end(ap);
	printf("\n");
	//fprintf(log_fd, "%s", "\n");
}



#ifdef WIN32
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

#endif

void GetWorkDir(char *pBuf)
{
	//char pBuf[MAX_PATH];   
#ifdef WIN32
	GetCurrentDirectory(MAX_PATH, pBuf);
	strcat(pBuf, "\\");
#endif
}

int32_t _ont_cmd_live_stream_ctrl(void *dev, int32_t channel, int32_t level);
int32_t _ont_cmd_dev_ptz_ctrl(void *dev, int32_t channel, int32_t mode, t_ont_video_ptz_cmd ptz, int32_t speed);
int32_t _ont_cmd_dev_query_files(void *dev, int32_t channel, int32_t startindex, int32_t max, ont_video_file_t **files, int32_t *filenum, int32_t *totalnum);



void ont_keepalive_resp_callback(ont_device_t *dev);
int ont_video_live_stream_start(void *dev, int channel);
int ont_video_live_stream_rtmp_publish(ont_device_t *dev, int32_t channel, uint8_t protype, uint16_t ttl_min, const char *push_url);

int32_t ont_vod_start_notify(ont_device_t *dev, int32_t channel, uint8_t protype, ont_video_file_t *fileinfo, const char *playflag, const char*pushurl, uint16_t ttl);
int32_t user_defind_cmd(ont_device_t *dev, ont_device_cmd_t *cmd);

ont_device_callback_t _g_device_cbs =
{
	ont_keepalive_resp_callback,
	ont_video_live_stream_rtmp_publish,
	ont_vod_start_notify,
	user_defind_cmd
};

ont_cmd_callbacks_t _g_ont_cmd =
{
	_ont_cmd_live_stream_ctrl,
	_ont_cmd_dev_ptz_ctrl,
	_ont_cmd_dev_query_files
};


int ont_add_onvif_devices(ont_device_t *dev)
{
	int i;
	int j = cfg_get_channel_num();
	struct _onvif_channel * channels = cfg_get_channels();
	for (i = 0; i < j; i++)
	{
		if (ont_onvifdevice_adddevice(cfg_get_cluster(),
			channels[i].channelid, channels[i].url, channels[i].user, channels[i].pass) != 0)
		{
			ONT_LOG0(ONTLL_ERROR, "Failed to login the onvif server");
		}
		else {
			//uint8_t devid[32];
			//ont_platform_snprintf((char *)devid, 32, "%d", dev->device_id);
			ont_video_live_stream_start(dev, channels[i].channelid);
		}
	}
	return j;
}

#define IPSIZE 40
#define TOKENBUFSIZE 32

extern int initialize_environment(void);

int main(int argc, char *argv[])
{
	//log_redirect();
	initialize_environment();
	ont_security_init();

#ifdef WIN32
	SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)ApplicationCrashHandler);
#endif

	//ont_onvif_device_discovery();
	ont_device_t *dev=NULL;
	int err;
	int delta = 0;

	ont_device_cmd_t *cmd = NULL;

	ont_log_set_logger(NULL, sample_log);
	ont_log_set_log_level(ONTLL_DEBUG);

	char cfg_path[256] = {0};
	GetWorkDir(cfg_path);
	strcat(cfg_path, "config.json");

	ONT_LOG1(ONTLL_INFO, "cfg_path:%s\n", cfg_path);
#ifdef WIN32
    //cfg_initilize("E:\\share\\video_proj\\video_sdk\\bin\\Debug\\config.json");
	
	cfg_initilize(cfg_path);
#else
	cfg_initilize(cfg_path);
#endif
	//cfg_initilize("config.json");
	uint64_t product_id = cfg_get_profile()->productid;
	uint64_t device_id = cfg_get_profile()->deviceid;

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


	char token[TOKENBUFSIZE];

	err = ont_device_create(&dev, &_g_device_cbs);
	if (ONT_ERR_OK != err) {
		ONT_LOG1(ONTLL_ERROR, "Failed to create device instance, error=%d\n", err);
        getchar();
		return -1;
	}

	ont_add_onvif_devices(dev);

	err = ont_device_set_platformcmd_handle(dev, &_g_ont_cmd);
	if (ONT_ERR_OK != err) {
		ONT_LOG1(ONTLL_ERROR, "Failed to set platformcmd handle, error=%d\n", err);
        getchar();
		return -1;
	}
	dev->product_id = product_id;
	dev->device_id = device_id;

	err = ont_device_get_acc(dev, token);
	if (ONT_ERR_OK != err) {
		ONT_LOG1(ONTLL_ERROR, "Failed to get acc service, error=%d\n", err);
        getchar();
		return -1;
	}

	err = ont_device_connect(dev);
	if (ONT_ERR_OK != err)
	{
		ont_device_destroy(dev);
		ONT_LOG1(ONTLL_ERROR, "Failed to connect to the server, error=%d\n", err);
        getchar();
		return -1;
	}
	err = ont_device_verify(dev, token);
	if (ONT_ERR_OK != err) {
		ONT_LOG1(ONTLL_ERROR, "Failed to verify device, error=%d\n", err);
        getchar();
		return -1;
	}
	int pf_request_timeout = 1000;
#if ONT_VIDEO_PROTOCOL_ENCRYPT
	err = ont_device_request_rsa_publickey(dev);
	if (ONT_ERR_OK != err) {
		ONT_LOG1(ONTLL_ERROR, "Failed to request rsa publickey, error=%d\n", err);
        getchar();
		return -1;
	}
	//need wait request platform key
	pf_request_timeout=10000;
#endif


	err = ont_device_check_receive(dev, pf_request_timeout);
	if (ONT_ERR_OK != err) {
		ONT_LOG1(ONTLL_ERROR, "Failed to receive, error=%d\n", err);
        getchar();
		return -1;
	}

	const char *id = cfg_get_profile()->id;
	const char *reg_code = cfg_get_profile()->key;//pass = key
	err = ont_device_register(dev, product_id, reg_code, id);
	if (ONT_ERR_OK != err) {
		ONT_LOG1(ONTLL_ERROR, "Failed to register device, error=%d\n", err);
        getchar();
		return -1;
	}

	char auth_code_new[128];
	err = ont_device_auth(dev, dev->auth_code, auth_code_new);
	if (ONT_ERR_OK != err) {
		ONT_LOG1(ONTLL_ERROR, "Failed to  authentification, error=%d\n", err);
        getchar();
		return -1;
	}

	ONT_LOG1(ONTLL_INFO, "get dev id %d\n", dev->device_id);


	/*add channels*/
	int i = 0;
	int num = cfg_get_channel_num();
	struct _onvif_channel *_channels = cfg_get_channels();
	for (; i < num; i++)
	{
		err = ont_device_add_channel(dev, _channels[i].channelid, _channels[i].title, strlen(_channels[i].title), _channels[i].desc, strlen(_channels[i].desc));
	}

	uint32_t tm_last = ont_platform_time();
	volatile uint32_t tm_now;
	while (1)
	{

		if (delta)
		{
			ont_platform_sleep(delta>100 ? 100 : delta);
		}
		tm_now = ont_platform_time();
		if (tm_now - tm_last > 30)
		{
			err = ont_device_keepalive(dev);
			if (err != 0)
			{
				/*error */
				ONT_LOG1(ONTLL_ERROR, "device is not alive, error=%d", err);
				break;
			}
			tm_last = tm_now;
		}
		err = ont_device_check_receive(dev, 0);
		if (err != 0)
		{
			/*error */
			ONT_LOG1(ONTLL_ERROR, "check recv error=%d", err);
			break;
		}
		delta = ont_video_playlist_singlestep(dev);
	}

	ont_device_destroy(dev);
	printf("press any key to exit\n");
	getchar();
	return 0;
}
