#ifndef ONT_SRC_JTEXT_PACKET_H_
#define ONT_SRC_JTEXT_PACKET_H_

#include <ont/platform.h>


struct ont_jtext_t;

int ont_packet_pack_register_pkt(struct ont_jtext_t *ctx);
int ont_packet_pack_auth_pkt(struct ont_jtext_t *ctx);
int ont_packet_pack_heartbeat_pkt(struct ont_jtext_t *ctx);
int ont_packet_pack_uplinkdata_pkt(struct ont_jtext_t *ctx, char type,
                                   const char *data, size_t size);

int ont_packet_unpack(struct ont_jtext_t *ctx, const char *pkt, size_t size);

#define ont_packet_get_msg_id(pkt) (((uint16_t)(pkt[0]) << 8) | (uint16_t)pkt[1])
#define ont_packet_get_property(pkt) (((uint16_t)pkt[2] << 8) | (uint16_t)pkt[3])
#define ont_packet_is_subpkt(pkt) (ont_packet_get_property(pkt) & 0x2000)
#define ont_packet_get_body(pkt) (ont_packet_is_subpkt(pkt) ? pkt + 16 : pkt + 12)
#define ont_packet_get_body_len(pkt) (ont_packet_get_property(pkt) & 0x03ff)

int ont_packet_parse_msg_regresp(const char *body, uint16_t size,
                                 uint16_t *sn, char *result,
                                 const char **auth_code, uint16_t *auth_code_len);

int ont_packet_parse_msg_platresp(const char *body, uint16_t size,
                                  uint16_t *sn, uint16_t *msg_id, char *result);
#endif /* ONT_SRC_JTEXT_PACKET_H_ */
