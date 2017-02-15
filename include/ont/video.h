#ifndef _ONT_VIDEO_H_
#define _ONT_VIDEO_H_
#include "config.h"


#ifdef ONT_PROTOCOL_VIDEO

# ifdef __cplusplus
extern "C" {
# endif


#define M_ONT_VIDEO_TIME_LEN 20  /*!<时间字符串长度 */

#define M_ONT_VIDEO_FILEDES_MAX 255 /*!<文件描述最大长度, UTF-8 编码 */


    /**文件上传索引描述 */
    typedef struct _ont_video_file
    {
        unsigned char begin_time[M_ONT_VIDEO_TIME_LEN];     /**<开始时间，例如2012-04-16 16:00:00*/
        unsigned char end_time[M_ONT_VIDEO_TIME_LEN];       /**<结束时间*/
        unsigned char descrtpion[M_ONT_VIDEO_FILEDES_MAX]; /**<文件描述*/
    }t_ont_video_file;

    /**云台命令 */
    typedef enum _t_ont_video_ptz_cmd
    {
        C_ONT_VIDEO_ZOOM_IN = 1,        /**<焦距变大(倍率变大) */
        C_ONT_VIDEO_ZOOM_OUT = 2,       /**<焦距变小(倍率变小) */
        C_ONT_VIDEO_FOCUS_IN = 3,       /**<焦点前调*/
        C_ONT_VIDEO_FOCUS_OUT = 4,      /**<焦点后调*/
        C_ONT_VIDEO_IRIS_ENLARGE = 5,   /**<光圈扩大*/
        C_ONT_VIDEO_IRIS_SHRINK = 6,    /**<光圈缩小*/
        C_ONT_VIDEO_TILT_UP = 11,       /**<云台向上*/
        C_ONT_VIDEO_TILT_DOWN = 12,     /**<云台向下*/
        C_ONT_VIDEO_PAN_LEFT = 13,      /**<云台左转*/
        C_ONT_VIDEO_PAN_RIGHT = 14,     /**<云台右转*/
        C_ONT_VIDEO_PAN_AUTO = 22       /**<云台以SS的速度左右自动扫描*/
    }t_ont_video_ptz_cmd;

    /**RTMP medtadata 描述*/
    typedef struct _RTMPMetadata
    {
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
    /*@brief 开始直播                                                      */
    /*@param dev        设备指针                                           */
    /*@param channel    通道号                                             */
    /*@param push_url   推送地址                                           */
    /*@param deviceid   设备ID                                             */
    /************************************************************************/
    typedef void(*t_ont_video_live_stream_start)(void *dev, int channel, const char *push_url, const char* deviceid);

    /************************************************************************/
    /*@brief 设置直播码流级别                                                  */
    /*@param dev        设备指针                                           */
    /*@param channel    通道号                                             */
    /*@param stream     1 流畅，2 标清，3 高清， 4 超清                    */
    /************************************************************************/
	typedef void(*t_ont_video_live_stream_ctrl)(void *dev, int channel, int stream);

    
    /************************************************************************/
    /*@brief 开始一路视频回放                                              */
    /*@param dev        设备指针                                           */
    /*@param channel    通道号                                             */
    /*@param fileinfo   指示一个开始时间和结束时间的文件信息                 */
    /*@param playflag   文件的回放标示                                       */
    /*@param push_url   推送地址                                           */
    /*@param deviceid   设备ID                                             */
    /************************************************************************/
	typedef void(*t_ont_video_vod_stream_start)(void *dev, int channel, t_ont_video_file *fileinfo, const char *playflag, const char *push_url, const char *deviceid);
    
    /*************************************************************************/
    /*@brief 通知指定的通道生成关键帧                                       */
    /*@param dev        设备指针                                           */
    /*@param channel    通道号                                             */
    /************************************************************************/
	typedef void(*t_ont_video_stream_make_keyframe)(void *dev, int channel);



    /*************************************************************************/
    /*@brief 云台控制                                       */
    /*@param dev        设备指针                                           */
    /*@param channel    通道号                                             */
    /*@param stop   0 开始控制，1 停止控制, 2 单步                         */
    /*@param cmd    控制命令                                               */   
    /*@param speed  speed 云台速度 [1-7], 1表示最低                        */   
	typedef void(*t_ont_video_dev_ptz_ctrl)(void *dev, int channel, int mode, t_ont_video_ptz_cmd cmd, int speed);


    typedef struct _t_ont_video_dev_ctrl_callbacks
    {
		t_ont_video_live_stream_ctrl  stream_ctrl;
		t_ont_video_live_stream_start live_start;
		t_ont_video_stream_make_keyframe make_keyframe;
		t_ont_video_vod_stream_start     vod_start;
		t_ont_video_dev_ptz_ctrl      ptz_ctrl;
    }t_ont_video_dev_ctrl_callbacks;


    /*************************************************************************/
    /*@brief 上报通道，创建对应通道的数据流                                   */
    /*@param dev        设备指针                                           */
    /*@param channels   通道数                                             */
    int ont_video_dev_set_channels(void *dev, int channels); 

    
    /*************************************************************************/
    /*@brief 视频文件上传，创建数据点                                           */
    /*@brief channel 通道号                                                   */   
    /*@param list      文件列表                                               */
    /*@param n     文件数量                                                    */
    int ont_video_dev_fileinfo_upload(void *dev, int channel, t_ont_video_file *list, int n);

    /************************************************************************/
    /*@brief 创建一个连接服务器得RTMP通道                                    */
    /*@param pushurl 推送目标地址                                           */
    /*@param sock_timeout_seconds socket 超时时间                          */
    /************************************************************************/
    void* rtmp_create_publishstream(const char *pushurl, unsigned int sock_timeout_seconds, const char*deviceid);

	/************************************************************************/
	/*@brief 发送MetaData                                             */
	/*@param RTMP rtmp指针                                            */
	/*@param metadata  媒体数据描述                                          */
	/************************************************************************/
    int rtmp_send_metadata(void* rtmp, RTMPMetadata *metadata);



	/************************************************************************/
	/*@brief 发送H264的sps，pps 数据                                   */
	/*@param sps sps地址                                           */
	/*@param sps_len  sps 长度                                          */
	/*@param pps pps地址                                           */
	/*@param pps_len  pps 长度                                          */
	/*@param ts  时间戳                                          */
	/************************************************************************/
    int rtmp_send_spspps(void *rtmp, unsigned char * sps, int sps_len, unsigned char *pps, int pps_len, int ts);


	/************************************************************************/
	/*@brief 发送视频帧                                                   */
    /*@param rtmp, rtmp 指针                                               */
    /*@param data, 视频数据采用FLV Tag data格式封装                         */
    /*@param size, 视频数据大小                                             */
    /*@param ts, 视频时间戳                                                 */
    /*@param keyFrame, 是否关键帧                                          */
	/************************************************************************/
    int rtmp_send_videodata(void* rtmp, unsigned char *data, unsigned int size, unsigned int ts, unsigned int keyFrame);



    /************************************************************************/
    /*@brief RTMP 发送RTMP声音数据接收处理                                   */
    /*@param rtmp rtmp指针                                                  */
    /*@param headertag  音频数据格式                                        */
    /*@param data 音频数据                                                  */
    /*@param size 音频大小                                                  */
    /*@param ts 音频时间戳                                                  */
    /************************************************************************/
    int rtmp_send_audiodata(void* rtmp, unsigned char headertag, unsigned char *data, unsigned int size, unsigned int ts, unsigned int headertype);



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


#endif /*ONT_PROTOCOL_VIDEO*/
#endif
