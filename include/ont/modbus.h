#ifndef ONT_INCLUDE_ONT_MODBUS_H_
#define ONT_INCLUDE_ONT_MODBUS_H_

#include "ont/error.h"
#include "ont/device.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ONT_PROTOCOL_MODBUS

/**
 * 创建设备数据流
 * @param dev      OneNET接入设备实例
 * @param id       数据流ID
 * @param cmd      数据流命令，16进制字节字符串
 * @param interval 数据流命令定时下发的时间间隔（秒）
 * @param formula  数据流结构计算公式，例如"(A0+A1)*A2"
**/
int ont_modbus_create_ds( ont_device_t *dev,
                          const char *server,
                          uint16_t port,
                          const char *id,
                          const char *cmd,
                          int interval,
                          const char *formula );

#endif /* ONT_PROTOCOL_MODBUS */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ONT_INCLUDE_ONT_MODBUS_H_ */