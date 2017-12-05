
#ifndef _ONT_VIDEO_CMD_H_
#define _ONT_VIDEO_CMD_H_

# ifdef __cplusplus
extern "C" {
# endif


/**云台命令 */
typedef enum _t_ont_video_ptz_cmd
{
    C_ONT_VIDEO_ZOOM_IN = 1,        /**<焦距变大(倍率变大)?*/
    C_ONT_VIDEO_ZOOM_OUT = 2,       /**<焦距变小(倍率变小)?*/
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



/************************************************************************/
/*@brief 开始直播                                                      */
/*@param dev        设备指针                                           */
/*@param channel    通道号                                             */
/*@param push_url   推送地址                                           */
/*@param deviceid   设备ID                                             */
/************************************************************************/
typedef int(*t_ont_video_live_stream_start)(void *dev, int channel, const char *push_url, const char* deviceid);

/************************************************************************/
/*@brief 设置直播码流级别                                                  */
/*@param dev        设备指针                                           */
/*@param channel    通道号                                             */
/*@param stream     1 流畅，2 标清，3 高清， 4 超清                    */
/************************************************************************/
typedef int(*t_ont_video_live_stream_ctrl)(void *dev, int channel, int stream);

  
/*************************************************************************/
/*@brief 通知指定的通道生成关键帧                                       */
/*@param dev        设备指针                                           */
/*@param channel    通道号                                             */
/************************************************************************/
typedef int(*t_ont_video_stream_make_keyframe)(void *dev, int channel);



/*************************************************************************/
/*@brief 云台控制                                       */
/*@param dev        设备指针                                           */
/*@param channel    通道号                                             */
/*@param stop   0 开始控制，1 停止控制, 2 单步                         */
/*@param cmd    控制命令                                               */   
/*@param speed  speed 云台速度 [1-7], 1表示最低                        */   
typedef int(*t_ont_video_dev_ptz_ctrl)(void *dev, int channel, int mode, t_ont_video_ptz_cmd cmd, int speed);


/*************************************************************************/
/*@brief 远程文件查询                                                    */
/*@param dev        设备指针                                             */
/*@param channel    通道号                                               */
/*@param pageindex  页码, 从1开始                                        */
/*@param max        当页返回的最大数量                                   */   
typedef int(*t_ont_video_dev_query_files)(void *dev, int channel, int startindex, int max, t_ont_video_file **files, int *filenum, int *totalnum);


/************************************************************************/
/*@brief 开始一路视频回放                                              */
/*@param dev        设备指针                                           */
/*@param channel    通道号                                             */
/*@param fileinfo   指示一个开始时间和结束时间的文件信息                 */
/*@param playflag   文件的回放标示                                       */
/*@return 0         找到回放文件，创建回放连接                           */
/************************************************************************/
typedef int(*t_ont_video_vod_start_notify)(void *dev, t_ont_video_file *fileinfo, const char *playflag, const char*pushurl);


typedef struct _t_ont_dev_video_callbacks
{
    t_ont_video_live_stream_ctrl      stream_ctrl;
    t_ont_video_live_stream_start     live_start;
    t_ont_video_stream_make_keyframe  make_keyframe;
    t_ont_video_dev_ptz_ctrl          ptz_ctrl;
    t_ont_video_vod_start_notify      vod_start;
    t_ont_video_dev_query_files       query;
}t_ont_dev_video_callbacks;



int ont_videocmd_handle(void *dev, void*cmd, t_ont_dev_video_callbacks *cb);



# ifdef __cplusplus
}
# endif


#endif /*_ONT_VIDEO_CMD_H_*/

