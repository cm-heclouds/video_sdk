#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <rtmp_if.h>
#include <log.h>
#include <device.h>
#include <fcntl.h>
#include <pthread.h>

#include "ont_list.h"


#include "librtmp_send264.h"
#include "gmlib.h"

/* to avoid type cast for undeclared functions by declaring */
#include "send_queue.h"

#define BITSTREAM_LEN       (720 * 1024 * 3/2 )
#define AUDIO_BITSTREAM_LEN 12800
#define AU_BITSTREAM_LEN    12800
#define MAX_BITSTREAM_NUM   1

/* to avoid type cast for undeclared function by declaring */
void ont_video_live_stream_stop();

gm_system_t gm_system;
void *groupfd;
void *audio_groupfd;
void *audio_render_groupfd;

void *bindfd;
void *audio_bindfd;
void *audio_render_bindfd;
void *capture_object;
void *encode_object;
void *audio_grab_object;
void *audio_encode_object;
pthread_t enc_thread_id;
pthread_t send_thread_id;
pthread_t au_enc_thread_id;

volatile int enc_exit = 0;

void *file_object = NULL;
void *audio_render_object = NULL;
static int _cam_init = 0;
static int _aac_render_set = 0;



int _nowtime()
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	printf("CLOCK_REALTIME: %ld, %ld", ts.tv_sec, ts.tv_nsec);
	return  ts.tv_sec * 1000 + ts.tv_nsec / 10000000;
}




int video_started = 0;

int get_one_ADTS_frame(unsigned char *buffer, size_t buf_size, unsigned char *data, size_t *data_size)
{
	size_t size = 0;


	if (!buffer || !data || !data_size ) {
		return -1;
	}


	while (1) {
		if (buf_size  < 7 ) {
			return -1;
		}

		if ((buffer[0] == 0xff) && ((buffer[1] & 0xf0) == 0xf0)) {
			// profile; 2 uimsbf
			// sampling_frequency_index; 4 uimsbf
			// private_bit; 1 bslbf
			// channel_configuration; 3 uimsbf
			// original/copy; 1 bslbf
			// home; 1 bslbf
			// copyright_identification_bit; 1 bslbf
			// copyright_identification_start; 1 bslbf
			// frame_length; 13 bslbf


			size |= (((buffer[3] & 0x03)) << 11);//high 2 bit
			size |= (buffer[4] << 3);//middle 8 bit
			size |= ((buffer[5] & 0xe0) >> 5);//low 3bit

			//printf("len1=%x\n", (buffer[3] & 0x03));
			//printf("len2=%x\n", buffer[4]);
			//printf("len3=%x\n", (buffer[5] & 0xe0) >> 5);
			//printf("size=%d\r\n", (int)size);
			break;
		}
		--buf_size;
		++buffer;
	}


	if (buf_size < size) {
		return -1;
	}


	memcpy(data, buffer, size);
	*data_size = size;

	return 0;
}




