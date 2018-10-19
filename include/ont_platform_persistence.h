#ifndef _ONT_PLATFORM_PERSISTENCE_H_
#define _ONT_PLATFORM_PERSISTENCE_H_

# ifdef __cplusplus
extern "C" {
# endif


/**
 * 持久化保存设备状态数据
 * @param status 指向设备状态数据块
 * @param size 设备状态数据块字节数
 * @return 保存成功返回ONT_ERR_OK
 * @remark 存储设备状态的过程中，要防止存储的数据被写坏
 */
int ont_platform_save_device_status(const char *status, size_t size);
/**
 * 加载设备状态数据
 * @param status 指向用于保存设备状态数据的内存
 * @param size 加载的设备状态的字节数
 * @return 加载成功返回ONT_ERR_OK
 */
int ont_platform_load_device_status(char *status, size_t size);



#ifdef __cplusplus
} /* extern "C" */
#endif



#endif /* ONT_PLATFORM_PERSISTENCE_SUPPORT */



