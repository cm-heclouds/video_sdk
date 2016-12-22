#include <stdlib.h>
#include <string.h>
#include "ont/platform.h"
#include "librtmp/rtmp.h"
#include "librtmp/rtmp_sys.h"
#include "librtmp/log.h"
#include "librtmp/amf.h"

#include "ont/video.h"

#define RTMP_HEAD_SIZE   (sizeof(RTMPPacket)+RTMP_MAX_HEADER_SIZE)



unsigned char  rtmp_make_audio_headerTag(unsigned int format, unsigned int samplerate, unsigned int samplesize, unsigned int sampletype)
{
    return (format << 4) | (samplerate << 2 & 0x0f) | (samplesize<<1 & 0x02) | (sampletype & 0x01);
}


int rtmp_send_spspps(void *_r, unsigned char * sps, int sps_len, unsigned char *pps, int pps_len, int ts)
{
    RTMP* r = _r;
    RTMPPacket * packet = NULL;/*rtmp包结构*/
    unsigned char * body = NULL;
    int i;
    int max_packet = RTMP_HEAD_SIZE + pps_len + sps_len + 32;
    packet = (RTMPPacket *)ont_platform_malloc(max_packet);
    if (packet == NULL){
        abort();
        return -1;
    }
    memset(packet, 0, max_packet);
    packet->m_body = (char *)packet + RTMP_HEAD_SIZE;
    body = (unsigned char *)packet->m_body;
    i = 0;
    body[i++] = 0x17;
    body[i++] = 0x00;

    body[i++] = 0x00;
    body[i++] = 0x00;
    body[i++] = 0x00;

    /*AVCDecoderConfigurationRecord*/
    body[i++] = 0x01;
    body[i++] = sps[1];
    body[i++] = sps[2];
    body[i++] = sps[3];
    body[i++] = 0xff;

    /*sps*/
    body[i++] = 0xe1;
    body[i++] = (sps_len >> 8) & 0xff;
    body[i++] = sps_len & 0xff;
    memcpy(&body[i], sps, sps_len);
    i += sps_len;

    /*pps*/
    body[i++] = 0x01;
    body[i++] = (pps_len >> 8) & 0xff;
    body[i++] = (pps_len)& 0xff;
    memcpy(&body[i], pps, pps_len);
    i += pps_len;

    packet->m_packetType = RTMP_PACKET_TYPE_VIDEO;
    packet->m_nBodySize = i;
    packet->m_nChannel = 0x04;
    packet->m_nTimeStamp = ts;
    packet->m_hasAbsTimestamp = 0;
    packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet->m_nInfoField2 = r->m_stream_id;

    /*调用发送接口*/
    int nRet = RTMP_SendPacket(r, packet, FALSE);
    ont_platform_free(packet);   
    return nRet==TRUE?0:-1;
}

static int _rtmp_setchunksize(RTMP* rtmp, uint32_t size)
{
    RTMP* r = rtmp;
    RTMPPacket * packet = NULL;
    unsigned char * body = NULL;
    int i;
    int max_packet = RTMP_HEAD_SIZE + 4;
    packet = (RTMPPacket *)malloc(max_packet);
    memset(packet, 0, max_packet);
    packet->m_body = (char *)packet + RTMP_HEAD_SIZE;
    body = (unsigned char *)packet->m_body;
    i = 0;
    body[i++] = size>>24&0xff;
    body[i++] = size>>16&0xff;
    body[i++] = size>>8&0xff;
    body[i++] = size&0xff;

    packet->m_packetType = RTMP_PACKET_TYPE_CHUNK_SIZE;
    packet->m_nBodySize = 4;
    packet->m_nChannel = 0x04;
    packet->m_nTimeStamp = 0;
    packet->m_hasAbsTimestamp = 0;
    packet->m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    packet->m_nInfoField2 = 0; 
    int nRet =  RTMP_SendPacket(r, packet, FALSE);
    if (nRet)
    {
        r->m_outChunkSize = size;
    }
    free(packet);
    return nRet == TRUE ? 0 : -1;
}


