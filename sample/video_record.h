#ifndef _ONT_RECORD_H
#define _ONT_RECORD_H


# ifdef __cplusplus
extern "C" {
# endif

typedef struct _t_dev_record_status {
	int channel;
	int status; //1 platform begin to record,  2 platform stop to record
	uint64_t record_starttime; //seconds
	uint64_t record_endtime;
	uint64_t recorded_starttime;
	uint64_t recorded_endtime;
	char url[255];
	void *onvif_ctx;
	MP4FileHandle hMP4File;
	char         path[64];
	uint8_t      stop;/*stop status*/
	char         state;/*1 not record, 2 recording, 3 record end, 4 record sending*/
	int          appendmeta[2];
	MP4TrackId   video;
	MP4TrackId   audio;
} t_dev_record_ctx;


int ont_record_channel_cmd_update(void *dev, int32_t channel, int32_t status, int32_t seconds, char url[255]);
int ont_record_check(void *dev, int32_t channel, ont_onvif_playctx *onvif_ctx);

t_dev_record_ctx *ont_record_start(void *dev, int32_t channel, void *onvif_ctx);

int ont_record_intime(void *dev, int32_t channel);

int  ont_record_singlestep(t_dev_record_ctx *rc);

int ont_record_stop(void *dev, int32_t channel);

int ont_record_check(void *dev, int32_t channel, ont_onvif_playctx *onvif_ctx);


int ont_record_send_check(void *dev, int32_t channel);

t_rtmp_vod_ctx *ont_record_send_start(void *dev, int32_t channel);




# ifdef __cplusplus
}
# endif

#endif


