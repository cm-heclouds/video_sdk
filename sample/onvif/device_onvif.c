#include <stdint.h>
#include <log.h>
#include "namespace.h"
#include "soapH.h"
#include "device_onvif.h"
#include "wsseapi.h"
#include "getifaddrs.h"


#define DEBUG 1

#define M_ONVIFDEVICE_NUMBERS 30
#define M_ONVIFDEVICE_DISCOVERY_NUMS 30

int  ont_onvifdevice_capablity(const char *url, device_onvif_t *devicePtr);
int  ont_onvifdevice_getplayurl(device_onvif_t *devicePtr, int profileindex);
int  ont_onvifdevice_getprofile(device_onvif_t *devicePtr);
int  ont_onvifdevice_getconfigurations(device_onvif_t *devicePtr);


int  ont_onvifdevice_getconfigurations(device_onvif_t *devicePtr)
{
    struct _tptz__GetConfigurationOptions tptz__GetConfigurationOptions = { 0 };
    struct _tptz__GetConfigurationOptionsResponse tptz__GetConfigurationOptionsResponse = { 0 };
    struct tt__Space2DDescription *descrpion;
    static struct soap *soap;
    struct SOAP_ENV__Header header = { 0 };
    int result = 0;

    ///tptz__GetConfigurationOptions.ConfigurationToken = "PTZToken";
	tptz__GetConfigurationOptions.ConfigurationToken = devicePtr->strPtzToken;
    soap = soap_new();

    soap_set_namespaces(soap, namespaces);
    soap_default_SOAP_ENV__Header(soap, &header);
    soap_wsse_add_Security(soap);
    soap_wsse_add_UsernameTokenDigest(soap, "Id", (const char*)devicePtr->strUser, (const char*)devicePtr->strPass);
    devicePtr->ptXrangeMin = -1.0;
    devicePtr->ptXrangeMax = 1.0;
    devicePtr->ptYrangeMin = -1.0;
    devicePtr->ptYrangeMax = 1.0;
    devicePtr->ptzTimeoutMin = 1000;
    devicePtr->ptzTimeoutMax = 300000;
    devicePtr->zoomRangeMin = -1.0;
    devicePtr->zoomRangeMax = 1.0;

    do
    {
		if (devicePtr->hasPTZ)
        {
            result = soap_call___tptz__GetConfigurationOptions(soap, devicePtr->strPTZ, NULL, &tptz__GetConfigurationOptions, &tptz__GetConfigurationOptionsResponse);
            if (result != SOAP_OK)
            {
                printf("%s:%d,soap error: %d, %s, %s\n", __FILE__, __LINE__, soap->error, *soap_faultcode(soap), *soap_faultstring(soap));
                break;
            }
            if (tptz__GetConfigurationOptionsResponse.PTZConfigurationOptions && tptz__GetConfigurationOptionsResponse.PTZConfigurationOptions->Spaces && tptz__GetConfigurationOptionsResponse.PTZConfigurationOptions->Spaces->ContinuousPanTiltVelocitySpace)
            {
                descrpion = tptz__GetConfigurationOptionsResponse.PTZConfigurationOptions->Spaces->ContinuousPanTiltVelocitySpace;
            }

            if (descrpion)
            {
                devicePtr->ptXrangeMin = descrpion->XRange->Min;
                devicePtr->ptXrangeMax = descrpion->XRange->Max;
                devicePtr->ptYrangeMin = descrpion->YRange->Min;
                devicePtr->ptYrangeMax = descrpion->YRange->Max;
                if (tptz__GetConfigurationOptionsResponse.PTZConfigurationOptions->PTZTimeout)
                {
                    devicePtr->ptzTimeoutMin = tptz__GetConfigurationOptionsResponse.PTZConfigurationOptions->PTZTimeout->Min;
                    devicePtr->ptzTimeoutMax = tptz__GetConfigurationOptionsResponse.PTZConfigurationOptions->PTZTimeout->Max;
                }
                devicePtr->zoomRangeMin = tptz__GetConfigurationOptionsResponse.PTZConfigurationOptions->Spaces->ContinuousZoomVelocitySpace->XRange->Min;
                devicePtr->zoomRangeMax = tptz__GetConfigurationOptionsResponse.PTZConfigurationOptions->Spaces->ContinuousZoomVelocitySpace->XRange->Max;

            }
        }
    } while (0);

    soap_wsse_delete_Security(soap);
    soap_wsse_verify_done(soap);
    soap_destroy(soap);
    soap_end(soap);
    soap_done(soap);
    soap_free(soap);

    return result;
}