void* rtmp_create_publishstream(const char *pushurl, unsigned int sock_timeout_seconds, const char*deviceid)
{
    RTMP* rtmp = NULL;
    char url[256] = { 0 };
    rtmp = RTMP_Alloc();
    /*after device init.so, no need to init socket.*/
    RTMP_Init(rtmp);
    /*set connection timeout,default 30s*/
    rtmp->Link.timeout = sock_timeout_seconds;
    ont_platform_snprintf(url, sizeof(url), "%s?%s", pushurl, deviceid);
    if (!RTMP_SetupURL(rtmp, url))
    {
        RTMP_Log(RTMP_LOGERROR, "SetupURL Err\n");
        RTMP_Free(rtmp);
        return NULL;
    }

    /*if unable,the AMF command would be 'play' instead of 'publish'*/
    RTMP_EnableWrite(rtmp);
    if (!RTMP_Connect(rtmp, NULL)){
        RTMP_Log(RTMP_LOGERROR, "Connect Err\n");
        RTMP_Free(rtmp);
        return NULL;
    }

    if (!RTMP_ConnectStream(rtmp, 0)){
        RTMP_Log(RTMP_LOGERROR, "ConnectStream Err\n");
        RTMP_Close(rtmp);
        RTMP_Free(rtmp);
        return NULL;
    }
    /*RTMP_LogPrintf("could send data now\n");*/
    return rtmp;
}



int rtmp_send_metadata(void* r, RTMPMetadata *metadata)
{
    char body[1024] = { 0 };
    char * p = (char *)body;
    char *pend = p + sizeof(body);
    AVal avValue;
    RTMPPacket packet;
    int ret;
    RTMP *rtmp = r;
    if (metadata == NULL)
    {
        return -1;
    }


    avValue.av_val = "@setDataFrame";
    avValue.av_len = strlen(avValue.av_val);

    p = AMF_EncodeString(p, pend, &avValue);

    avValue.av_val = "onMetaData";
    avValue.av_len = strlen(avValue.av_val);
    p = AMF_EncodeString(p, pend, &avValue);

    AMFObject obj;
    memset(&obj, 0x00, sizeof(obj));


    AMF_Reset(&obj);

    AMFObjectProperty prop;
    memset(&prop, 0x00, sizeof(prop));
    avValue.av_val = "duration";
    avValue.av_len = strlen(avValue.av_val);
    AMFProp_SetName(&prop, &avValue);
    prop.p_type = AMF_NUMBER;
    prop.p_vu.p_number = metadata->duration;
    AMF_AddProp(&obj, &prop);

    avValue.av_val = "width";
    avValue.av_len = strlen(avValue.av_val);
    AMFProp_SetName(&prop, &avValue);
    prop.p_type = AMF_NUMBER;
    prop.p_vu.p_number = metadata->width;
    AMF_AddProp(&obj, &prop);

    avValue.av_val = "height";
    avValue.av_len = strlen(avValue.av_val);
    AMFProp_SetName(&prop, &avValue);
    prop.p_type = AMF_NUMBER;
    prop.p_vu.p_number = metadata->height;
    AMF_AddProp(&obj, &prop);


    avValue.av_val = "videorate";
    avValue.av_len = strlen(avValue.av_val);
    AMFProp_SetName(&prop, &avValue);
    prop.p_type = AMF_NUMBER;
    prop.p_vu.p_number = ((double)metadata->videoDataRate) / 1000;
    AMF_AddProp(&obj, &prop);

    avValue.av_val = "framerate";
    avValue.av_len = strlen(avValue.av_val);
    AMFProp_SetName(&prop, &avValue);
    prop.p_type = AMF_NUMBER;
    prop.p_vu.p_number = metadata->frameRate;
    AMF_AddProp(&obj, &prop);

    avValue.av_val = "videocodecid";
    avValue.av_len = strlen(avValue.av_val);
    AMFProp_SetName(&prop, &avValue);
    prop.p_type = AMF_NUMBER;
    prop.p_vu.p_number = metadata->videoCodecid;
    AMF_AddProp(&obj, &prop);

    if (metadata->hasAudio)
    {
        avValue.av_val = "audiocodecid";
        avValue.av_len = strlen(avValue.av_val);
        AMFProp_SetName(&prop, &avValue);
        prop.p_type = AMF_NUMBER;
        prop.p_vu.p_number = metadata->audioCodecid;
        AMF_AddProp(&obj, &prop);

        if (metadata->audioDataRate != 0)
        {
            avValue.av_val = "audiodataerate";
            avValue.av_len = strlen(avValue.av_val);
            AMFProp_SetName(&prop, &avValue);
            prop.p_type = AMF_NUMBER;
            prop.p_vu.p_number = metadata->audioDataRate;
            AMF_AddProp(&obj, &prop);
        }
        
        avValue.av_val = "audiosamplerate";
        avValue.av_len = strlen(avValue.av_val);
        AMFProp_SetName(&prop, &avValue);
        prop.p_type = AMF_NUMBER;
        prop.p_vu.p_number = metadata->audioSampleRate;
        AMF_AddProp(&obj, &prop);
    }

    p = AMF_Encode(&obj, p, pend);

    RTMPPacket_Reset(&packet);
    RTMPPacket_Alloc(&packet, p - body);

    memcpy(packet.m_body, body, p-body);

    packet.m_nBodySize = p - body;
    packet.m_packetType = RTMP_PACKET_TYPE_INFO;
    packet.m_nChannel = 0x04;
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet.m_nInfoField2 = rtmp->m_stream_id;

    ret = RTMP_SendPacket(rtmp, &packet, 0);

    RTMPPacket_Free(&packet);

    AMF_Reset(&obj);


    _rtmp_setchunksize(rtmp, 1200);

    return ret==TRUE?0:-1;
}

