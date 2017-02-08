#ifndef _WIN32
#include <sys/time.h>
#include <netdb.h>
#include <arpa/inet.h>
#else
#include "winsock2.h"
#endif

//#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include "ont/mqtt.h"
#include "ont/log.h"

extern void usleep(int);

 

/* 设置 log 函数*/
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
/* mqtt 订阅topic */
int ont_mqtt_subscribe_sample(ont_device_t *dev)
{ 
	int err =0;
	
	/* 订阅的topic 数组*/
	const char *subscribe_topic_array[]={"topic_test","hello_iot"};
	
	/* 参数： dev, 订阅的topic数组,该数组的长度(topic的个数) */
	err = ont_mqtt_sub_topic(dev,(const char **)&subscribe_topic_array,2);

	return err;
}
/* mqtt 取消订阅topic */
int ont_mqtt_unsub_sample(ont_device_t *dev)
{
	int err = 0;
	/* 取消订阅的topic 数组*/
	const char *unsub_topic_array[]={"topic_test","hello_iot"};
	
	/* 参数： dev, 取消订阅的topic数组,该数组的长度(topic的个数) */
	err = ont_mqtt_unsub_topic(dev,(const char **)&unsub_topic_array,2);
	return err;
}
int ont_mqtt_publish_sample(ont_device_t *dev)
{
	/*
		调用ont_mqtt_publish(dev);需要指定推送的topic的名称,
		推送的具体内容，以及内容的长度，qos的级别(1：返回确认ack，0：不返回)
		其中topic_name不能为系统保留的topic_name($dp,$creq,$crsp),
		
	*/
	int err = 0;
	/* publish 的topic的名称 */
	const char *publish_topic_name = "hello_iot";
	
	/* publish 按topic推送的内容 */
	const char *publish_content = "this is a test publish msg";
	
	/* publish qos级别 */
	uint8_t qos_level = 1;
	
	/*
		参数：dev, topic名称，推送的内容的起始地址，推送内容的长度，qos级别
	*/
	size_t publish_content_size = strlen(publish_content);
	err = ont_mqtt_publish(dev,publish_topic_name
	,publish_content,publish_content_size,qos_level);
	
	return err;
}
int main( int argc, char *argv[] )
{
	ont_device_t *dev;
	int err, i;
	struct hostent *hosts;
	int32_t next_dp_time;
	
	int product_id = 339;
	ont_device_cmd_t *cmd = NULL;
	
	/* 设置日志记录的回调函数 */
	ont_log_set_logger(NULL, sample_log);

	/* 创建设备，指定产品(product_id),创建的设备类型(mqtt),设备的名称 */
	err = ont_device_create(product_id, ONTDEV_MQTT, "MQTT-TEST-DEV", &dev);
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

	/* 连接server：device实例,server地址,端口,注册码,auth_info,keepalive时间(单位：s) */
	err = ont_device_connect(dev,
	inet_ntoa(*(struct in_addr*)hosts->h_addr_list[i]),
			ONT_SERVER_PORT,
			"0123456789abcdef",
			"MQTT-OneNET;0000005",
			120);
	if (ONT_ERR_OK != err)
	{
		ont_device_destroy(dev);

		ONT_LOG1(ONTLL_ERROR, "Failed to connect to the server, error=%d", err);
		return -1;
	}

	next_dp_time = (int32_t)time(NULL) + 15;
	
	/* test mqtt subscribe*/
	err = ont_mqtt_subscribe_sample(dev);
	if (ONT_ERR_OK != err)
	{
		ONT_LOG1(ONTLL_ERROR, "Failed to subscribe topics, error=%d", err);  
		return err;
	}
	usleep(1000 * 3000);
	
	/* test mqtt unsubscribe*/
	err = ont_mqtt_unsub_sample(dev);
	if (ONT_ERR_OK != err)
	{
		ONT_LOG1(ONTLL_ERROR, "Failed to unsubscribe topics, error=%d", err);  
		return err;
	}
	
	usleep(1000 * 3000);
	/* re subscribe the topic */
	err = ont_mqtt_subscribe_sample(dev);
	if (ONT_ERR_OK != err)
	{
		ONT_LOG1(ONTLL_ERROR, "Failed to subscribe topics, error=%d", err);  
		return err;
	}
	
	/* test mqtt publish, you should subscribe the topic first */
	err = ont_mqtt_publish_sample(dev);
	if (ONT_ERR_OK != err)
	{
		ONT_LOG1(ONTLL_ERROR, "Failed to publish messages, error=%d", err); 
		return err;		
	}
	usleep(1000 * 3000);
	while (1)
	{
		/* 保活，发送心跳包 */
		err = ont_device_keepalive(dev);
		if (ONT_ERR_OK != err)
		{
			ONT_LOG1(ONTLL_ERROR, "device is not alive, error=%d", err);
			break;
		}

		/* test send data_points */
		if (next_dp_time <= time(NULL))
		{
			err = ont_device_add_dp_double(dev, "temperature", 12.78);
			if (ONT_ERR_OK != err)
			{
				ONT_LOG1(ONTLL_ERROR, "Failed to add the [temperature] data point, error=%d", err);
				break;
			}
			
			err = ont_device_add_dp_int(dev, "speed", 80);
			if (ONT_ERR_OK != err)
			{
				ONT_LOG1(ONTLL_ERROR, "Failed to add [speed] data point, error=%d", err);
				break;
			}

			/* 发送数据 */
			err = ont_device_send_dp(dev);
			if (ONT_ERR_OK != err)
			{
				ONT_LOG1(ONTLL_ERROR, "Failed to send the data points, error=%d", err);
				break;
			}

			next_dp_time = (int32_t)time(NULL) + 15;
			ONT_LOG0(ONTLL_INFO, "Send data points successfully.");
		}
		/* 获取平台下发命令，获取之后需调用ont_device_cmd_destroy(cmd); */
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
			
			/* 回复平台下发的命令 */
			err = ont_device_reply_cmd(dev, cmd->id, "got it", strlen("got it"));
			if (ONT_ERR_OK != err)
			{
				ONT_LOG1(ONTLL_ERROR, "Failed to reply cmd, error=%d", err);
				break;
			}
			/* 销毁获取的命令 */
			ont_device_cmd_destroy(cmd);
		}
		
		usleep(1000 * 20); /* 20ms */
	}
	ont_device_destroy(dev);
	return 0;
}