int  ont_onvifdevice_getprofile(device_onvif_t *devicePtr)
{
    struct _trt__GetProfiles trt__GetProfile = { 0 };
    struct _trt__GetProfilesResponse trt__Response = { 0 };
    static struct soap *soap;
    struct SOAP_ENV__Header header = { 0 };
    int i = 0;
    int result = 0;
    soap = soap_new();

    soap_set_namespaces(soap, namespaces);
    soap_default_SOAP_ENV__Header(soap, &header);
    soap_wsse_add_Security(soap);
    soap_wsse_add_UsernameTokenDigest(soap, "Id", (const char*)devicePtr->strUser, (const char*)devicePtr->strPass);
    do 
    {
        if (devicePtr->hasMedia)
        {
            result = soap_call___trt__GetProfiles(soap, devicePtr->strMedia, NULL, &trt__GetProfile, &trt__Response);
            if (result != SOAP_OK)
            {
                result = soap->error;
                printf("%s:%d,soap error: %d, %s, %s\n", __FILE__, __LINE__, soap->error, *soap_faultcode(soap), *soap_faultstring(soap));
                break;
            }
            else
            {
                for (i = 0; i<trt__Response.__sizeProfiles; i++)
                {

                    if (trt__Response.Profiles[i].Name != NULL)
                    {

                    }
                    if (trt__Response.Profiles[i].token != NULL)
                    {
                        memcpy(devicePtr->profiles[i].strprofile, trt__Response.Profiles[i].token, strlen(trt__Response.Profiles[i].token));
						devicePtr->profiles[i].level = trt__Response.__sizeProfiles - i;
					}
                    if (trt__Response.Profiles[i].VideoEncoderConfiguration)
                    {
                        struct tt__VideoEncoderConfiguration *pVideoConfig = trt__Response.Profiles[i].VideoEncoderConfiguration;
                        if (pVideoConfig->Encoding == tt__VideoEncoding__H264)
                        {
							devicePtr->profiles[i].height = pVideoConfig->Resolution->Height;
							devicePtr->profiles[i].width = pVideoConfig->Resolution->Width;
                            if (pVideoConfig->RateControl)
                            {
								devicePtr->profiles[i].framterate= pVideoConfig->RateControl->FrameRateLimit;
								devicePtr->profiles[i].bitrate = pVideoConfig->RateControl->BitrateLimit;
                            }
                        }
                        else
                        {
                            printf("%s:%d,VideoEncoding is not H264 \n", __FILE__, __LINE__);
                            result = -1;
                            break;
                        }
                    }

                    if (trt__Response.Profiles[i].PTZConfiguration)
                    {
						if (trt__Response.Profiles[i].PTZConfiguration->DefaultContinuousPanTiltVelocitySpace)
						{
							devicePtr->hasCoutinusPanTilt = 1;
							if (!devicePtr->strPtzToken[0]) /*only set once*/
							{
								ont_platform_snprintf(devicePtr->strPtzToken, sizeof(devicePtr->strPtzToken), "%s", trt__Response.Profiles[i].PTZConfiguration->token);
							}
                        }
                        if (trt__Response.Profiles[i].PTZConfiguration->DefaultContinuousZoomVelocitySpace)
                        {
                            devicePtr->hasCoutinusZoom = 1;
                        }
                    }
                }
            }
        }
    } while (0);

    soap_wsse_delete_Security(soap);
    soap_wsse_verify_done(soap);
    soap_destroy(soap);
    soap_end(soap);
    soap_done(soap);
    soap_free(soap);

    return result;
}

