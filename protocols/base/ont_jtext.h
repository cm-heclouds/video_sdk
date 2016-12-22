#ifndef ONT_SRC_COMM_ONT_JTEXT_H_
#define ONT_SRC_COMM_ONT_JTEXT_H_

#include <ont/device.h>



int ont_jtext_create(struct ont_device_t **dev);
void ont_jtext_destroy(struct ont_device_t *dev);
int ont_jtext_connect(struct ont_device_t *dev, const char *auth_info);
int ont_jtext_send_datapoints(struct ont_device_t *dev, const char *payload, size_t bytes);
struct ont_device_cmd_t *ont_jtext_get_cmd(struct ont_device_t *dev);
int ont_jtext_reply_cmd(struct ont_device_t *dev, const char *cmdid, const char *resp, size_t bytes);
int ont_jtext_keepalive(struct ont_device_t *dev);

#endif /* ONT_SRC_COMM_ONT_JTEXT_H_ */
