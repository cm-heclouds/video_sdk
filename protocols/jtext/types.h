#ifndef ONT_SRC_JTEXT_TYPES_H_
#define ONT_SRC_JTEXT_TYPES_H_

#include <ont/device.h>
#include <ont_channel.h>


#define ONT_JTEXT_MSG_REGISTER    0x0100
#define ONT_JTEXT_MSG_AUTH        0x0102
#define ONT_JTEXT_MSG_HEARTBEAT   0x0002
#define ONT_JTEXT_MSG_UPLINK      0x0900
#define ONT_JTEXT_MSG_REG_RESP    0x8100
#define ONT_JTEXT_MSG_PLAT_RESP   0x8001

#define ONT_JTEXT_MAX_PKT_SIZE       2080
#define ONT_JTEXT_MAX_PKT_BODY_SIZE  1023


#define ONT_JTEXT_STATUS_IDLE       0x100
#define ONT_JTEXT_STATUS_SENDING    0x101


typedef struct ont_extent_t {
    size_t size;
    char *base;
} ont_extent_t;

typedef struct ont_packet_t {
    struct ont_packet_t *next;
    size_t size;
    char *pkt;
} ont_packet_t;

typedef struct ont_jtext_t {
    ont_device_t dev;

    char *auth_code;
    char terminal_type[20];
    char terminal_id[7];
    ont_channel_interface_t channel[1];
    ont_packet_t packets[1];
    ont_packet_t *last_pkt;

    char phone_number[6];
    uint16_t serial_number;

    char recv_pkt[1023 + 16 + 1]; /* header(16 bytes) + body(1023 bytes) + crc(1 bytes) */
    int recv_pkt_len;

    int status;
    int32_t next_ping_time;
} ont_jtext_t;

#endif /* ONT_SRC_JTEXT_TYPES_H_ */