int  ont_onvifdevice_capablity( const char *url, device_onvif_t *devicePtr)
{
    static struct soap *soap;
    struct SOAP_ENV__Header header = { 0 };
    const char* soap_endpoint = url;
    int i = 0;
    int result = 0;
    struct _tds__GetCapabilities _Capabilities = { 0 };
    struct tt__Capabilities *items = NULL;
    struct _tds__GetCapabilitiesResponse __GetCapabilitiesResponse = { 0 };
    enum tt__CapabilityCategory _eCapAll[1] = { tt__CapabilityCategory__All };

    _Capabilities.__sizeCategory = 1;
    _Capabilities.Category = _eCapAll;

    soap = soap_new();
    soap_set_namespaces(soap, namespaces);
    soap_default_SOAP_ENV__Header(soap, &header);
    soap_wsse_add_Security(soap);
    soap_wsse_add_UsernameTokenDigest(soap, "Id", (const char*)devicePtr->strUser, (const char*)devicePtr->strPass);

    soap->recv_timeout = 3;
    soap->connect_timeout = 3;
    soap->send_timeout = 3;

    do 
    {
        result = soap_call___tds__GetCapabilities(soap, soap_endpoint, NULL, &_Capabilities, &__GetCapabilitiesResponse);
        if (result != SOAP_OK)
        {
            printf("%s:%d,soap error: %d, %s, %s\n", __FILE__, __LINE__, soap->error, *soap_faultcode(soap), *soap_faultstring(soap));
            break;
        }

        for (i = 0; i < _Capabilities.__sizeCategory; i++)
        {
            if (__GetCapabilitiesResponse.Capabilities != NULL)
            {
                items = __GetCapabilitiesResponse.Capabilities;
                /* Media */
                if (items->Media != NULL)
                {
                    strncpy(devicePtr->strMedia, items->Media->XAddr, sizeof(devicePtr->strMedia));
                    devicePtr->hasMedia = 1;
                }

                /* PTZ */
                if (items->PTZ != NULL)
                {
                    devicePtr->hasPTZ = 1;
                    strncpy(devicePtr->strPTZ, items->PTZ->XAddr, sizeof(devicePtr->strPTZ));
                }

                /* Event */
                if (items->Events != NULL)
                {
                    devicePtr->hasEvent = 1;
                    strncpy(devicePtr->strEvent, items->Events->XAddr, sizeof(devicePtr->strEvent));
                }

                /* Imaging */
                if (items->Imaging != NULL)
                {
                    devicePtr->hasImaging = 1;
                    strncpy(devicePtr->strImaging, items->Imaging->XAddr, sizeof(devicePtr->strImaging));
                }

                /* Extension */
                if (items->Extension != NULL)
                {
                    /* Receiver */
                    if (items->Extension->Receiver != NULL)
                    {
                        devicePtr->hasReceiver = 1;
                        strncpy(devicePtr->strReceiver, items->Extension->Receiver->XAddr, sizeof(devicePtr->strReceiver));
                    }
                    /* Recording */
                    if (items->Extension->Recording != NULL)
                    {
                        devicePtr->hasRecording = 1;
                        strncpy(devicePtr->strRecording, items->Extension->Recording->XAddr, sizeof(devicePtr->strRecording));
                    }
                    /* Search */
                    if (items->Extension->Search != NULL)
                    {
                        devicePtr->hasSearch = 1;
                        strncpy(devicePtr->strSearch, items->Extension->Search->XAddr, sizeof(devicePtr->strSearch));
                    }
                    /* Replay */
                    if (items->Extension->Replay != NULL)
                    {
                        devicePtr->hasReplay = 1;
                        strncpy(devicePtr->strReplay, items->Extension->Replay->XAddr, sizeof(devicePtr->strReplay));
                    }
                }

                break; /*only get one device*/
            }
        }
    } while (0);


    
    soap_wsse_delete_Security(soap);
    soap_wsse_verify_done(soap);
    soap_destroy(soap);
    soap_end(soap);
    soap_done(soap);
    soap_free(soap);

    return result;

}

int  ont_onvifdevice_getplayurl(device_onvif_t *devicePtr, int profileindex)
{
    struct _trt__GetStreamUri trt__GetStreamUri = { 0 };
    struct _trt__GetStreamUriResponse trt__GetStreamUriResponse = { 0 };
    struct tt__StreamSetup _tt__StreamSetup = { 0 };
    struct tt__Transport tt__transport = { 0 };
    int result = 0;
    trt__GetStreamUri.StreamSetup = &_tt__StreamSetup;
    trt__GetStreamUri.StreamSetup->Stream = 0;//stream type  

    trt__GetStreamUri.StreamSetup->Transport = &tt__transport;
    trt__GetStreamUri.StreamSetup->Transport->Protocol = 2;
    trt__GetStreamUri.StreamSetup->Transport->Tunnel = 0;

    trt__GetStreamUri.ProfileToken = devicePtr->profiles[profileindex].strprofile;
    struct soap *soap;
    struct SOAP_ENV__Header header = { 0 };

    soap = soap_new();
    soap_set_namespaces(soap, namespaces);
    soap_default_SOAP_ENV__Header(soap, &header);
    soap_wsse_add_Security(soap);
    soap_wsse_add_UsernameTokenDigest(soap, "Id", (const char*)devicePtr->strUser, (const char*)devicePtr->strPass);
    soap->recv_timeout = 5;


    do 
    {
        result = soap_call___trt__GetStreamUri(soap, devicePtr->strMedia, NULL, &trt__GetStreamUri, &trt__GetStreamUriResponse);
        if (result != SOAP_OK)
        {
            printf("%s:%d,soap error: %d, %s, %s\n", __FILE__, __LINE__, soap->error, *soap_faultcode(soap), *soap_faultstring(soap));
            break;
        }
        else
        {
            strncpy(devicePtr->profiles[profileindex].strPlayurl, trt__GetStreamUriResponse.MediaUri->Uri, sizeof(devicePtr->profiles[profileindex].strPlayurl));
        }
    } while (0);

    soap_wsse_delete_Security(soap);
    soap_wsse_verify_done(soap);
    soap_destroy(soap);
    soap_end(soap);
    soap_done(soap);
    soap_free(soap);

    return result;
}