int rtmp_check_rcv_handler(void *_r)
{
    RTMP*  r = _r;
    RTMPPacket packet = { 0 };
    while (RTMP_IsConnected(r))
    {
        int ret = RTMPSockBuf_Fill(&r->m_sb);
        if (ret == 0)
        {
            return 0;
        }
        else if (ret<0)
        {
            return -1;
        }
        RTMP_ReadPacket(r, &packet);
        if (packet.m_nBodySize == 0)
        {
            return 0;
        }
        else
        {
            if (RTMPPacket_IsReady(&packet))
            {
                RTMP_ClientPacket(r, &packet);
                RTMPPacket_Free(&packet);
            }
			else
			{
				return -1;
			}
        }
    }
	return -1;
}


int rtmp_send_videodata(void* r, unsigned char *data, unsigned int size, unsigned int ts, unsigned int keyFrame)
{
    RTMP *rtmp = r;
    RTMPPacket packet;
    
    if (rtmp == NULL)
    {
        return -1;
    }
    
    RTMPPacket_Reset(&packet);
    RTMPPacket_Alloc(&packet, size);
    
    packet.m_packetType = RTMP_PACKET_TYPE_VIDEO;
    packet.m_nChannel = 0x04;
    packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    packet.m_hasAbsTimestamp = 0;
    packet.m_nTimeStamp = ts;
    packet.m_nInfoField2 = rtmp->m_stream_id;
    memcpy(packet.m_body, data, size);
    packet.m_nBodySize += size;

    int nRet = RTMP_SendPacket(rtmp, &packet, 0);


    RTMPPacket_Free(&packet);
    return nRet==TRUE?0:-1;
}

int rtmp_send_audiodata(void* r, unsigned char headertag, unsigned char *data, unsigned int size, unsigned int ts, unsigned int headertype)
{
    RTMP *rtmp = r;
    RTMPPacket packet;

    if (rtmp == NULL)
    {
        return -1;
    }

    RTMPPacket_Reset(&packet);
    RTMPPacket_Alloc(&packet, size+1);

    packet.m_packetType = RTMP_PACKET_TYPE_AUDIO;
    packet.m_nChannel = 0x06;
    packet.m_hasAbsTimestamp = 0;
    packet.m_headerType = headertype;
    packet.m_nTimeStamp = ts;
    packet.m_nInfoField2 = rtmp->m_stream_id;
    packet.m_body[0] = headertag;
    packet.m_nBodySize++;

    memcpy(packet.m_body+1, data, size);
    packet.m_nBodySize += size;

    int nRet = RTMP_SendPacket(rtmp, &packet, 0);

    RTMPPacket_Free(&packet);
    return nRet==TRUE?0:-1;
}
