
#ifndef __ONT_MQTT_VIDEO_H__
#define __ONT_MQTT_VIDEO_H__


void ont_video_set_callbacks(void *dev, t_ont_video_dev_ctrl_callbacks *callbacks);
int ont_video_dev_set_channels(void *_dev, int channels);

int ont_video_dev_fileinfo_upload(void *_dev, int channel, t_ont_video_file *list, int n);



int ont_videocmd_parse(void *dev, const char*str);


#endif /*__ONT_MQTT_VIDEO_H__*/