/*#include "namespace.h"*/

void ont_gen_uuid(char _HwId[1024])
{
	unsigned char macaddr[6];
	unsigned int Flagrand;

	srand((int)time(0));
	Flagrand = rand() % 9000 + 1000;
	macaddr[0] = 0x1; macaddr[1] = 0x2; macaddr[2] = 0x3; macaddr[3] = 0x4; macaddr[4] = 0x5; macaddr[5] = 0x6;

	sprintf(_HwId, "urn:uuid:%ud68b-1dd2-11b2-a105-%02X%02X%02X%02X%02X%02X",
		Flagrand, macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);

}

device_onvif_discovery_info_t* getDeviceOnvifDiscoveryPtr(device_onvif_discovery_list_t * dodlPtr, int* index)
{
    int i = *index;
    device_onvif_discovery_info_t *dodiPtr = NULL;

    if (dodlPtr->list == NULL) {
        dodlPtr->list = malloc(M_ONVIFDEVICE_DISCOVERY_NUMS * sizeof(device_onvif_discovery_info_t));
        memset(dodlPtr->list, 0x00, M_ONVIFDEVICE_DISCOVERY_NUMS * sizeof(device_onvif_discovery_info_t));
        dodlPtr->number = M_ONVIFDEVICE_DISCOVERY_NUMS;
    }

    while (i < dodlPtr->number) {
        if (dodlPtr->list[i].srvAddr[0] == '\0') {
            dodiPtr = &dodlPtr->list[i++];
            break;
        } else {
            i++;
        }
    }

    *index = i;

    if (dodiPtr == NULL) {
        dodlPtr->list = realloc(dodlPtr->list, dodlPtr->number * 2 * sizeof(device_onvif_discovery_info_t));
        dodiPtr = &dodlPtr->list[dodlPtr->number];
        memset(dodlPtr->list, 0x00,  dodlPtr->number * sizeof(device_onvif_discovery_info_t));
        dodlPtr->number = dodlPtr->number * 2;
    }
    return dodiPtr;
}


static struct soap* ont_onvif_initsoap(struct SOAP_ENV__Header *header, const char *was_To, const char *was_Action, int timeout, const char* ip)
{
	struct soap *soap = NULL;
	unsigned char macaddr[6];
	char _HwId[1024];
	unsigned int Flagrand;
	soap = soap_new();
	if (soap == NULL)
	{
		printf("[%d]soap = NULL\n", __LINE__);
		return NULL;
	}
	soap_set_namespaces(soap, namespaces);

	if (timeout > 0)
	{
		soap->recv_timeout = timeout;
		soap->send_timeout = timeout;
		soap->connect_timeout = timeout;
	}
	else
	{
		soap->recv_timeout = 3;
		soap->send_timeout = 3;
		soap->connect_timeout = 3;
	}
    soap->connect_flags = SO_BROADCAST;
    soap->user = (void *)ip;

	soap_default_SOAP_ENV__Header(soap, header);

	srand((int)time(0));
	Flagrand = rand() % 9000 + 1000;
	macaddr[0] = 0x1; macaddr[1] = 0x2; macaddr[2] = 0x3; macaddr[3] = 0x4; macaddr[4] = 0x5; macaddr[5] = 0x6;

	sprintf(_HwId, "urn:uuid:%ud68b-1dd2-11b2-a105-%02X%02X%02X%02X%02X%02X",
		Flagrand, macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);

