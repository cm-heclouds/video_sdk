#ifndef ONT_INCLUDE_RTMP_IF_H_
#define ONT_INCLUDE_RTMP_IF_H_

#include "config.h"

# ifdef __cplusplus
extern "C" {
# endif

/**RTMP medtadata 描述*/
typedef struct _RTMPMetadata {
	double duration;               /**< 视频播放时间，直播视频置0*/
	unsigned int    width;         /**< 视频宽度*/
	unsigned int    height;        /**< 视频高度*/
	unsigned int    videoDataRate; /**< 视频位率bps*/
	unsigned int    frameRate;     /**< 视频帧率 */
	unsigned int    videoCodecid;  /**< FLV视频格式参数，AVC 写7 */
	unsigned char   hasAudio;      /**< 是否包含音频 */
	unsigned int    audioDataRate; /**< 音频位率kbps*/
	unsigned int    audioSampleRate;  /**< 音频播放频率, kbps*/
	unsigned int    audioCodecid;     /**< FLV 音频格式参数, AAC 写10*/
} RTMPMetadata;

/************************************************************************/
/*@brief 创建一个连接服务器得RTMP通道                                    */
/*@param pushurl 推送目标地址                                           */
/*@param sock_timeout_seconds socket 超时时间                          */
/************************************************************************/
void *rtmp_create_publishstream(const char *pushurl, unsigned int sock_timeout_seconds, const char *deviceid);


typedef void(*rtmp_sound_notify)(void *ctx, int chunk, int ts, const char *data, int len);
int rtmp_set_sound_notify(void *rtmp, rtmp_sound_notify cb, void *ctx);


typedef void(*rtmp_video_notify)(void *_ctx, int channel_id, int ts, int nLen, const char *data);
int rtmp_set_video_notify(void *rtmp, rtmp_video_notify cb, void *ctx);


/************************************************************************/
/*@brief 关闭RTMP 连接并释放资源                                        */
/*@param rtmp rtmp_create_publishstream  返回                           */
/************************************************************************/
void rtmp_stop_publishstream(void *rtmp);


/************************************************************************/
/*@brief 发送MetaData                                             */
/*@param RTMP rtmp指针                                            */
/*@param metadata  媒体数据描述                                          */
/************************************************************************/
int rtmp_send_metadata(void *rtmp, RTMPMetadata *metadata);


/************************************************************************/
/*@brief 发送H264的sps，pps 数据                                   */
/*@param sps sps地址                                           */
/*@param sps_len  sps 长度                                          */
/*@param pps pps地址                                           */
/*@param pps_len  pps 长度                                          */
/*@param ts  时间戳                                          */
/************************************************************************/
int rtmp_send_spspps(void *rtmp, unsigned char *sps, int sps_len, unsigned char *pps, int pps_len, int ts);


/************************************************************************/
/*@brief 发送视频帧                                                   */
/*@param rtmp, rtmp 指针                                               */
/*@param data, 视频数据采用FLV Tag data格式封装                         */
/*@param size, 视频数据大小                                             */
/*@param ts, 视频时间戳                                                 */
/*@param keyFrame, 是否关键帧                                          */
/************************************************************************/
int rtmp_send_videodata(void *rtmp, unsigned char *data, unsigned int size, unsigned int ts, unsigned int keyFrame);


/************************************************************************/
/*@brief 发送H264视频帧                                                   */
/*@param rtmp, rtmp 指针                                               */
/*@param data, 视频数据为264视频数据                       */
/*@param size, 视频数据大小                                             */
/*@param ts, 视频时间戳                                                 */
/*@param keyFrame, 是否关键帧                                          */
/************************************************************************/
int rtmp_send_h264data(void *rtmp, unsigned char *data, unsigned int size, unsigned int ts, unsigned int keyFrame);



/************************************************************************/
/*@brief RTMP 发送RTMP声音数据接收处理                                   */
/*@param rtmp rtmp指针                                                  */
/*@param headertag  音频数据格式                                        */
/*@param data 音频数据                                                  */
/*@param size 音频大小                                                  */
/*@param ts 音频时间戳                                                  */
/************************************************************************/
int rtmp_send_audiodata(void *rtmp, unsigned char headertag, unsigned char *data, unsigned int size, unsigned int ts, unsigned int headertype);



/************************************************************************/
/*@brief RTMP 接收处理                                                  */
/*@param  rtmp rtmp指针                                                 */
/************************************************************************/
int rtmp_check_rcv_handler(void *rtmp);

/************************************************************************/
/*@brief  创建一个FLV audio header tag                                  */
/*@param  format                                                        */
/*    0 = Linear PCM, platform endian                                   */
/*    1 = ADPCM                                                         */
/*    2 = MP3                                                           */
/*    3 = Linear PCM, little endian                                     */
/*    4 = Nellymoser 16 kHz mono                                        */
/*    5 = Nellymoser 8 kHz mono                                         */
/*    6 = Nellymoser                                                    */
/*    7 = G.711 A-law logarithmic PCM                                   */
/*    8 = G.711 mu-law logarithmic PCM                                  */
/*    9 = reserved                                                      */
/*    10 = AAC                                                          */
/*    11 = Speex                                                        */
/*    14 = MP3 8 kHz                                                    */
/*    15 = Device-specific sound                                        */
/*    Formats 7, 8, 14, and 15 are reserved.                            */
/*    AAC is supported in Flash Player 9,0,115,0 and higher.            */
/*    Speex is supported in Flash Player 10 and higher.                 */
/*@param samplerate 0 = 5.5 kHz, 1 = 11 kHz, 2 = 22 kHz, 3 = 44 kHz     */
/*@param samplesize  0 = 8-bit samples   1 = 16-bit samples             */
/*@param sampletype  0 = Mono sound     1 = Stereo sound                */
/************************************************************************/
unsigned char  rtmp_make_audio_headerTag(unsigned int format, unsigned int samplerate, unsigned int samplesize, unsigned int sampletype);




# ifdef __cplusplus
}
# endif

#endif
