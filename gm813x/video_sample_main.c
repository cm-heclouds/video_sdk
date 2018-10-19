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
#include "platform_cmd.h"
#include "sample_config.h"

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



#if(PROTOCOL_SECURITY_MBEDTLS ||  PROTOCOL_SECURITY_OPENSSL)
#define ONT_VIDEO_PROTOCOL_ENCRYPT 1
#endif


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

int32_t ont_video_live_stream_ctrl(void *dev, int32_t channel, int32_t level);



void ont_keepalive_resp_callback(ont_device_t *dev);
int ont_video_live_stream_start(void *dev, int channel);
int ont_video_live_stream_rtmp_publish(ont_device_t *dev, int32_t channel, uint8_t protype, uint16_t ttl_min, const char *push_url);


ont_device_callback_t _g_device_cbs = {
	ont_keepalive_resp_callback,
	ont_video_live_stream_rtmp_publish,
	NULL,
	NULL
};

ont_cmd_callbacks_t _g_ont_cmd = {
	ont_video_live_stream_ctrl,
	NULL,
	NULL
};

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
	ont_device_t *dev = NULL;
	int err;
#if 0 /* unused variables */
	int delta = 0;
	ont_device_cmd_t *cmd = NULL;
#endif

	RTMP_LogSetLevel(RTMP_LOGINFO);

	char cfg_path[256] = {0};
	GetWorkDir(cfg_path);
	strcat(cfg_path, "config.json");

	RTMP_Log(RTMP_LOGINFO, "cfg_path:%s\n", cfg_path);
#ifdef WIN32
	cfg_initilize("config.json");

	//cfg_initilize(cfg_path);
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
	    sigaction(SIGPIPE, &sa, 0) == -1) {
		//屏蔽SIGPIPE信号
		exit(1);
	}
#endif


	char token[TOKENBUFSIZE];


	err = ont_device_create(&dev, &_g_device_cbs);
	if (ONT_ERR_OK != err) {
		RTMP_Log(RTMP_LOGERROR, "Failed to create device instance, error=%d\n", err);
		abort();
	}
 
	err = ont_device_set_platformcmd_handle(dev, &_g_ont_cmd);
	if (ONT_ERR_OK != err) {
		ont_device_destroy(dev);
		RTMP_Log(RTMP_LOGERROR, "Failed to set platformcmd handle, error=%d\n", err);
        return -1;
	}

__start_connect:
    ont_platform_sleep(5000); 
	dev->product_id = product_id;
	dev->device_id = device_id;
	memset(token, 0x00, sizeof(token));
	err = ont_device_get_acc(dev, token);
	if (ONT_ERR_OK != err) {
		RTMP_Log(RTMP_LOGERROR, "Failed to get acc service, error=%d\n", err);
		goto __start_connect;
	}

	err = ont_device_connect(dev);
	if (ONT_ERR_OK != err) {
		RTMP_Log(RTMP_LOGERROR, "Failed to connect to the server, error=%d\n", err);
		goto __start_connect;
	}
	
	err = ont_device_verify(dev, token);
	if (ONT_ERR_OK != err) {
		RTMP_Log(RTMP_LOGERROR, "Failed to verify device, error=%d\n", err);
		goto __start_connect;
	}
	
	int pf_request_timeout = 1000;
#if ONT_VIDEO_PROTOCOL_ENCRYPT
	err = ont_device_request_rsa_publickey(dev);
	if (ONT_ERR_OK != err) {
		RTMP_Log(RTMP_LOGERROR, "Failed to request rsa publickey, error=%d\n", err);
		goto __start_connect;
	}
	//need wait request platform key
	pf_request_timeout = 10000;
#endif

	err = ont_device_check_receive(dev, pf_request_timeout);
	if (ONT_ERR_OK != err) {
		RTMP_Log(RTMP_LOGERROR, "Failed to receive, error=%d\n", err);
		goto __start_connect;
	}

	char id[30];
	GetLocalMac(id);
	const char *reg_code = cfg_get_profile()->key;//pass = key
	err = ont_device_register(dev, product_id, reg_code, id);
	if (ONT_ERR_OK != err) {
		RTMP_Log(RTMP_LOGERROR, "Failed to register device, error=%d\n", err);
		goto __start_connect;
	}

	char auth_code_new[128];
	err = ont_device_auth(dev, dev->auth_code, auth_code_new);
	if (ONT_ERR_OK != err) {
		RTMP_Log(RTMP_LOGERROR, "Failed to  authentification, error=%d\n", err);
		goto __start_connect;
	}

	RTMP_Log(RTMP_LOGINFO, "get dev id %lld\n", dev->device_id);


	/*add channels*/
	int i = 0;
	for (; i < 1; i++) {
		err = ont_device_add_channel(dev, 1, "1", 1, "8136", 4);
	}

	uint32_t tm_last = ont_platform_time();
	volatile uint32_t tm_now;
	while (1) {
		ont_platform_sleep(100);
		tm_now = ont_platform_time();
		if (tm_now - tm_last > 30) {
			err = ont_device_keepalive(dev);
			if (err != 0) {
				/*error */
				RTMP_Log(RTMP_LOGERROR, "device is not alive, error=%d", err);
				break;
			}
			tm_last = tm_now;
		}
		err = ont_device_check_receive(dev, 0);
		if (err != 0) {
			/*error */
			RTMP_Log(RTMP_LOGERROR, "check recv error=%d", err);
			break;
		}
	}

	goto __start_connect;
	//not run here
	ont_device_destroy(dev);
	printf("press any key to exit\n");
	getchar();
	return 0;
}
