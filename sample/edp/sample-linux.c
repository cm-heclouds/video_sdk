#include <sys/time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <ont/edp.h>
#include <ont/log.h>
#include "Openssl.h"
static void sample_log(void *ctx, ont_log_level_t ll , const char *format, ...)
{
    static const char *level[] = {
	"DEBUG", "INFO", "WARNING", "ERROR", "FATAL"
    };
    va_list ap;
    struct timeval tv;
    time_t t;
    struct tm *st;

    gettimeofday(&tv, NULL);

    t = tv.tv_sec;
    st = localtime(&t);

    printf("[%02d:%02d:%02d.%03d] [%s] ", st->tm_hour, st->tm_min,
	    st->tm_sec, (int)tv.tv_usec/1000, level[ll]);

    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
    printf("\n");
}
void trans_data_cb_func_ex(const char*svr_name,size_t svr_len,
	const char* data,size_t data_len,ont_device_t *dev,void* user_data)
{
    int i = 0;
    printf("********trans_cb**********\n");
    printf("get %d bytes svr_name: ",(int)svr_len);
    while(i<svr_len)
    {
	printf("%c",svr_name[i++]);
    }
    printf("\n");
    i = 0;
    printf("get %d bytes data: ",(int)data_len);
    while(i<data_len)
    {
	printf("%c",data[i++]);
    }
    printf("user_data:%s\n",(char *)user_data);
    printf("\n");

}
void trans_data_cb_func(const char*svr_name,size_t svr_len,
	const char* data,size_t data_len)
{
    int i = 0;
    printf("********trans_cb**********\n");
    printf("get %d bytes svr_name: ",(int)svr_len);
    while(i<svr_len)
    {
	printf("%c",svr_name[i++]);
    }
    printf("\n");
    i = 0;
    printf("get %d bytes data: ",(int)data_len);
    while(i<data_len)
    {
	printf("%c",data[i++]);
    }
    printf("\n");

}

void pushdata_cb_func(const char*src_devid,size_t len,
	const char* data,size_t data_len)
{
    int i = 0;
    printf("********push_cb**********\n");
    printf("get %d bytes devid: ",(int)len);
    while(i<len)
    {
	printf("%c",src_devid[i++]);
    }
    printf("\n");
    i = 0;
    printf("get %d bytes data: ",(int)data_len);
    while(i<data_len)
    {
	printf("%c",data[i++]);
    }
    printf("\n");

}

