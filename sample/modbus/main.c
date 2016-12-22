#include "ont/modbus.h"

int main( int argc, char *argv[] ) {
    int rc = 0;
    ont_device_t *dev = NULL;
    ont_device_cmd_t *cmd = NULL;

    unsigned char reply_1[] = { 0x01, 0x03, 0x04, 0x00, 0x01, 0x00, 0x02, 0x32, 0x2a };
    unsigned char reply_2[] = { 0x02, 0x03, 0x04, 0x00, 0x03, 0x00, 0x04, 0xf0, 0x38 };
    unsigned char reply_3[] = { 0x03, 0x03, 0x04, 0x00, 0x05, 0x00, 0x06, 0xf0, 0x49 };
    unsigned char reply_4[] = { 0x04, 0x01, 0x05, 0x71, 0x72, 0x73, 0x74, 0x75, 0x21, 0x22 };
    unsigned char reply_5[] = { 0x05, 0x01, 0x01, 0x61, 0x91, 0x50 };
    unsigned char reply_6[] = { 0x06, 0x03, 0x04, 0x00, 0x03, 0x00, 0x04, 0x30, 0x7d };
    unsigned char reply_7[] = { 0x07, 0x03, 0x04, 0x00, 0x03, 0x00, 0x04, 0xf0, 0x6d };
    unsigned char reply_8[] = { 0x08, 0x02, 0x05, 0x71, 0x72, 0x73, 0x74, 0x75, 0x77, 0x12 };
    unsigned char reply_9[] = { 0x09, 0x04, 0x04, 0x00, 0x03, 0x00, 0x04, 0x87, 0x83 };
    unsigned char *replays[] = { reply_1, reply_2, reply_3, reply_4, reply_5, reply_6, reply_7, reply_8, reply_9 };
    unsigned int sizes[] = { sizeof(reply_1), sizeof(reply_2), sizeof(reply_3), sizeof(reply_4), sizeof(reply_5), sizeof(reply_6), sizeof(reply_7), sizeof(reply_8), sizeof(reply_9) };

    rc = ont_device_create( 59261, /* product_id，测试时替换成自己的产品ID */
                            ONTDEV_MODBUS,
                            "test_modbus_sdk", /*device name,测试是替换成自己的设备名称*/
                            &dev );
    if ( rc ) {
        return -1;
    }

    rc = ont_device_connect( dev,
                             ONT_SERVER_ADDRESS,
                             ONT_SERVER_PORT,
                             "4q0vXEp99yeGjfEc", /* 设备注册码，测试时替换成自己产品的设备注册码*/
                             "{\"18888888888\":\"12345678\"}", /* {"phone":"svrpwd"}，测试时替换成自己的phone和svrpwd */
                             200 );
    if ( rc ) {
        return -1;
    }

    ont_modbus_create_ds( dev,
                          ONT_SERVER_ADDRESS,
                          ONT_SERVER_PORT,
                          "test_ds1",
                          "01030001000295CB",
                          20,
                          NULL );
    ont_modbus_create_ds( dev,
                          ONT_SERVER_ADDRESS,
                          ONT_SERVER_PORT,
                          "test_ds2",
                          "02030001000295F8",
                          40,
                          "(A0+A1)*A0-A1/A0" );
    ont_modbus_create_ds( dev,
                          ONT_SERVER_ADDRESS,
                          ONT_SERVER_PORT,
                          "test_ds4",
                          "0401009600251DA8",
                          60,
                          NULL );
    ont_modbus_create_ds( dev,
                          ONT_SERVER_ADDRESS,
                          ONT_SERVER_PORT,
                          "test_ds5",
                          "050100130008CD8D",
                          80,
                          NULL );
    ont_modbus_create_ds( dev,
                          ONT_SERVER_ADDRESS,
                          ONT_SERVER_PORT,
                          "test_ds8",
                          "0802009600255964",
                          100,
                          NULL );
    ont_modbus_create_ds( dev,
                          ONT_SERVER_ADDRESS,
                          ONT_SERVER_PORT,
                          "test_ds9",
                          "0904000100022143",
                          120,
                          NULL );

    while ( 1 ) {
        ont_device_cmd_t *cmd = ont_device_get_cmd( dev );
        if ( NULL != cmd ) {
            if ( cmd->size > 2 ) {
                if ( cmd->req[0] >= 1 && cmd->req[0] <= 9 ) {
                    ont_device_reply_cmd( dev,
                                          cmd->id,
                                          (const char*)replays[cmd->req[0] - 1],
                                          sizes[cmd->req[0] - 1] );
                }
            }

            ont_device_cmd_destroy( cmd );
        }

        ont_platform_sleep( 100 );
    }

    return 0;
}