	header->wsa__MessageID = (char *)malloc(100);
	memset(header->wsa__MessageID, 0, 100);
	strncpy(header->wsa__MessageID, _HwId, strlen(_HwId));

	printf("messageid:%s\n", header->wsa__MessageID);
	if (was_Action != NULL)
	{
		header->wsa__Action = (char *)malloc(1024);
		memset(header->wsa__Action, '\0', 1024);
		strncpy(header->wsa__Action, was_Action, 1024);//"http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe";
	}
	if (was_To != NULL)
	{
		header->wsa__To = (char *)malloc(1024);
		memset(header->wsa__To, '\0', 1024);
		strncpy(header->wsa__To, was_To, 1024);
	}
	soap->header = header;
	return soap;
}

int ont_onvif_device_discovery( device_onvif_cluster_t *cluster )
{
    device_onvif_discovery_list_t *_gOnvifDeviceDiscoveryPtr = &cluster->onvif_device_discovery_list;
	int HasDev = 0;
    int i = 0;
	int retval = SOAP_OK;
	wsdd__ProbeType req;
	struct __wsdd__ProbeMatches resp;
	wsdd__ScopesType sScope;
	struct SOAP_ENV__Header header;
	struct soap* soap;

    //获取所有网卡的IP地址
    SearchIPAddress searchIpAddress = {NULL, 0};
    getIPAddress(&searchIpAddress);

    //每个网卡进行多播
    for (i = 0; i < searchIpAddress.number && searchIpAddress.list[i].ip[0] != '\0' ; i++) {
        const char *was_To = "urn:schemas-xmlsoap-org:ws:2005:04:discovery";
        const char *was_Action = "http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe";
        const char *soap_endpoint = "soap.udp://239.255.255.250:3702";

        const char *ip = searchIpAddress.list[i].ip;
        soap = ont_onvif_initsoap(&header, was_To, was_Action, 1, ip);
        soap->header = &header;

        soap_default_wsdd__ScopesType(soap, &sScope);
        sScope.__item = "";
        soap_default_wsdd__ProbeType(soap, &req);
        req.Scopes = &sScope;
        req.Types = "dn:NetworkVideoTransmitter";

        retval = soap_send___wsdd__Probe(soap, soap_endpoint, was_Action, &req);

        //tds:Device
        while (retval == SOAP_OK)
        {
            retval = soap_recv___wsdd__ProbeMatches(soap, &resp);
            if (retval == SOAP_OK)
            {
                if (soap->error)
                {
                    printf("[%d][ip: %s] recv soap error: %d, %s, %s\n", __LINE__, ip, soap->error, *soap_faultcode(soap), *soap_faultstring(soap));
                    retval = soap->error;
                }
                else //成功接收某一个设备的消息
                {
                    if (resp.wsdd__ProbeMatches->ProbeMatch != NULL && resp.wsdd__ProbeMatches->ProbeMatch->XAddrs != NULL)
                    {
                        device_onvif_discovery_info_t* dodiPtr = getDeviceOnvifDiscoveryPtr(_gOnvifDeviceDiscoveryPtr, &HasDev);
                        printf("###[ip: %s]: ####### recv %d devices info ################# \n", ip, HasDev);
                        printf("Target Service Address  : %s\r\n", resp.wsdd__ProbeMatches->ProbeMatch->XAddrs);
                        memcpy(dodiPtr->srvAddr, resp.wsdd__ProbeMatches->ProbeMatch->XAddrs, strlen(resp.wsdd__ProbeMatches->ProbeMatch->XAddrs));
                        printf("Target EP Address       : %s\r\n", resp.wsdd__ProbeMatches->ProbeMatch->wsa__EndpointReference.Address);
                        memcpy(dodiPtr->epAddr, resp.wsdd__ProbeMatches->ProbeMatch->wsa__EndpointReference.Address, strlen(resp.wsdd__ProbeMatches->ProbeMatch->wsa__EndpointReference.Address));
                        printf("Target Type             : %s\r\n", resp.wsdd__ProbeMatches->ProbeMatch->Types);
                        memcpy(dodiPtr->types, resp.wsdd__ProbeMatches->ProbeMatch->Types, strlen(resp.wsdd__ProbeMatches->ProbeMatch->Types));
                        //ont_platform_sleep(1000);
                    }
                }
            }
            else if (soap->error)
            {
                if (HasDev == 0)
                {
                    printf("[%s][%s][Line:%d][ip: %s] Device discovery or soap error: %d, %s, %s \n", __FILE__, __FUNCTION__, __LINE__, ip, soap->error, *soap_faultcode(soap), *soap_faultstring(soap));
                    retval = soap->error;
                }
                else
                {
                    printf(" [%s]-[%d][ip: %s] Search end! It has Searched %d devices! \n", __FUNCTION__, __LINE__, ip, HasDev);
                    retval = 0;
                }
                break;
            }
        }
    }

    free(searchIpAddress.list);

	soap_destroy(soap);
	soap_end(soap);
	soap_free(soap);

	return retval;
}


