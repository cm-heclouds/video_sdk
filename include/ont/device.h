#ifndef ONT_INCLUDE_ONT_DEVICE_H_
#define ONT_INCLUDE_ONT_DEVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "platform.h"
#include "error.h"

struct ont_pkt_formatter_t;

/**
 * 设备类型，内部使用
 */
typedef enum ont_device_type_t {
    ONTDEV_EDP       = 1,      /**< EDP类型设备 */
    ONTDEV_JTEXT     = 3,      /**< JT/T808类型设备 */
    ONTDEV_MODBUS    = 6,      /**< MODBUS类型设备 */
    ONTDEV_MQTT      = 7       /**< MQTT类型设备 */
} ont_device_type_t;

/**
 * 设备状态，内部使用
 */
typedef enum ont_device_status_t {
    ONTDEV_STATUS_STOPPED,
    ONTDEV_STATUS_REGISTERING,
    ONTDEV_STATUS_RETRIEVING_ACCEPTOR,
    ONTDEV_STATUS_RETRIEVED_ACCEPTOR,
    ONTDEV_STATUS_CONNECTED,
    ONTDEV_STATUS_AUTHENTICATED
} ont_device_status_t;

/**
 * OneNET 接入设备类型
 */
typedef struct ont_device_t {
    int type;            /**< 设备类型 */
    int status;          /**< 设备状态 */
    uint32_t device_id;  /**< 设备ID */
    uint32_t product_id; /**< 产品ID */
    char *key;           /**< 设备鉴权信息 */
    char ip[16];         /**< 接入机IP， ipv4: xxx.xxx.xxx.xxx */
    uint16_t port;       /**< 接入机端口 */
    uint16_t keepalive;  /**< 设备保活时间，单位:秒 */

    volatile int exit;
    struct ont_pkt_formatter_t *formatter;
    char name[33];
} ont_device_t;

/**
 * 平台下发的命令
 */
typedef struct ont_device_cmd_t {
    char id[37];      /**< 命令ID，字符串类型，以NULL结尾 */
    char *req;        /**< 指向命令的起始位置 */
    size_t size;      /**< 命令的字节数 */
} ont_device_cmd_t;

/**
 * 内部使用
 */
typedef struct ont_device_rt_t {
    int (*create)(ont_device_t **dev);
    void (*destroy)(ont_device_t *dev);
    int (*connect)(ont_device_t *dev,const char *auth_info);
    int (*send_datapoints)(ont_device_t *dev, const char *payload, size_t bytes);
    ont_device_cmd_t *(*get_cmd)(ont_device_t *dev);
    int (*reply_cmd)(ont_device_t *dev, const char *cmdid, const char *resp, size_t size);
    int (*keepalive)(ont_device_t *dev);
} ont_device_rt_t;

/**
 * 创建OneNET接入设备
 * @param product_id 产品ID
 * @param type 设备类型 @see ont_device_type_t
 * @param name 设备名称，最长不超过32字节，可以为NULL
 * @param dev [OUT] 保存创建后的OneNET接入设备实例
 * @return 创建成功则返回ONT_ERR_OK
 * @remark 不使用OneNET接入设备实例后，调用@see ont_device_destroy销毁
 */
int ont_device_create(uint32_t product_id,
                      ont_device_type_t type,
                      const char *name,
                      ont_device_t **dev);
/**
 * 与OneNET建立连接
 * @param dev OneNET接入设备实例
 * @param ip 服务器IP地址
 * @param port 服务器端口
 * @param reg_code 注册码
 * @param auth_info 鉴权信息，每次调用该函数时，同一设备的鉴权信息应该相同
 * @param keepalive 设备保活时间，单位：秒
 * @return 成功建立连接则返回ONT_ERR_OK
 */
int ont_device_connect(ont_device_t *dev,
                       const char *ip,
                       uint16_t port,
                       const char *reg_code,
                       const char *auth_info,
                       uint16_t keepalive);
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
 * 添加整数类型数据点
 * @param dev OneNET接入设备实例
 * @param datastream 数据流名称
 * @param value 数据值
 * @return 成功则返回ONT_ERR_OK
 */
int ont_device_add_dp_int(ont_device_t *dev, const char *datastream, int value);
/**
 * 添加字符串类型数据点
 * @param dev OneNET接入设备实例
 * @param datastream 数据流名称
 * @param value 数据值
 * @return 成功则返回ONT_ERR_OK
 */
int ont_device_add_dp_string(ont_device_t *dev, const char *datastream, const char *value);
/**
 * 添加二进制类型数据点
 * @param dev OneNET接入设备实例
 * @param datastream 数据流名称
 * @param value 数据值
 * @param size 二进制数据的字节数
 * @return 成功则返回ONT_ERR_OK
 */
int ont_device_add_dp_binary(ont_device_t *dev, const char *datastream,
                             const char *value, size_t size);

#ifdef ONT_PLATFORM_FLOATING_POINT_SUPPORT
/**
 * 添加浮点类型数据点
 * @param dev OneNET接入设备实例
 * @param datastream 数据流名称
 * @param value 数据值
 * @return 成功则返回ONT_ERR_OK
 */
int ont_device_add_dp_double(ont_device_t *dev, const char *datastream, double value);
#endif /* ONT_PLATFORM_FLOATING_POINT_SUPPORT */

/**
 * 添加JSON类型数据点
 * @param dev OneNET接入设备实例
 * @param datastream 数据流名称
 * @param value 数据值，JSON字符串，如 位置信息：{"lat":29.35,"lon":106.33}
 * @return 成功则返回ONT_ERR_OK
 */
int ont_device_add_dp_object(ont_device_t *dev, const char *datastream, const char *value);
/**
 * 发送已经添加的所有数据点
 * @param dev OneNET接入设备实例
 * @return 成功则返回ONT_ERR_OK
 */
int ont_device_send_dp(ont_device_t *dev);
/**
 * 清理已经添加的所有数据点
 * @param dev OneNET接入设备实例
 */
void ont_device_clear_dp(ont_device_t *dev);
/**
 * 获取已经接收到的OneNET下发的命令
 * @param dev OneNET接入设备实例
 * @return 如果有接收到的命令，返回命令实例，处理完后通过
 *         @see ont_device_cmd_destroy销毁；如果没有接收到的命令返回NULL
 */
ont_device_cmd_t *ont_device_get_cmd(ont_device_t *dev);
/**
 * 发送命令响应结果
 * @param dev OneNET接入设备实例
 * @param cmdid 命令响应结果对应的命令ID
 * @param resp 命令响应结果
 * @param size 命令响应结果的字节数
 */
int ont_device_reply_cmd(ont_device_t *dev, const char *cmdid,
                         const char *resp, size_t size);
/**
 * 销毁ont_device_cmd_t类型的实例
 */
void ont_device_cmd_destroy(ont_device_cmd_t *cmd);

/**
 * 内部使用
 */
const ont_device_rt_t *ont_device_get_rt(ont_device_type_t type);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ONT_INCLUDE_ONT_DEVICE_H_ */
