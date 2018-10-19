#ifndef ONT_INCLUDE_PLATFORM_CMD_H_
#define ONT_INCLUDE_PLATFORM_CMD_H_

# ifdef __cplusplus
extern "C" {
# endif


/**云台命令 */
typedef enum _t_ont_video_ptz_cmd {
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
} t_ont_video_ptz_cmd;


/************************************************************************/
/*@brief 设置直播码流级别,                                                */
/*@param dev        设备指针                                           */
/*@param channel    通道号                                             */
/*@param stream     1 流畅，2 标清，3 高清， 4 超清                    */
/************************************************************************/
typedef int32_t(*t_ont_cmd_live_stream_ctrl)(void *dev, int32_t channel, int32_t stream);



/*************************************************************************/
/*@brief 云台控制                                       */
/*@param dev        设备指针                                           */
/*@param channel    通道号                                             */
/*@param stop   0 开始控制，1 停止控制, 2 单步                         */
/*@param cmd    控制命令                                               */
/*@param speed  speed 云台速度 [1-7], 1表示最低                        */
typedef int32_t(*t_ont_cmd_dev_ptz_ctrl)(void *dev, int32_t channel, int32_t mode, t_ont_video_ptz_cmd ptz, int32_t speed);


/*************************************************************************/
/*@brief 远程文件查询                                                    */
/*@param dev        设备指针                                             */
/*@param channel    通道号                                               */
/*@param pageindex  页码, 从1开始                                        */
/*@param max        当也返回的最大数量                                   */
typedef int32_t(*t_ont_cmd_dev_query_files)(void *dev, int32_t channel, int32_t startindex, int32_t max, const char *starTime, const char *endTime, ont_video_file_t **files, int32_t *filenum, int32_t *totalnum);




struct _ont_cmd_callbacks_t {
	t_ont_cmd_live_stream_ctrl      stream_ctrl;
	t_ont_cmd_dev_ptz_ctrl          ptz_ctrl;
	t_ont_cmd_dev_query_files       query;
};


int32_t ont_videocmd_handle(void *dev, ont_device_cmd_t *cmd);



# ifdef __cplusplus
}
# endif


#endif /*ONT_INCLUDE_PLATFORM_CMD_H_*/

