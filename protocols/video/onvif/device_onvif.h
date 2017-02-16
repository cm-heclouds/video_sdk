#ifndef _DEVICE_ONVIF_H_
#define _DEVICE_ONVIF_H_

#include "ont/video.h"

# ifdef __cplusplus
extern "C" {
# endif

#define ONVIF_UUID_LEN   64
#define ONVF_DEVICE_PROFILES 4

typedef struct
{
    int level; /*1, 2, 3, 4*/
	int width;
	int height;
	unsigned bitrate;
	unsigned framterate;
    char strprofile[64];
    /*rtsp url*/
    char strPlayurl[256];
}device_profile_t;

typedef struct
{
    char strUrl[256];
    char strUser[64];
    char strPass[64];
    unsigned hasGetCap:1;

    /* If the Device support blow service */
    unsigned hasMedia:1;
    unsigned hasPTZ:1;
    unsigned hasImaging : 1;
    unsigned hasReceiver : 1;
    unsigned hasRecording : 1;
    unsigned hasSearch : 1;
    unsigned hasReplay : 1;
    unsigned hasEvent : 1;

    /* The Url of blow service */
    char strMedia[256];
    char strPTZ[256];
    char strImaging[256];
    char strReceiver[256];
    char strRecording[256];
    char strSearch[256];
    char strReplay[256];
    char strEvent[256];
	
	char   strPtzToken[64];
    unsigned hasCoutinusPanTilt : 1;
    unsigned hasCoutinusZoom : 1;
    double ptXrangeMin;
    double ptXrangeMax;
    double ptYrangeMin;
    double ptYrangeMax;

    long ptzTimeoutMin;
    long ptzTimeoutMax;

    double zoomRangeMin;
    double zoomRangeMax;

    int qualityRangeMin;
    int qualityRangeMax;
    
    device_profile_t profiles[ONVF_DEVICE_PROFILES];


}device_onvif_t;


typedef struct
{
    device_onvif_t *list;
    int number;
}device_onvif_list_t;

int ont_onvifdevice_adddevice(const char *url, const char*user, const char *passwd);

int ont_onvifdevice_ptz(int index, int cmd, int speed, int status);

device_onvif_t* ont_getonvifdevice(int index);

char* ont_geturl_level(device_onvif_t *devicePtr, int level, RTMPMetadata *meta);

int ont_onvif_device_discovery();

# ifdef __cplusplus
}
# endif


#endif
