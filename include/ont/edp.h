#ifndef ONT_INCLUDE_ONT_EDP_H_
#define ONT_INCLUDE_ONT_EDP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "device.h"

/*START CONN DEFINE*/
/**
 * 回调函数 连接响应
 * @param device       创建的edp设备的指针
 * @param result       连接结果
**/
#ifdef ONT_PROTOCOL_EDP_EXTRA
typedef void(*ont_edp_conn_resp_cb_ex)( ont_device_t *device, int result );
#endif

/**
 * 设置连接响应回调函数
**/
#ifdef ONT_PROTOCOL_EDP_EXTRA
void ont_edp_set_conn_resp_cb_ex( ont_device_t *device, ont_edp_conn_resp_cb_ex cb );
#endif

/*END CONN DEFINE*/

/*START TRANS_DATA DEFINE*/
/**
 * 回调函数 接收第三方平台透传数据
 * @param device       创建的edp设备的指针
 * @param svr_name     外部服务名称的起始地址
 * @param svr_name_len svr_name的长度
 * @param data         透传数据的起始地址
 * @param data_len     data的长度
 * */
typedef void(*ont_edp_get_transdata_cb)( const char *svr_name,
                                         size_t      svr_name_len,
                                         const char *data,
                                         size_t      data_len );
#ifdef ONT_PROTOCOL_EDP_EXTRA
typedef void(*ont_edp_get_transdata_cb_ex)( ont_device_t *device,
                                            const char   *svr_name,
                                            size_t        svr_name_len,
                                            const char   *data,
                                            size_t        data_len );
#endif

/**
 * 设置接收第三方平台透传数据的回调函数 
 * @param device       创建的edp设备的指针 
 * @param cb           回调函数指针  
 * */
void ont_edp_set_transdata_cb( ont_device_t* device,ont_edp_get_transdata_cb cb );
#ifdef ONT_PROTOCOL_EDP_EXTRA
void ont_edp_set_transdata_cb_ex( ont_device_t *device, ont_edp_get_transdata_cb_ex cb );
#endif

/**
 * 发送透传数据到第三方平台   
 * @param device       创建的edp设备的指针
 * @param svr_name     外部服务名称的起始地址
 * @param data         透传数据的起始地址
 * @param data_len     data的长度  
 * @return 成功则返回ONT_ERR_OK
 * */
int ont_edp_send_transdata(ont_device_t* device,const char* svr_name,const char* data,size_t len);
/*END TRANS_DATA DEFINE*/

/*START PUSH_DATA DEFINE*/
/**
* 回调函数 转发(透传)数据
* @param device           创建的edp设备的指针
* @param src_devid        源设备ID
* @param src_devid_len    源设备ID的长度
* @param data             数据
* @param data_len         数据长度
* */
typedef void(*ont_edp_get_pushdata_cb)( const char *src_devid,
                                        size_t      src_devid_len,
                                        const char *data,
                                        size_t      data_len );
#ifdef ONT_PROTOCOL_EDP_EXTRA
typedef void(*ont_edp_get_pushdata_cb_ex)( ont_device_t *device,
                                           const char   *src_devid,
                                           size_t        src_devid_len,
                                           const char   *data,
                                           size_t        data_len );
#endif

/**
* 设置转发(透传)数据的回调函数
* @param device          创建的edp设备的指针
* @param cb              回调函数指针
* */
void ont_edp_set_pushdata_cb( ont_device_t* device, ont_edp_get_pushdata_cb cb );
#ifdef ONT_PROTOCOL_EDP_EXTRA
void ont_edp_set_pushdata_cb_ex( ont_device_t *device, ont_edp_get_pushdata_cb_ex cb );
#endif

/**
* 发送转发(透传)数据
* @param device           创建的edp设备的指针
* @param dst_devid        目的设备ID
* @param data             数据
* @param data_len         数据长度
* @return 成功则返回ONT_ERR_OK
* */
int ont_edp_send_pushdata(ont_device_t* device, const char* dst_devid, const char* data, size_t len);
/*END PUSH_DATA DEFINE*/

/*START SAVE_DATA DEFINE */
/**
 * 回调函数 存储确认
 * @param device         创建的edp设备的指针
 * @param msg_id         消息ID
**/
#ifdef ONT_PROTOCOL_EDP_EXTRA
typedef void(*ont_edp_save_ack_cb_ex)( ont_device_t *device, int msg_id );
#endif

/**
 * 设置存储确认的回调函数
 * @param device         创建的edp设备的指针
 * @param cb             存储确认的回调函数
**/
#ifdef ONT_PROTOCOL_EDP_EXTRA
void ont_edp_set_save_ack_cb_ex( ont_device_t *device, ont_edp_save_ack_cb_ex cb );
#endif

/*END SAVE_DATA DEFINE */

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* ONT_INCLUDE_ONT_EDP_H_ */
