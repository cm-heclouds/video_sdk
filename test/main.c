#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

#include "error.h"
#include "platform.h"
#include "security.h"
#include "device.h"
#include "platform_cmd.h"
#include "rtmp_if.h"
#include "rvod.h"
#include "ont_bytes.h"


const uint64_t productid = 7420;

extern int initialize_environment(void);

extern void cleanup_environment(void);

#define ASSERT(v) if(!(v)) (*(int*)0x00)=0x00;


static int32_t  _ont_cmd_live_stream_ctrl(void *dev,  int32_t channel, int32_t stream)
{
	return ONT_RET_NOT_SUPPORT;
}



static int32_t _ont_cmd_dev_ptz_ctrl(void *dev, int32_t channel, int32_t mode, t_ont_video_ptz_cmd ptz, int32_t speed)
{
	return ONT_RET_NOT_SUPPORT;
}


static int32_t _ont_cmd_dev_query_files(void *dev, int32_t channel, int32_t startindex, int32_t max, ont_video_file_t **files, int32_t *filenum, int32_t *totalnum)
{
	return ONT_RET_NOT_SUPPORT;
}

ont_cmd_callbacks_t _g_ont_cmd =
{
	_ont_cmd_live_stream_ctrl,
	_ont_cmd_dev_ptz_ctrl,
	_ont_cmd_dev_query_files
};



static void _ont_keepalive_resp_callback(ont_device_t *dev)
{
	printf("keep alive response\n");
}

/*no need response here*/
static  int32_t _ont_live_stream_start(ont_device_t *dev, int32_t channel, uint8_t protype, uint16_t ttl_min, const char *push_url)
{
	return ONT_RET_NOT_SUPPORT;
}

/*no need response here*/
static int32_t _ont_vod_start_notify(ont_device_t *dev, int32_t channel, uint8_t protype, ont_video_file_t *fileinfo, const char *playflag, const char*pushurl, uint16_t ttl )
{
	return ONT_RET_NOT_SUPPORT;
}

int32_t _user_defind_cmd(ont_device_t *dev, ont_device_cmd_t *cmd)
{	/*response*/
	if (cmd->need_resp)
	{
		ont_device_reply_user_defined_cmd(dev, ONT_RET_NOT_SUPPORT, cmd->id, NULL, 0);
	}
	else
	{
		ont_device_reply_user_defined_cmd(dev, ONT_RET_NOT_SUPPORT, NULL, NULL, 0);
	}
	return ONT_ERR_OK;
}

ont_device_callback_t _g_device_cbs =
{
	_ont_keepalive_resp_callback,
	_ont_live_stream_start,
	_ont_vod_start_notify,
	_user_defind_cmd
};



void test_security()
{
	char buf_in[1111];
	char dec_out[1170];
	char buf_out[2048];
	int out_size = 2048;
	int dec_size = 0;
	void *rsa2;
	char keybuf[128];
	memset(buf_in, 0x33, sizeof(buf_in));

	ont_security_init();
	void *rsa;
	ont_security_rsa_generate(&rsa, NULL, 0, NULL, 0);
	ont_security_rsa_get_pubkey(rsa, keybuf, 128);
	ont_security_rsa_generate(&rsa2, keybuf, 128, NULL, 0);

	ont_security_rsa_encrypt(rsa2, buf_in, sizeof(buf_in), buf_out, &out_size);
	dec_size = sizeof(dec_out);
	memset(dec_out, 0x00, sizeof(dec_out));

	ont_security_rsa_decrypt(rsa, buf_out, out_size, dec_out, &dec_size);

}

int main(int argc, char *argv[])
{

    int ret = 0;
	char    token[40];

	initialize_environment();
	ont_security_init();

    ont_device_t *dev = NULL;
    
    ont_device_create(&dev, & _g_device_cbs);
	ont_device_set_platformcmd_handle(dev, &_g_ont_cmd);
	dev->product_id = productid;

	ret = ont_device_get_acc(dev, token);
	ASSERT(ret == 0);


	ret = ont_device_connect(dev);
	ASSERT(ret == 0);

    ret = ont_device_verify(dev, token);
	ASSERT(ret == 0);
	ret = ont_device_request_rsa_publickey(dev);
	ASSERT(ret == 0);

	ret= ont_device_check_receive(dev, DEVICE_MSG_TIMEOUT);
	ASSERT(ret == 0);
    const char *device_info = "12344321";

    const char *reg_code = "O9iBJF8WQ8mQoHYS";

    ret = ont_device_register(dev, productid, reg_code, device_info);
  	ASSERT(ret == 0);
    char auth_code_new[128];
    ret = ont_device_auth(dev, dev->auth_code, auth_code_new);
	ASSERT(ret == 0);

    uint32_t channels[50];
    size_t size;
    ret = ont_device_get_channels(dev, channels, &size);
    printf("get channels ret = %d\n", ret);

    uint32_t channel = 1;
    ret = ont_device_del_channel(dev, channel);
    printf("del channels ret = %d\n", ret);

    const char *title = "miao";
    uint32_t tsize = strlen(title);
    const char *desc = "miao";
    uint32_t dsize = strlen(desc);
    ret = ont_device_add_channel(dev, channel, title, tsize, desc, dsize);
    printf("add channels ret = %d\n", ret);
	
    char upload_data[10] = { 0 };
	upload_data[0] = 't';
	upload_data[9] = 'e';
	ont_device_data_upload(dev, (uint64_t)0x0101010101010101, upload_data, 10);
	ASSERT(ret == 0);

	uint32_t tm_last = ont_platform_time();
	uint32_t tm_now;
	while (1)
	{
		tm_now = ont_platform_time();
		if (tm_now - tm_last > 60)
		{
			ret = ont_device_keepalive(dev);
			if (ret != 0)
			{
				/*error */
				break;
			}
			tm_last = tm_now;
		}
		ret = ont_device_check_receive(dev, 1000);
		if (ret != 0)
		{
			break;
		}
	}
    ont_device_disconnect(dev);

    ont_device_destroy(dev);
	printf("exit ret %d\n",ret);
	getchar();


    return ret;
}