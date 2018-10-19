#ifndef _LIBRTMP_SENDH264_H_
#define _LIBRTMP_SENDH264_H_

# ifdef __cplusplus
extern "C" {
# endif





int RTMP264_Connect(const char* url);     
int GM8136_SPS(char *buf, int pos,  int buf_size)  ;
int GM8136_InitWidth(unsigned int  width,unsigned int height, unsigned int fps);

//int GM8136_Send(char *buf, int buf_size,int first)  ;

int GM8136_Send(char *buf, int buf_size, int keyframe, unsigned int tick)  ;


//int GM8136_SendAAC(unsigned char* data,unsigned int size,int first);
int GM8136_SendAAC(unsigned char* data,unsigned int size,unsigned int audio_tick);

int RTMP_CheckRcv();

/**
 * 断开连接，释放相关的资源。
 *
 */    
void RTMP264_Close();  

int GM8136_Rtmp_Rcvhandler();

int RTMP_Getfd();

# ifdef __cplusplus
}
# endif

# endif