extern void sound_notify(void *ctx, int chunk, int ts, const char *data, int len)
{
	int ret;
	gm_dec_multi_bitstream_t multi_bs[1];
	memset(multi_bs, 0, sizeof(multi_bs));  //clear all mutli bs
	if (len < 10) {
		return;
	}

	static int      aac_sample_rates[] = {
		96000, 88200, 64000, 48000,
		44100, 32000, 24000, 22050,
		16000, 12000, 11025,  8000,
		7350,     0,     0,     0
	};

	if (!_aac_render_set) {
		DECLARE_ATTR(file_attr, gm_file_attr_t);
		DECLARE_ATTR(audio_render_attr, gm_audio_render_attr_t);

		audio_render_groupfd = gm_new_groupfd();

		// sample_rate/sample_size/channel_type: the audio info of the input file

		int sample_rate_idx = (data[2] & 0x3c) >> 2;
		int channel = ((data[2] & 0x1) << 2) | ((data[3] & 0xc0) >> 6);

		file_object = gm_new_obj(GM_FILE_OBJECT);
		audio_render_object = gm_new_obj(GM_AUDIO_RENDER_OBJECT);

		// sample_rate/sample_size/channel_type: the audio info of the input file

		file_attr.sample_rate = aac_sample_rates[sample_rate_idx];
		file_attr.sample_size = 16;
		file_attr.channel_type = channel == 1 ? GM_MONO : GM_STEREO;
		gm_set_attr(file_object, &file_attr);

		audio_render_attr.vch = 0;
		audio_render_attr.encode_type = GM_AAC;
		audio_render_attr.block_size = 1024;
		gm_set_attr(audio_render_object, &audio_render_attr);
		audio_render_bindfd = gm_bind(audio_render_groupfd, file_object, audio_render_object);

		if (gm_apply(audio_render_groupfd) < 0) {
			perror("Error! gm_apply fail, AP procedure something wrong!");
			exit(0);
		}
		RTMP_Log(RTMP_LOGINFO, "sample_rate_idx %d, channel %d", sample_rate_idx,  channel);
		_aac_render_set = 1;
	}

	/* to avoid warnings by using type cast,
	 * remark: unsigned cast to signed */
	multi_bs[0].bindfd = audio_render_bindfd;
	multi_bs[0].bs_buf = (char *)data;
	multi_bs[0].bs_buf_len = (unsigned int)len;

	if ((ret = gm_send_multi_bitstreams(multi_bs, 1, 500)) < 0) {
		RTMP_Log(RTMP_LOGINFO, "<send bitstream fail(%d)!>\n", ret);
	} else {
		RTMP_Log(RTMP_LOGDEBUG, "rcv sound ts %d, len %d", ts, len);
		RTMP_LogHex(RTMP_LOGDEBUG, (const uint8_t *)data, 10);
	}
}

static void *video_encode_thread(void *arg)
{
	int i = 0, ret;

	gm_pollfd_t poll_fds[MAX_BITSTREAM_NUM];
	gm_enc_multi_bitstream_t multi_bs[MAX_BITSTREAM_NUM];



	memset(poll_fds, 0, sizeof(poll_fds));

	poll_fds[0].bindfd = bindfd;
	poll_fds[0].event = GM_POLL_READ;

	while (enc_exit == 0) {
		ret = gm_poll(poll_fds, MAX_BITSTREAM_NUM, 500);
		if (ret == GM_TIMEOUT) {
			//printf("Poll timeout!!");
			continue;
		}

		memset(multi_bs, 0, sizeof(multi_bs));

		if (poll_fds[0].revent.event != GM_POLL_READ) {
			continue;
		}
		if (poll_fds[0].revent.bs_len > BITSTREAM_LEN) {
			printf("bitstream buffer length is not enough! %d, %d\n",
			       poll_fds[0].revent.bs_len, BITSTREAM_LEN);
			continue;
		}


		multi_bs[0].bindfd = poll_fds[0].bindfd;

		if (poll_fds[0].bindfd == bindfd) {
			multi_bs[0].bs.bs_buf = malloc(BITSTREAM_LEN);
			multi_bs[0].bs.bs_buf_len = BITSTREAM_LEN;
		}

		multi_bs[0].bs.mv_buf = 0;
		multi_bs[0].bs.mv_buf_len = 0;

		if ((ret = gm_recv_multi_bitstreams(multi_bs, MAX_BITSTREAM_NUM)) < 0) {
			printf("video Error return value %d\n", ret);
			free(multi_bs[0].bs.bs_buf );

		} else {
			for (i = 0; i < MAX_BITSTREAM_NUM; i++) {
				if ((multi_bs[i].retval < 0) && multi_bs[i].bindfd) {
					printf("CH%d Error to receive bitstream. ret=%d\n", i, multi_bs[i].retval);
					free(multi_bs[i].bs.bs_buf );
					continue;

				}
				if (multi_bs[i].bindfd == bindfd) { //video

					g813x_PackEnqueue(&multi_bs[i].bs, 1);
					//GM8136_Send(multi_bs[i].bs.bs_buf, multi_bs[i].bs.bs_len, multi_bs[i].bs.timestamp, firstframe);
					if (!video_started) {
						video_started = 1;

					}

				}


			}
		}
	}

	return NULL;
}