int ont_onvifdevice_srvaddr_omit_port(char *tempurl, char *url)
{
    char *p = NULL;
    int len = 0;
    p = strstr(url, ":80");
    if (NULL != p) {
        memcpy(tempurl, url, p - url);
        tempurl[p-url] = '\0';
        strcat(tempurl, p+3);
    } else {
        len = strlen(url);
        memcpy(tempurl, url, len);
        tempurl[len] = '\0';
    }
    return 0;
}

int ont_onvifdevice_checkdevice_online( device_onvif_cluster_t *cluster, const char*url )
{
    device_onvif_discovery_list_t *_gOnvifDeviceDiscoveryPtr = &cluster->onvif_device_discovery_list;
    int i = 0;
    char tempurl[256];
    char tempSrvAddr[256];
    ont_onvifdevice_srvaddr_omit_port(tempurl, (char *)url);

    for (i = 0; i < _gOnvifDeviceDiscoveryPtr->number && _gOnvifDeviceDiscoveryPtr->list[i].srvAddr[0] != '\0'; i++) {
        char *srvAddr = _gOnvifDeviceDiscoveryPtr->list[i].srvAddr;
        ont_onvifdevice_srvaddr_omit_port(tempSrvAddr, srvAddr);

        if (strstr(tempSrvAddr, tempurl)) {
            return 0;
        }
    }
    return -1;
}

device_onvif_cluster_t* ont_onvif_device_cluster_create( void )
{
    device_onvif_cluster_t *cluster = (device_onvif_cluster_t*)malloc( sizeof(device_onvif_cluster_t) );
    if ( cluster )
    {
        cluster->onvif_device_list.list = NULL;
        cluster->onvif_device_list.number = 0;
        cluster->onvif_device_discovery_list.list = NULL;
        cluster->onvif_device_discovery_list.number = 0;
    }
	return cluster;
}

void ont_onvif_device_cluster_destroy( device_onvif_cluster_t *cluster )
{
    if ( cluster )
    {
        if ( cluster->onvif_device_list.list )
        {
            free( cluster->onvif_device_list.list );
        }

        if ( cluster->onvif_device_discovery_list.list )
        {
            free( cluster->onvif_device_discovery_list.list );
        }

        free( cluster );
    }
}

