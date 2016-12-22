#ifndef ONT_SRC_COMM_ONT_EDP_H_
#define ONT_SRC_COMM_ONT_EDP_H_

#include "ont/platform.h"
#include "ont/device.h"
#include "ont/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
* @brief        设备登录
* @param device 设备基本信息及服务器信息 [inout]
* @return       成功则返回ONT_ERR_OK，并设置ont_edp_device_t的socket句柄

int32_t     ont_edp_encrypt();*/

/**
* @brief        创建设备
* @param name   设备名称 [in]
* @param desc   设备描述 [in]
* @param device 设备基本信息及服务器信息 [out]
* @return       成功则返回ONT_ERR_OK，并设置ont_edp_device_t的socket句柄
*/
int     ont_edp_create(ont_device_t **dev);

void    ont_edp_destroy(ont_device_t *dev);

/**
* @brief        设备登录
* @param device 设备基本信息及服务器信息 [inout]
* @return       成功则返回ONT_ERR_OK，并设置ont_edp_device_t的socket句柄
*/
int     ont_edp_connect(ont_device_t *device, const char *auth_info);

/**
* @brief        发送心跳包
* @param device 设备基本信息及连接信息 [in]
* @return       成功则返回ONT_ERR_OK
*/
int     ont_edp_keepalive(ont_device_t *device);
/**
* @brief        接收命令
* @param device 设备基本信息及连接信息 [in]
* @param cmd    接收到的命令 [out]
* @return       成功则返回ONT_ERR_OK
*/
ont_device_cmd_t* ont_edp_get_cmd(ont_device_t *dev);
/**
* @brief            命令响应
* @param device     设备基本信息及连接信息 [in]
* @param cmd_id     响应的命令ID [in]
* @param resp_data  响应的内容 [in]
* @param data_len   响应的内容长度 [in]
* @return           成功则返回ONT_ERR_OK
*/
int     ont_edp_reply_cmd(ont_device_t *device,
                                const char* cmd_id, 
                                const char* resp_data, 
                                size_t  data_len);
/**
* @brief            上传数据点
* @param device     设备基本信息及连接信息 [in]
* @param data       打包后的数据类型8消息体部分 [in]
* @param data_len   data的长度 [in]
* @return           成功则返回ONT_ERR_OK
*/
int     ont_edp_send_datapoints(ont_device_t *device,
                            const char *data,
                            size_t data_len);

#ifdef __cplusplus
} /* end of extern "C" */
#endif


#endif