static void   *audio_encode_thread(void *arg)
{
	int i = 0, ret;

	gm_pollfd_t poll_fds[MAX_BITSTREAM_NUM];
	gm_enc_multi_bitstream_t multi_bs[MAX_BITSTREAM_NUM];



	memset(poll_fds, 0, sizeof(poll_fds));

	poll_fds[0].bindfd = audio_bindfd;
	poll_fds[0].event = GM_POLL_READ;

#if 0 /* unused variable */
	int randtripdiff = 0;
#endif
	while (enc_exit == 0) {
		ret = gm_poll(poll_fds, 1, 500);
		if (ret == GM_TIMEOUT) {
			//printf("Poll timeout!!");
			continue;
		}
		if (poll_fds[0].revent.event != GM_POLL_READ) {
			continue;
		}

		if (poll_fds[0].revent.bs_len > BITSTREAM_LEN) {
			printf("bitstream buffer length is not enough! %d, %d\n",
			       poll_fds[0].revent.bs_len, BITSTREAM_LEN);
			continue;
		}

		memset(multi_bs, 0, sizeof(multi_bs));

		multi_bs[0].bindfd = poll_fds[0].bindfd;

		multi_bs[0].bs.bs_buf = malloc(AUDIO_BITSTREAM_LEN);
		multi_bs[0].bs.bs_buf_len = AUDIO_BITSTREAM_LEN;

		multi_bs[0].bs.mv_buf = 0;
		multi_bs[0].bs.mv_buf_len = 0;

		if ((ret = gm_recv_multi_bitstreams(multi_bs, 1)) < 0) {
			printf("Error return value %d\n", ret);
			free(multi_bs[0].bs.bs_buf );

		} else {

			for (i = 0; i < MAX_BITSTREAM_NUM; i++) {
				if ((multi_bs[i].retval < 0) && multi_bs[i].bindfd) {
					printf("CH%d Error to receive bitstream. ret=%d\n", i, multi_bs[i].retval);
					free(multi_bs[i].bs.bs_buf );
					continue;

				}

				if (multi_bs[i].bs.bs_len <= 0 || !video_started) {
					free(multi_bs[i].bs.bs_buf );
					continue;
				}

				g813x_PackEnqueue(&multi_bs[i].bs, 0);
				//GM8136_SendAAC(multi_bs[i].bs.bs_buf, multi_bs[i].bs.bs_len, start_ts_audio, firstaudio);
			}
		}
	}

	return NULL;
}


static void *send_thread(void *arg)
{

	int fd = RTMP_Getfd();
	int ret = 0;
#if 0 /* unsued variable */
	int i = 0;
#endif
	u_long flags = fcntl(fd, F_GETFL, 0);

	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	int s_buf_size = 6553600;

	setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char *)&s_buf_size, sizeof(int));

	fd_set rfd, wfd;
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 1000;
	g813x_init();
	while (enc_exit == 0) {
		FD_ZERO(&rfd);
		FD_ZERO(&wfd);
		FD_SET(fd, &rfd);
		FD_SET(fd, &wfd);
		ret = select(fd + 1, &rfd, &wfd, 0, &timeout);
		if (ret < 0) {
			perror("select");
			break;
		} else if (ret == 0) {
			continue;
		} else {
			if (FD_ISSET(fd, &rfd)) {
				RTMP_CheckRcv();
			}
			if (FD_ISSET(fd, &wfd)) {
				g813x_RTMPPackDequeue(NULL);
			}
		}

	}

	ont_video_live_stream_stop();


	return NULL;
}




