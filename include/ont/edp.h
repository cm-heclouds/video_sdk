#ifndef ONT_INCLUDE_ONT_EDP_H_
#define ONT_INCLUDE_ONT_EDP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "device.h"
/**
 * */
typedef void(*ont_edp_get_transdata_cb)(const char*svr_name,size_t svr_len,const char* data,size_t data_len);
/**
 * */
void ont_edp_set_transdata_cb(ont_device_t* device,ont_edp_get_transdata_cb cb);
/**
 * */
int ont_edp_send_transdata(ont_device_t* device,const char* svr_name,const char* data,size_t len);
#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* ONT_INCLUDE_ONT_EDP_H_ */
