#ifndef ONT_INCLUDE_DEVICE_H_
#define ONT_INCLUDE_DEVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "error.h"
#include "platform.h"

#define M_TIME_LEN    20  /*!<时间字符串长度 */

#define M_FILEDES_MAX 255 /*!<文件描述最大长度, UTF-8 编码 */


#define DEVICE_MSG_TIMEOUT 6000

/**
 * 设备状态，内部使用
 */
typedef enum _ont_device_status_t {
	ONTDEV_STATUS_STOPPED = 0,
	ONTDEV_STATUS_CONNECTED,
	ONTDEV_STATUS_AUTHENTICATED
} ont_device_status_t;

typedef struct _ont_device_callback_t ont_device_callback_t;
typedef struct _ont_cmd_callbacks_t   ont_cmd_callbacks_t;

typedef struct  _ont_device_t {
	uint64_t                device_id;     /**< 设备ID */
	uint64_t                product_id;    /**< 产品ID */
	char                    acc_ip[40];
	uint16_t                port;
	char                    auth_code[256]; /**<设备鉴权码 */
	char                    encrypt_flag;
	uint32_t                seq_rcv;
	uint32_t                seq_snd;
	ont_device_callback_t   *device_cbs;
	ont_cmd_callbacks_t     *ont_cmd_cbs;
	ont_socket_t            *fd;
	char                    *rcv_buf;
	uint32_t                 buf_length;
	uint32_t                 rcv_length;
	void                    *decrypt_ctx;
	void                    *encrypt_ctx;
	uint8_t                  status;      /**< 设备状态 */
} ont_device_t;


/**文件索引*/
typedef struct _t_ont_video_file {
	int32_t  channel;
	int32_t  size;
	char     begin_time[M_TIME_LEN];     /**<开始时间，例如2012-04-16 16:00:00*/
	char     end_time[M_TIME_LEN];       /**<结束时间*/
	char     descrtpion[M_FILEDES_MAX];  /**<文件描述*/
} ont_video_file_t;


/**
 * 平台下发的命令
 */
typedef struct _ont_device_cmd_t {
	char need_resp;   /**< 是否需要回复                   */
	char id[37];      /**< 命令ID，字符串类型，以NULL结尾 */
	char *req;        /**< 指向命令的起始位置 */
	size_t size;      /**< 命令的字节数 */
} ont_device_cmd_t;

/**
* 平台对设备请求推流的响应
*/
typedef struct _ont_plat_resp_dev_push_stream_t {
	char     result;        /**< 请求执行结果 */
	uint32_t chan_id;       /**< 通道ID */
	char     protype;       /**< 推流协议，0:RTMP,其他 */
	char     push_url[512]; /**< 推流地址 */
	uint16_t url_len;       /**< 推流地址长度 */
	uint16_t url_ttl_min;   /**< 链接有效期，单位分钟 */
} ont_plat_resp_dev_ps_t;

/************************************************************************/
/*@brief 设备保活回复处理                                               */
/*@param dev        设备指针                                            */
/************************************************************************/
typedef void (*t_ont_keepalive_resp_callback)(ont_device_t *dev);


/************************************************************************/
/*@brief 开始直播                                                      */
/*@param dev        设备指针                                           */
/*@param channel    通道号                                             */
/*@param push_url   推送地址                                           */
/*@protype          0                                                 */
/*@ttl_min          超时时间                                           */
/*@return           错误码                                             */
/************************************************************************/
typedef int32_t (*t_ont_live_stream_start)(ont_device_t *dev, int32_t channel, uint8_t protype, uint16_t ttl_min, const char *push_url);


/************************************************************************/
/*@brief 开始一路视频回放                                              */
/*@param dev        设备指针                                           */
/*@param channel    通道号                                             */
/*@protype          0                                                 */
/*@param fileinfo   指示一个开始时间和结束时间的文件信息                 */
/*@param playflag   文件的回放标识                                      */
/*@return 错误码                                                        */
/************************************************************************/
typedef int32_t (*t_ont_vod_start_notify)(ont_device_t *dev, int32_t channel, uint8_t protype, ont_video_file_t *fileinfo, const char *playflag, const char *pushurl, uint16_t ttl);