int camera_init()
{
#if 0 /* unsed variables */
	int ret = 0;
	int key;
#endif
	DECLARE_ATTR(cap_attr, gm_cap_attr_t);
	DECLARE_ATTR(h264e_attr, gm_h264e_attr_t);
	DECLARE_ATTR(audio_grab_attr, gm_audio_grab_attr_t);
	DECLARE_ATTR(audio_encode_attr, gm_audio_enc_attr_t);
	// DECLARE_ATTR(rotation_attr, gm_rotation_attr_t);



	int ch = 0;
	gm_init();
	gm_get_sysinfo(&gm_system);

	groupfd = gm_new_groupfd();
	audio_groupfd = gm_new_groupfd();

	capture_object = gm_new_obj(GM_CAP_OBJECT);
	encode_object = gm_new_obj(GM_ENCODER_OBJECT);
	audio_grab_object = gm_new_obj(GM_AUDIO_GRAB_OBJECT);
	audio_encode_object = gm_new_obj(GM_AUDIO_ENCODER_OBJECT);


	cap_attr.cap_vch = ch;
	cap_attr.path = 2;
	cap_attr.enable_mv_data = 0;
	gm_set_attr(capture_object, &cap_attr);

	//rotation_attr.enabled=1;
	//rotation_attr.clockwise=270;
	//gm_set_attr(capture_object, &rotation_attr);

	h264e_attr.dim.width = gm_system.cap[ch].dim.width;
	h264e_attr.dim.height = gm_system.cap[ch].dim.height;

	h264e_attr.frame_info.framerate = 25;
	h264e_attr.ratectl.mode = GM_CBR;
	h264e_attr.ratectl.gop = 50;
	h264e_attr.ratectl.bitrate = 512;
	h264e_attr.b_frame_num = 0;
	h264e_attr.enable_mv_data = 0;
	h264e_attr.profile_setting.profile = GM_H264E_HIGH_PROFILE;
	//h264e_attr.profile_setting.coding=GM_H264E_CAVLC_CODING;
	gm_set_attr(encode_object, &h264e_attr);



	bindfd = gm_bind(groupfd, capture_object, encode_object);
	if (gm_apply(groupfd) < 0) {
		perror("Error! gm_apply fail, AP procedure something wrong!");
		exit(0);
	}
	//gm_set_attr(capture_object, &rotation_attr);




	audio_grab_attr.vch = ch;
	audio_grab_attr.sample_rate = 16000;
	audio_grab_attr.sample_size = 16;
	audio_grab_attr.channel_type = GM_MONO;

	gm_set_attr(audio_grab_object, &audio_grab_attr);

	audio_encode_attr.encode_type = GM_AAC;
	audio_encode_attr.bitrate = 96000;// 32000;
	audio_encode_attr.frame_samples = 1024;
	gm_set_attr(audio_encode_object, &audio_encode_attr);

	audio_bindfd = gm_bind(audio_groupfd, audio_grab_object, audio_encode_object);
	if (gm_apply(audio_groupfd) < 0) {
		perror("Error! gm_apply fail, AP procedure something wrong!");
		exit(0);
	}

	_cam_init = 1;
	_aac_render_set = 0;

	/* should return zero, i guess */
	return 0;
}

int camera_release()
{

	if (file_object) {
		gm_unbind(audio_render_bindfd);
		gm_apply(audio_render_groupfd);
		gm_delete_obj(file_object);
		gm_delete_obj(audio_render_object);
		gm_delete_groupfd(audio_render_groupfd);
		file_object = NULL;
	}
	if (_cam_init) {
		gm_unbind(bindfd);
		gm_unbind(audio_bindfd);
		gm_apply(groupfd);
		gm_apply(audio_groupfd);
		gm_delete_obj(encode_object);
		gm_delete_obj(capture_object);
		gm_delete_obj(audio_grab_object);
		gm_delete_obj(audio_encode_object);
		gm_delete_groupfd(groupfd);
		gm_delete_groupfd(audio_groupfd);
		gm_release();
	}
	_cam_init = 0;
	return 0;

}

typedef  struct {
	char playflag[32];
	char publishurl[255];
	char deviceid[16];
	int channel;
	char rvod;
	char closeflag; //0, nothing; 1 server closed,
	void *playctx;
} t_playlist;

ont_list_t *g_list = NULL;

#define VARAIBLE_CHECK if(!g_list) { g_list = ont_list_create();}

#if 0 /* defined but not used */
static int compareliveFlag(t_playlist *d1, t_playlist *d2);
#endif

#if 0 /* defined but not used */
static int ont_video_live_stream_start_level(void *dev, int channel, const char *push_url, const char *deviceid, int level)
{
	printf("ont_video_live_stream_start_level \r\n");

	return -1;
}
#endif

int ont_video_live_stream_start(void *dev, int channel, const char *push_url, const char *deviceid)
{
	printf("ont_video_live_stream_start \r\n");
	return 0;
}