int main( int argc, char *argv[] )
{
    ont_device_t *dev;
    int err, i;
    struct hostent *hosts;
    int32_t next_dp_time;
    char* trans_data = "I'm data";
    ont_device_cmd_t *cmd = NULL;
    char* data = NULL;
    char* data_decode = NULL;
    unsigned char key_md5[16];
    ont_log_set_logger(NULL, sample_log);
/*    ont_log_set_log_level(ONTLL_FATAL);*/
    /*zuwang en|decrypt demo start*/
    MD5String((const unsigned char*)"app@2017",key_md5);
    InitAes(key_md5);
    data = Zuwang_encrypt("MTcwNjM1NjE3Nzk2MTIzMTIz");
    data_decode = Zuwang_decrypt(data);
    ONT_LOG1(ONTLL_INFO,"%s\n",data);
    ONT_LOG1(ONTLL_INFO,"%s\n",data_decode);
    ont_platform_free(data);
    ont_platform_free(data_decode);
    /*zuwang en|decrypt demo end*/

    err = ont_device_create(85, ONTDEV_EDP, "auto_register", &dev);
    if (ONT_ERR_OK != err) {
	ONT_LOG1(ONTLL_ERROR, "Failed to create device instance, error=%d", err);
	return -1;
    }


    hosts = gethostbyname(ONT_SERVER_ADDRESS);
    if (NULL == hosts)
    {
	ONT_LOG2(ONTLL_ERROR, "Failed to get the IP of the server('%s'), "
		"for: %d", ONT_SERVER_ADDRESS, h_errno);
	return -1;
    }

    for (i = 0; hosts->h_addr_list[i]; ++i)
    {
	if (AF_INET == hosts->h_addrtype)
	    break;
    }

    if (NULL == hosts->h_addr_list[i])
    {
	ONT_LOG0(ONTLL_ERROR, "No invalide address of the server.");
	return -1;
    }
    ont_edp_set_transdata_cb(dev,trans_data_cb_func);
    ont_edp_set_transdata_cb_ex(dev,trans_data_cb_func_ex);
    ont_edp_set_pushdata_cb(dev,pushdata_cb_func);
    err = ont_device_connect_ex(dev,
	    inet_ntoa(*(struct in_addr*)hosts->h_addr_list[i]),
	    ONT_SERVER_PORT,
	    "omq_test",
	    "0123456789abcdefhikj",
	    "test_edp_sample",
	    30);
    if (ONT_ERR_OK != err)
    {
	ont_device_destroy(dev);

	ONT_LOG1(ONTLL_ERROR, "Failed to connect to the server, error=%d", err);
	return -1;
    }

    next_dp_time = (int32_t)time(NULL) + 3;
    while (1)
    {
	err = ont_device_keepalive(dev);
	if (ONT_ERR_OK != err)
	{
	    ONT_LOG1(ONTLL_ERROR, "device is not alive, error=%d", err);
	    if (ONT_ERR_OK != err)
	    {
		ONT_LOG1(ONTLL_ERROR, "Failed to send transdata , error=%d", err);
		break;
	    }

	    break;
	}
	if (next_dp_time <= time(NULL))
	{
	    err = ont_device_add_dp_double(dev, "temperature", 12.78);
	    if (ONT_ERR_OK != err)
	    {
		ONT_LOG1(ONTLL_ERROR, "Failed to add the [temperature] data point, error=%d", err);
		break;
	    }
	    err = ont_edp_send_transdata(dev,"ccomqtest",trans_data,strlen(trans_data));
	    if (ONT_ERR_OK != err)
	    {
		ONT_LOG1(ONTLL_ERROR, "Failed to send transdata , error=%d", err);
		break;
	    }
	    /*err = ont_edp_send_pushdata(dev,"1684470",trans_data,strlen(trans_data));
	    if (ONT_ERR_OK != err)
	    {
		ONT_LOG1(ONTLL_ERROR, "Failed to send pushdata , error=%d", err);
		break;
	    }*/	    
            err = ont_device_add_dp_int(dev, "speed", 80);
	    if (ONT_ERR_OK != err)
	    {
		ONT_LOG1(ONTLL_ERROR, "Failed to add [speed] data point, error=%d", err);
		break;
	    }

	    err = ont_device_send_dp(dev);
	    if (ONT_ERR_OK != err)
	    {
		ONT_LOG1(ONTLL_ERROR, "Failed to send the data points, error=%d", err);
		break;
	    }

	    next_dp_time = (int32_t)time(NULL) + 10;
	    ONT_LOG0(ONTLL_INFO, "Send data points successfully.");
	}
	cmd = ont_device_get_cmd(dev);

	if(NULL != cmd)
	{
	    printf("recv cmd:\n");
	    printf("cmd id : %s\n", cmd->id);
	    printf("cmd data:\n");
	    for (i = 0; i < cmd->size; i++)
	    {
		printf("%c",(cmd->req[i])); 
	    }
	    putchar('\n');

	    err = ont_device_reply_cmd(dev, cmd->id, "got it", strlen("got it"));
	    if (ONT_ERR_OK != err)
	    {
		ONT_LOG1(ONTLL_ERROR, "Failed to reply cmd, error=%d", err);
		break;
	    }
	    ont_device_cmd_destroy(cmd);
	}
	ont_platform_sleep(50);/* 50 ms*/
    }

    ont_device_destroy(dev);
    return 0;
}