/*************************************************************************/
/*@brief 平台录像状态更新                                                  */
/*@param dev        设备指针                                             */
/*@param channel    通道号                                               */
/*@param status     1 平台开始录像；2 平台结束录像                          */
/*@param seconds    当status 为1时，表示录像时长                          */
/*@param url        断线续传链接                                         */

typedef int32_t(*t_ont_cmd_channel_record_update)(void *dev, int32_t channel, int32_t status, int32_t seconds, char url[255]);



/************************************************************************/
/*@brief 用户自定义命令下发                                            */
/*@param dev        设备指针                                           */
/*@param cmd        用户定义的命令                                      */
/*@return 0         命令执行成功                                        */
/*        -1        失败                                                */
/************************************************************************/
typedef int32_t (*t_user_defind_cmd)(ont_device_t *dev, ont_device_cmd_t *cmd);

/************************************************************************/
/*@brief API自定义消息下发处理                                          */
/*@param dev        设备指针                                            */
/*@param msg        API自定义消息                                       */
/*@param length     API自定义消息长度                                   */
/*@return 0         命令执行成功                                        */
/*        -1        失败                                                */
/************************************************************************/
typedef int32_t(*t_api_defind_msg)(ont_device_t *dev, char *msg, size_t msg_len);

/************************************************************************/
/*@brief 平台回复设备推流请求的信息                                          */
/*@param dev         设备指针                                            */
/*@param resp        平台回复设备请求推流的信息                                */
/*@return ONT_ERR_OK 命令执行成功                                        */
/*        其他       失败                                                */
/************************************************************************/
typedef int32_t(*t_plt_resp_dev_req_stream_msg)(ont_device_t *dev, ont_plat_resp_dev_ps_t *resp);

struct _ont_device_callback_t {
	t_ont_keepalive_resp_callback   keepalive_resp;
	t_ont_live_stream_start         live_start;
	t_ont_vod_start_notify          rvod_start;
	t_ont_cmd_channel_record_update record_status;
	t_user_defind_cmd               user_defined_cmd;
	t_api_defind_msg                api_defined_msg;
	t_plt_resp_dev_req_stream_msg   plt_resp_dev_req_stream_msg;
};


/**
 * 获取接入机
 * @param dev   设备上下文
 * @param token 接入机token, 与视频接入机建立连接后,需要调用消息验证这个token
 * @remark UDP 消息
 */
int ont_device_get_acc(ont_device_t *dev, char *token);


/**
 * 创建OneNET接入设备
 * @param dev 产品ID
 * @return 创建成功则返回ONT_ERR_OK
 * @remark 不使用OneNET接入设备实例后，调用@see ont_device_destroy销毁
 */
int ont_device_create(ont_device_t **dev, ont_device_callback_t *callback);

/**
* 创建OneNET接入设备
* @param dev   产品ID
* @param token 鉴权token
* @return 验证成功则返回ONT_ERR_OK
* @remark
*/
int ont_device_verify(ont_device_t *dev, const char *token);


/**
 * 与OneNET建立连接
 * @param dev OneNET接入设备实例
 * @return 成功建立连接则返回ONT_ERR_OK
 */
int ont_device_connect(ont_device_t *dev);

/**
 * 与OneNET建立连接
 * @param dev OneNET接入设备实例
 * @param productid  产品ID
 * @param reg_code 注册码
 * @param id        设备唯一标识
 * @return 成功建立连接则返回ONT_ERR_OK
 */
int ont_device_register(ont_device_t *dev,
                        uint64_t productid,
                        const char *reg_code,
                        const char *id);


/**
 * 设备鉴权
 * @param dev OneNET接入设备实例
 * @param auth_code  鉴权码, 通过设备注册获取, 或者平台手动添加的设备
 * @param auth_code_new  新的鉴权码
 * @return 成功建立连接则返回0
 */
int ont_device_auth(ont_device_t *dev, const char *auth_code, char *auth_code_new);