int ont_onvifdevice_adddevice( device_onvif_cluster_t *cluster,
                               int                     channel,
                               const char             *url,
                               const char             *user,
                               const char             *passwd )
{
    device_onvif_list_t *_gOnvifDevicePtr = &cluster->onvif_device_list;
	device_onvif_t *devicePtr = NULL;
	int i = 0;
	int result = 0;

	if (_gOnvifDevicePtr->list == NULL)
	{
		_gOnvifDevicePtr->list = malloc(M_ONVIFDEVICE_NUMBERS * sizeof(device_onvif_t));
		memset(_gOnvifDevicePtr->list, 0x00, M_ONVIFDEVICE_NUMBERS * sizeof(device_onvif_t));
		_gOnvifDevicePtr->number = M_ONVIFDEVICE_NUMBERS;
	}

	while (i < _gOnvifDevicePtr->number)
	{
		if (_gOnvifDevicePtr->list[i].strUrl[0] == '\0')
		{
			devicePtr = &_gOnvifDevicePtr->list[i];
			break;
		}
		else
		{
			i++;
		}
	}

	if (devicePtr == NULL)
	{
		_gOnvifDevicePtr->list = realloc(_gOnvifDevicePtr->list, _gOnvifDevicePtr->number * 2 * sizeof(device_onvif_t));
		devicePtr = &_gOnvifDevicePtr->list[_gOnvifDevicePtr->number];
		memset(&devicePtr, 0x00, _gOnvifDevicePtr->number * sizeof(device_onvif_t));
		_gOnvifDevicePtr->number = _gOnvifDevicePtr->number * 2;
	}

    memcpy(devicePtr->strUser, user, strlen(user));
    memcpy(devicePtr->strPass, passwd, strlen(passwd));
    devicePtr->channelid = channel;

    result = ont_onvifdevice_capablity(url, devicePtr);
    if (result != SOAP_OK)
    {
		printf("[error]:ont_onvifdevice_capablity fail for url=%s user=%s pass=%s.\n", url, devicePtr->strUser, devicePtr->strPass);
        return -1;
    }
    memcpy(devicePtr->strUrl, url, strlen(url));
    devicePtr->hasGetCap = 1;

    result = ont_onvifdevice_getprofile(devicePtr);
    if (result != SOAP_OK)
    {
		printf("[error]:ont_onvifdevice_getprofile fail for url=%s user=%s pass=%s\n", devicePtr->strUrl, devicePtr->strUser, devicePtr->strPass);
        return -1;
    }

	RTMP_Log(RTMP_LOGINFO, "[info]:url=%s has profiles-------------------------------------------------------:\n", devicePtr->strUrl);
    for (i = 0; i < 4; i++)
    {
        if (devicePtr->profiles[i].strprofile[0] != '\0')
        {
            result = ont_onvifdevice_getplayurl(devicePtr, i);
            if (result != SOAP_OK)
            {
                return -1;
            }
			RTMP_Log(RTMP_LOGINFO, "profile [%d: level=%d bitrate=%d framterate=%d] \n", 
				i, 
				devicePtr->profiles[i].level, 
				devicePtr->profiles[i].bitrate, 
				devicePtr->profiles[i].framterate);
        }
    }

    if (result != SOAP_OK)
    {
        return -1;
    }

    result = ont_onvifdevice_getconfigurations(devicePtr);
    if (result != SOAP_OK)
    {
        return -1;
    }

    return 0;

}

void ont_onvifdevice_deldevice( device_onvif_cluster_t *cluster, int channel )
{
    device_onvif_t *dev = ont_getonvifdevice( cluster, channel );
    if ( dev )
    {
        memset( dev, 0, sizeof(device_onvif_t) );
    }
}

device_onvif_t* ont_getonvifdevice( device_onvif_cluster_t *cluster, int channelid )
{
    device_onvif_list_t *_gOnvifDevicePtr = &cluster->onvif_device_list;
    int i = 0;
    
    for (i = 0; i<_gOnvifDevicePtr->number; i++)
    {
        if (_gOnvifDevicePtr->list[i].channelid == channelid)
        {
            return &_gOnvifDevicePtr->list[i];
        }
    }
    RTMP_Log(RTMP_LOGINFO, "no this channel %d\n", channelid);     
    return  NULL;
}


char* ont_geturl_level( device_onvif_t *devicePtr,
                        int             level,
                        RTMPMetadata   *meta )
{
    int i = 0;
	if (!devicePtr)
	{
		return NULL;
	}

    for (i = 0; i < ONVF_DEVICE_PROFILES; i++)
    {
        if (devicePtr->profiles[i].level == level)
        {
			/*get the url to avoid timeout*/
			if (ont_onvifdevice_getplayurl(devicePtr, i) != SOAP_OK)
			{
				return NULL;
			}
			meta->width = devicePtr->profiles[i].width;
			meta->height = devicePtr->profiles[i].height;
			meta->videoDataRate = devicePtr->profiles[i].bitrate;
			meta->frameRate = devicePtr->profiles[i].framterate;
            return devicePtr->profiles[i].strPlayurl;
        }
    }
    printf("no this level %d\n", level);        
	return NULL;
}