void ont_video_live_stream_play(void *dev, int channel, const char *push_url, uint64_t deviceid)
{
#if 0 /* unused variables */
	int i = 0;
	char key;
#endif
	int ret = 0;
	int ch;
	ch = 0;
	char url[256] = { 0 };

	//初始化并连接到服务器
	ont_video_live_stream_stop();

	sleep(2);
	camera_init();
	ont_platform_snprintf(url, sizeof(url), "%s?%lld", push_url, deviceid);

	printf("url %s \n", url);
	//ont_platform_snprintf(url, sizeof(url), "%s", "rtmp://192.168.200.135:1935/live/demo");
	RTMP264_Connect(url);
	//GM8136_InitWidth(gm_system.cap[0].dim.width,gm_system.cap[0].dim.height,gm_system.cap[0].framerate);

	enc_exit = 0;
	ret = pthread_create(&enc_thread_id, NULL, video_encode_thread, (void *)ch);
	if (ret < 0) {
		perror("create encode thread failed");
#if 0 /* return with a value in funcation returning void */
		return -1;
#endif
		return ;
	}

	ret = pthread_create(&au_enc_thread_id, NULL, audio_encode_thread, (void *)ch);
	if (ret < 0) {
		perror("create encode thread failed");
#if 0 /* return with a value in funcation returning void */
		return -1;
#endif
		return ;
	}

	ret = pthread_create(&send_thread_id, NULL, send_thread, (void *)ch);
	if (ret < 0) {
		perror("create audio encode thread failed");
#if 0 /* return with a value in funcation returning void */
		return -1;
#endif
		return ;
	}

	return;
}

int _start_play = 0;
int ont_video_live_stream_rtmp_publish(ont_device_t *dev, int32_t channel, uint8_t protype, uint16_t ttl_min, const char *push_url)
{
	VARAIBLE_CHECK;
	if (!_start_play) {
		_start_play = 1;
	} else {
#if 0 /* return with no value in function returning non-void */
		return;
#endif
		/* return this value, i guess */
		return ONT_RET_CHANNEL_ALREADY_START;
	}
	ont_video_live_stream_play(dev, channel, push_url, dev->device_id);
	return ONT_RET_SUCCESS;;
}

void ont_video_live_stream_stop()
{
	printf("ont_video_live_stream_stop\n");
	enc_exit = 1;
	usleep(100);
	if (enc_thread_id) {
		pthread_join(enc_thread_id, NULL);
		enc_thread_id = (pthread_t)NULL; /* to avoid warning by using type cast */
	}

	if (au_enc_thread_id) {
		pthread_join(au_enc_thread_id, NULL);
		au_enc_thread_id = (pthread_t)NULL; /* to avoid warning by using type cast */
	}
	/* to avoid warning by using type cast,
	 * */
	g813x_clearqueue((void *)0);
	camera_release();
	RTMP264_Close();
	printf("ont_video_live_stream_stop end \n");
	_start_play = 0;
	extern int audio_first;
	extern int video_first;

	audio_first = 1;
	video_first = 1;


}
#if 0 /* defined but not used */
static int compareliveFlag(t_playlist *d1, t_playlist *d2)
{
	if (d1->channel == d2->channel) {
		return 0;
	}
	return -1;
}
#endif

int ont_video_live_stream_ctrl(void *dev, int channel, int stream)
{
	printf("ont_video_live_stream_ctrl \r\n");

	/* return zero, i guess */
	return 0;
}

int startfindFlag(t_playlist *d1, t_playlist *d2)
{
	/* return zero, i guess */
	return 0;
}



int compareFlag(t_playlist *d1, t_playlist *d2)
{
	if (strcmp(d1->playflag, d2->playflag) == 0) {
		return 0;
	}
	return -1;
}


int ont_video_stream_make_keyframe(void *dev, int channel)
{
	//need implement.
	return 0;
}

int ont_video_playlist_singlestep(void *dev)
{
	return -1;
}

void ont_keepalive_resp_callback(ont_device_t *dev)
{
	RTMP_Log(RTMP_LOGINFO, "keepalive response");
}