/**
 * 断开与OneNET的连接
 * @param dev OneNET接入设备实例
 */
void ont_device_disconnect(ont_device_t *dev);


/**
 * 销毁OneNET接入设备实例
 */
void ont_device_destroy(ont_device_t *dev);

/**
 * 保活设备
 */
int ont_device_keepalive(ont_device_t *dev);


/**
 * 设备请求RSA 公钥
 **
 */
int ont_device_request_rsa_publickey(ont_device_t *dev);

/**
 * 发送加密公钥
 **
 */
int ont_device_replay_rsa_publickey(ont_device_t *dev);

/**
 * 获取设备已经添加的通道
 **
 */
int ont_device_get_channels(ont_device_t *dev, uint32_t *channels, size_t *size);


/**
 * 删除设备通道
 **
 */
int ont_device_del_channel(ont_device_t *dev, uint32_t channel);


/**
 * 添加通道
 **
 */
int ont_device_add_channel(ont_device_t *dev, uint32_t channel, const char *title, uint32_t tsize, const char *desc, uint32_t dsize);


/**
 *检查是否有命令需要处理
 *
 **/
int ont_device_check_receive(ont_device_t *dev, uint32_t tm_ms);


/**
 * 发送命令响应结果
 * @param dev    OneNET接入设备实例
 * @param result 命令处理结果
 * @param cmdid  命令响应结果对应的命令ID
 * @param resp   命令响应内容
 * @param size   命令响应内容长度
 */
int ont_device_reply_ont_cmd(ont_device_t *dev, int32_t result, const char *cmdid,
                             const char *resp, size_t size);


/**
 * 发送命令响应结果
 * @param dev OneNET接入设备实例
 * @param result 命令处理结果
 * @param cmdid  命令响应结果对应的命令ID
 * @param resp   命令响应内容
 * @param size   命令响应内容长度
 */
int ont_device_reply_user_defined_cmd(ont_device_t *dev, int32_t result, const char *cmdid,
                                      const char *resp, size_t size);

/**
* 处理API下发的自定义消息
* @param dev     OneNET接入设备实例
* @param msg     api自定义消息
* @param msg_len api自定义消息长度
* @remark        dev参数暂时未使用，暂且保留，备扩展使用
*/
int ont_device_deal_api_defined_msg(ont_device_t *dev, char *msg, size_t msg_len);

/**
* 处理平台回复的设备推流请求
* @param dev     OneNET接入设备实例
* @param resp    平台回复消息
* @remark        dev参数暂时未使用，暂且保留，备扩展使用
*/
int ont_device_deal_plat_resp_push_stream_msg(ont_device_t *dev, ont_plat_resp_dev_ps_t *resp);

/**
* 设置平台命令处理函数
* @param dev OneNET接入设备实例
* @param cb  命令处理结果
* @return 返回设置结果
*/
int ont_device_set_platformcmd_handle(ont_device_t *dev, ont_cmd_callbacks_t *cb);

/**
* 设置平台命令处理函数
* @param dev OneNET接入设备实例
* @param dataid  上传的消息自定义ID
* @param data    自定义消息内容
* @param len     自定义消息长度
* @return 返回设置结果
*/
int ont_device_data_upload(ont_device_t *dev, uint64_t dataid, const char *data, size_t len);

/**
 * 同步平台的系统时间到设备
 * @param dev             OneNET接入设备实例
 * @param tm              返回的时间
 */
int ont_device_get_systime(ont_device_t *dev, struct ont_timeval *tm);


/**
* 设备连接保持
* @param dev             OneNET接入设备实例
*/
int ont_device_keep_connect(ont_device_t *dev);

/**
* 设备请求推流并调用回调函数处理平台回复
* @param dev             OneNET接入设备实例
* @param chan_id         通道id
* @param idle_sec        推流空闲时间,单位s
*/
int ont_device_req_push_stream(ont_device_t *dev, uint32_t chan_id, uint16_t idle_sec);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ONT_INCLUDE_DEVICE_H_ */