static int _ont_onvifdevice_stop(device_onvif_t *devicePtr)
{
    static struct soap *soap;
    struct SOAP_ENV__Header header = { 0 };
    enum xsd__boolean flag = 1;
    struct _tptz__Stop stop;
    struct _tptz__StopResponse tptz__StopResponse = { 0 };
    int result = 0;
    stop.PanTilt = &flag;
    stop.Zoom = &flag;
    stop.ProfileToken = devicePtr->profiles[0].strprofile;


    soap = soap_new();
    soap->recv_timeout = 10;
    soap_set_namespaces(soap, namespaces);
    soap_default_SOAP_ENV__Header(soap, &header);

    soap_wsse_add_Security(soap);
    soap_wsse_add_UsernameTokenDigest(soap, "Id", devicePtr->strUser, devicePtr->strPass);
    do
    {
        result = soap_call___tptz__Stop(soap, devicePtr->strPTZ, NULL, &stop, &tptz__StopResponse);
        if (result != SOAP_OK)
        {
            printf("%s:%d,soap error: %d, %s, %s\n", __FILE__, __LINE__, soap->error, *soap_faultcode(soap), *soap_faultstring(soap));
            break;
        }
    } while (0); 
    soap_wsse_delete_Security(soap);
    soap_wsse_verify_done(soap);
    soap_destroy(soap);
    soap_end(soap);
    soap_done(soap);
    soap_free(soap);
    return result;

}
/*
1: 焦距变大(倍率变大)  
2: 焦距变小(倍率变小) 
3: 焦点前调
4: 焦点后调 
5: 光圈扩大 
6: 光圈缩小 
11: 方向向上
12: 方向向下
13: 方向左转
14: 方向右转 
22:自动扫描, 不支持
*/
int ont_onvifdevice_ptz( device_onvif_cluster_t *cluster,
                         int                     channel,
                         int                     cmd,
                         int                     _speed, /* [1-7] */
                         int                     status )
{

	static struct soap *soap;
	device_onvif_t *devicePtr;
	int result = 0;
	struct SOAP_ENV__Header header = { 0 };
	struct tt__PTZSpeed ptzSpeed = { NULL };
	struct tt__Vector2D PanTilt = { 0, };
	struct tt__Vector1D Zoom = { 0, };
	LONG64 timeout = 0;
	double speed = _speed*1.0 / (7.0 * 3);

	struct _tptz__ContinuousMove tptz__ContinuousMove;
	struct _tptz__ContinuousMoveResponse tptz__ContinuousMoveResponse;

    devicePtr = ont_getonvifdevice( cluster, channel );
    if (!devicePtr)
    {
        return -1;
    }
    
    if (!devicePtr->hasCoutinusPanTilt)
    {
        return -1;
    }
	if (!devicePtr->hasPTZ)
	{
		return -1;
	}

    tptz__ContinuousMove.ProfileToken = devicePtr->profiles[0].strprofile;
    tptz__ContinuousMove.Timeout = &timeout;
    tptz__ContinuousMove.Velocity = &ptzSpeed;

    if (status == 2) //step 
    {
        timeout = devicePtr->ptzTimeoutMin;
    }
    else if (status == 0)
    {
		timeout = -1;
    }
    else if (status == 1)
    {
        //stop
        return _ont_onvifdevice_stop(devicePtr);
    }
    else
    {
        return -1;
    }
    //pantilt.
    if (cmd == 11 || cmd == 12 || cmd == 13 || cmd == 14)
    {
        ptzSpeed.PanTilt = &PanTilt;
        if (cmd == 11)
        {
            PanTilt.x = 0;
            PanTilt.y = speed*devicePtr->ptYrangeMax;
        }
        else if (cmd == 12)
        {
            PanTilt.x = 0;
            PanTilt.y = speed*devicePtr->ptYrangeMin;
        }
        else if (cmd == 13)
        {
            PanTilt.y = 0;
            PanTilt.x = speed*devicePtr->ptXrangeMin;
        }
        else if (cmd == 14)
        {
            PanTilt.y = 0;
            PanTilt.x = speed*devicePtr->ptXrangeMax;
        }
    }
    //zoom
    else if (cmd == 1 || cmd == 2)
    {
        ptzSpeed.Zoom = &Zoom;
        if (cmd == 1)
        {
            Zoom.x = speed*devicePtr->zoomRangeMax;
        }
        else
        {
            Zoom.x = speed*devicePtr->zoomRangeMin;
        }
    }


    soap = soap_new();
    soap->recv_timeout = 10;
    soap_set_namespaces(soap, namespaces);
    soap_default_SOAP_ENV__Header(soap, &header);

    soap_wsse_add_Security(soap);
    soap_wsse_add_UsernameTokenDigest(soap, "Id", devicePtr->strUser, devicePtr->strPass);

    do
    {
        result = soap_call___tptz__ContinuousMove(soap, devicePtr->strPTZ, NULL, &tptz__ContinuousMove, &tptz__ContinuousMoveResponse);
        if (result != SOAP_OK)
        {
            printf("%s:%d,soap error: %d, %s, %s\n", __FILE__, __LINE__, soap->error, *soap_faultcode(soap), *soap_faultstring(soap));
            break;
        }
    } while (0);

    soap_wsse_delete_Security(soap);
    soap_wsse_verify_done(soap);
    soap_destroy(soap);
    soap_end(soap);
    soap_done(soap);
    soap_free(soap);
    return 0;
}
