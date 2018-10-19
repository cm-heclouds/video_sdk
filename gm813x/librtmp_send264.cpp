#include <stdio.h>
#include <stdlib.h>
#include "librtmp_send264.h"
#include "librtmp/rtmp.h"
#include "librtmp/rtmp_sys.h"
#include "librtmp/amf.h"
#include "rtmp_if.h"
#include "log.h"
#include "sps_decode.h"

//定义包头长度，RTMP_MAX_HEADER_SIZE=18
#define RTMP_HEAD_SIZE   (sizeof(RTMPPacket)+RTMP_MAX_HEADER_SIZE)
//存储Nal单元数据的buffer大小
#define BUFFER_SIZE 80000
//搜寻Nal单元时的一些标志
#define GOT_A_NAL_CROSS_BUFFER BUFFER_SIZE+1
#define GOT_A_NAL_INCLUDE_A_BUFFER BUFFER_SIZE+2
#define NO_MORE_BUFFER_TO_READ BUFFER_SIZE+3
/**
 * _NaluUnit
 * 内部结构体。该结构体主要用于存储和传递Nal单元的类型、大小和数据
 */
typedef struct _NaluUnit {
	int type;
	int size;
	unsigned char *data;
} NaluUnit;

NaluUnit naluUnit;
/**
 * _RTMPMetadata
 * 内部结构体。该结构体主要用于存储和传递元数据信息
 */
typedef struct _RTMPLIBMetadata {
	// video, must be h264 type
	unsigned int    nWidth;
	unsigned int    nHeight;
	unsigned int    nFrameRate;
	unsigned int    nSpsLen;
	unsigned char   *Sps;
	unsigned int    nPpsLen;
	unsigned char   *Pps;
} RTMPLIBMetadata, *LPRTMPLIBMetadata;

enum {
	VIDEO_CODECID_H264 = 7,
};

/**
 * 初始化winsock
 *
 * @成功则返回1 , 失败则返回相应错误代码
 */
int InitSockets()
{
#ifdef WIN32
	WORD version;
	WSADATA wsaData;
	version = MAKEWORD(1, 1);
	return (WSAStartup(version, &wsaData) == 0);
#else
	return TRUE;
#endif
}


/**
 * 释放winsock
 *
 * @成功则返回0 , 失败则返回相应错误代码
 */
inline void CleanupSockets()
{
#ifdef WIN32
	WSACleanup();
#endif
}

//网络字节序转换
char *put_byte( char *output, uint8_t nVal )
{
	output[0] = nVal;
	return output + 1;
}

char *put_be16(char *output, uint16_t nVal )
{
	output[1] = nVal & 0xff;
	output[0] = nVal >> 8;
	return output + 2;
}

char *put_be24(char *output, uint32_t nVal )
{
	output[2] = nVal & 0xff;
	output[1] = nVal >> 8;
	output[0] = nVal >> 16;
	return output + 3;
}
char *put_be32(char *output, uint32_t nVal )
{
	output[3] = nVal & 0xff;
	output[2] = nVal >> 8;
	output[1] = nVal >> 16;
	output[0] = nVal >> 24;
	return output + 4;
}
char   *put_be64( char *output, uint64_t nVal )
{
	output = put_be32( output, nVal >> 32 );
	output = put_be32( output, nVal );
	return output;
}

char *put_amf_string( char *c, const char *str )
{
	uint16_t len = strlen( str );
	c = put_be16( c, len );
	memcpy(c, str, len);
	return c + len;
}
char *put_amf_double( char *c, double d )
{
	*c++ = AMF_NUMBER;  /* type: Number */
	{
		unsigned char *ci, *co;
		ci = (unsigned char *)&d;
		co = (unsigned char *)c;
		co[0] = ci[7];
		co[1] = ci[6];
		co[2] = ci[5];
		co[3] = ci[4];
		co[4] = ci[3];
		co[5] = ci[2];
		co[6] = ci[1];
		co[7] = ci[0];
	}
	return c + 8;
}


unsigned int  m_nFileBufSize;
unsigned int  nalhead_pos;

static RTMP *_pRtmp = NULL;
RTMPLIBMetadata metaData;

unsigned char *m_pFileBuf_tmp_old;  //used for realloc



extern "C" void sound_notify(void *ctx, int chunk, int ts, const char *data, int len);

/**
 * 初始化并连接到服务器
 *
 * @param url 服务器上对应webapp的地址
 *
 * @成功则返回1 , 失败则返回0
 */
int RTMP264_Connect(const char *url)
{
	nalhead_pos = 0;
	m_nFileBufSize = BUFFER_SIZE;

	InitSockets();

	_pRtmp = RTMP_Alloc();
	RTMP_Init(_pRtmp);
	/*设置URL*/
	if (RTMP_SetupURL(_pRtmp, (char *)url) == FALSE) {
		RTMP_Free(_pRtmp);
		return false;
	}
	/*设置可写,即发布流,这个函数必须在连接前使用,否则无效*/
	RTMP_EnableWrite(_pRtmp);

	/*连接服务器*/
	if (RTMP_Connect(_pRtmp, NULL) == FALSE) {

		RTMP_Free(_pRtmp);
		return false;
	}

	/*连接流*/
	if (RTMP_ConnectStream(_pRtmp, 0) == FALSE) {

		RTMP_Close(_pRtmp);
		RTMP_Free(_pRtmp);
		return false;
	}

	rtmp_set_sound_notify(_pRtmp, sound_notify, _pRtmp);

	return true;
}


/**
 * 断开连接，释放相关的资源。
 *
 */
void RTMP264_Close()
{
	if (_pRtmp) {
		RTMP_Close(_pRtmp);
		RTMP_Free(_pRtmp);
		_pRtmp = NULL;
	}
	CleanupSockets();
}


int GM8136_Rtmp_Rcvhandler()
{
	RTMP  *r = _pRtmp;
	RTMPPacket packet = { 0 };
	while (RTMP_IsConnected(r)) {
		int ret = RTMPSockBuf_Fill(&r->m_sb);
		if (ret == 0) {
			return 0;
		} else if (ret < 0) {
			return -1;
		}
		RTMP_ReadPacket(r, &packet);
		if (packet.m_nBodySize == 0) {
			return 0;
		} else {
			if (RTMPPacket_IsReady(&packet)) {
				RTMP_ClientPacket(r, &packet);
				RTMPPacket_Free(&packet);
			} else {
				return -1;
			}
		}
	}
	return -1;
}


int RTMP_Getfd()
{
	return ont_platform_tcp_socketfd(_pRtmp->m_sb.ont_sock);
}


/**
 * 发送RTMP数据包
 *
 * @param nPacketType 数据类型
 * @param data 存储数据内容
 * @param size 数据大小
 * @param nTimestamp 当前包的时间戳
 *
 * @成功则返回 1 , 失败则返回一个小于0的数
 */
int SendPacket(unsigned int nPacketType, unsigned char *data, unsigned int size,
               unsigned char *data1,
               unsigned int size1,
               unsigned int nTimestamp)
{
	RTMPPacket *packet;
	/*分配包内存和初始化,len为包体长度*/
	packet = (RTMPPacket *)malloc(RTMP_HEAD_SIZE + size + size1);
	memset(packet, 0, RTMP_HEAD_SIZE);
	/*包体内存*/
	packet->m_body = (char *)packet + RTMP_HEAD_SIZE;
	packet->m_nBodySize = size + size1;
	memcpy(packet->m_body, data, size);
	memcpy(packet->m_body + size, data1, size1);

	packet->m_hasAbsTimestamp = 0;
	packet->m_packetType = nPacketType; /*此处为类型有两种一种是音频,一种是视频*/
	packet->m_nInfoField2 = _pRtmp->m_stream_id;
	packet->m_nChannel = 0x07;

	packet->m_headerType = RTMP_PACKET_SIZE_LARGE;

	packet->m_nTimeStamp = nTimestamp;
	/*发送*/
	int nRet = 0;
	if (RTMP_IsConnected(_pRtmp)) {
		nRet = RTMP_SendPacket(_pRtmp, packet, FALSE, 1); /*TRUE为放进发送队列,FALSE是不放进发送队列,直接发送*/
	}
	/*释放内存*/
	free(packet);
	return nRet;
}


static int _rtmp_setchunksize(RTMP *rtmp, uint32_t size)
{
	RTMP *r = rtmp;
	RTMPPacket *packet = NULL;
	unsigned char *body = NULL;
	int i;
	int max_packet = RTMP_HEAD_SIZE + 4;
	packet = (RTMPPacket *)malloc(max_packet);
	memset(packet, 0, max_packet);
	packet->m_body = (char *)packet + RTMP_HEAD_SIZE;
	body = (unsigned char *)packet->m_body;
	i = 0;
	body[i++] = size >> 24 & 0xff;
	body[i++] = size >> 16 & 0xff;
	body[i++] = size >> 8 & 0xff;
	body[i++] = size & 0xff;

	packet->m_packetType = RTMP_PACKET_TYPE_CHUNK_SIZE;
	packet->m_nBodySize = 4;
	packet->m_nChannel = 0x04;
	packet->m_nTimeStamp = 0;
	packet->m_hasAbsTimestamp = 0;
	packet->m_headerType = RTMP_PACKET_SIZE_MEDIUM;
	packet->m_nInfoField2 = 0;
	int nRet =  RTMP_SendPacket(r, packet, FALSE, 0);
	if (nRet) {
		r->m_outChunkSize = size;
	}
	free(packet);
	return nRet == TRUE ? 0 : -1;
}


int Send_metadata()
{
	char body[1024] = { 0 };
	char *p = (char *)body;
	char *pend = p + sizeof(body);
	AVal avValue;
	RTMPPacket packet;
	int ret;

	/* to avoid warning by using type cast */
	avValue.av_val = (char *)"@setDataFrame";
	avValue.av_len = strlen(avValue.av_val);

	p = AMF_EncodeString(p, pend, &avValue);

	/* to avoid warning by using type cast */
	avValue.av_val = (char *)"onMetaData";
	avValue.av_len = strlen(avValue.av_val);
	p = AMF_EncodeString(p, pend, &avValue);

	AMFObject obj;
	memset(&obj, 0x00, sizeof(obj));


	AMF_Reset(&obj);

	AMFObjectProperty prop;
	memset(&prop, 0x00, sizeof(prop));

	/* to avoid warning by using type cast */
	avValue.av_val = (char *)"duration";
	avValue.av_len = strlen(avValue.av_val);
	AMFProp_SetName(&prop, &avValue);
	prop.p_type = AMF_NUMBER;
	prop.p_vu.p_number = 0;//metadata->duration;
	AMF_AddProp(&obj, &prop);

	/* to avoid warning by using type cast */
	avValue.av_val = (char *)"width";
	avValue.av_len = strlen(avValue.av_val);
	AMFProp_SetName(&prop, &avValue);
	prop.p_type = AMF_NUMBER;
	prop.p_vu.p_number = metaData.nWidth;
	AMF_AddProp(&obj, &prop);

	/* to avoid warning by using type cast */
	avValue.av_val = (char *)"height";
	avValue.av_len = strlen(avValue.av_val);
	AMFProp_SetName(&prop, &avValue);
	prop.p_type = AMF_NUMBER;
	prop.p_vu.p_number = metaData.nHeight;
	AMF_AddProp(&obj, &prop);


	/*avValue.av_val = "videorate";
	avValue.av_len = strlen(avValue.av_val);
	AMFProp_SetName(&prop, &avValue);
	prop.p_type = AMF_NUMBER;
	prop.p_vu.p_number = ((double)metadata->videoDataRate) / 1000;
	AMF_AddProp(&obj, &prop);*/

	/* to avoid warning by using type cast */
	avValue.av_val = (char *)"framerate";
	avValue.av_len = strlen(avValue.av_val);
	AMFProp_SetName(&prop, &avValue);
	prop.p_type = AMF_NUMBER;
	prop.p_vu.p_number = metaData.nFrameRate;
	AMF_AddProp(&obj, &prop);

	/* to avoid warning by using type cast */
	avValue.av_val = (char *)"videocodecid";
	avValue.av_len = strlen(avValue.av_val);
	AMFProp_SetName(&prop, &avValue);
	prop.p_type = AMF_NUMBER;
	prop.p_vu.p_number = 7; // metadata->videoCodecid;
	AMF_AddProp(&obj, &prop);

	/* to avoid warning by using type cast */
	avValue.av_val = (char *)"audiocodecid";
	avValue.av_len = strlen(avValue.av_val);
	AMFProp_SetName(&prop, &avValue);
	prop.p_type = AMF_NUMBER;
	prop.p_vu.p_number = 10;
	AMF_AddProp(&obj, &prop);

	/* to avoid warning by using type cast */
	avValue.av_val = (char *)"audiosamplerate";
	avValue.av_len = strlen(avValue.av_val);
	AMFProp_SetName(&prop, &avValue);
	prop.p_type = AMF_NUMBER;
	prop.p_vu.p_number = 48000;
	AMF_AddProp(&obj, &prop);


	p = AMF_Encode(&obj, p, pend);

	RTMPPacket_Reset(&packet);
	RTMPPacket_Alloc(&packet, p - body);

	memcpy(packet.m_body, body, p - body);

	packet.m_nBodySize = p - body;
	packet.m_packetType = RTMP_PACKET_TYPE_INFO;
	packet.m_nChannel = 0x03;
	packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
	packet.m_nInfoField2 = _pRtmp->m_stream_id;

	ret = RTMP_SendPacket(_pRtmp, &packet, FALSE, 0);

	RTMPPacket_Free(&packet);

	AMF_Reset(&obj);


	_rtmp_setchunksize(_pRtmp, 1200);

	return ret == TRUE ? 0 : -1;
}

/**
 * 发送视频的sps和pps信息
 *
 * @param pps 存储视频的pps信息
 * @param pps_len 视频的pps信息长度
 * @param sps 存储视频的pps信息
 * @param sps_len 视频的sps信息长度
 *
 * @成功则返回 1 , 失败则返回0
 */
int SendVideoSpsPps(unsigned char *pps, int pps_len, unsigned char *sps, int sps_len, unsigned ts)
{
	RTMPPacket *packet = NULL;  //rtmp包结构
	unsigned char *body = NULL;
	int i;
	RTMP_Log(RTMP_LOGDEBUG, "line %d send sps pps \n", __LINE__);
	packet = (RTMPPacket *)malloc(RTMP_HEAD_SIZE + 1024);
	//RTMPPacket_Reset(packet);//重置packet状态
	memset(packet, 0, RTMP_HEAD_SIZE + 1024);
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
	body[i++]   = 0xe1;
	body[i++] = (sps_len >> 8) & 0xff;
	body[i++] = sps_len & 0xff;
	memcpy(&body[i], sps, sps_len);
	i +=  sps_len;

	/*pps*/
	body[i++]   = 0x01;
	body[i++] = (pps_len >> 8) & 0xff;
	body[i++] = (pps_len) & 0xff;
	memcpy(&body[i], pps, pps_len);
	i +=  pps_len;

	packet->m_packetType = RTMP_PACKET_TYPE_VIDEO;
	packet->m_nBodySize = i;
	packet->m_nChannel = 0x07;
	packet->m_nTimeStamp = ts;
	packet->m_hasAbsTimestamp = 0;
	packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
	packet->m_nInfoField2 = _pRtmp->m_stream_id;

	/*调用发送接口*/
	int nRet = RTMP_SendPacket(_pRtmp, packet, FALSE, 0);
	free(packet);    //释放内存
	return nRet;
}

/**
 * 发送H264数据帧
 *
 * @param data 存储数据帧内容
 * @param size 数据帧的大小
 * @param bIsKeyFrame 记录该帧是否为关键帧
 * @param nTimeStamp 当前帧的时间戳
 *
 * @成功则返回 1 , 失败则返回0
 */
int SendH264Packet(unsigned char *data, unsigned int size, int bIsKeyFrame, unsigned int nTimeStamp)
{
	if (data == NULL || size < 1) {
		return false;
	}

	char body[9];


	int i = 0;
	if (bIsKeyFrame) {
		body[i++] = 0x17;// 1:Iframe  7:AVC
		body[i++] = 0x01;// AVC NALU
		body[i++] = 0x00;
		body[i++] = 0x00;
		body[i++] = 0x00;


		// NALU size
		body[i++] = size >> 24 & 0xff;
		body[i++] = size >> 16 & 0xff;
		body[i++] = size >> 8 & 0xff;
		body[i++] = size & 0xff;
		// NALU data




	} else {
		body[i++] = 0x27;// 2:Pframe  7:AVC
		body[i++] = 0x01;// AVC NALU
		body[i++] = 0x00;
		body[i++] = 0x00;
		body[i++] = 0x00;


		// NALU size
		body[i++] = size >> 24 & 0xff;
		body[i++] = size >> 16 & 0xff;
		body[i++] = size >> 8 & 0xff;
		body[i++] = size & 0xff;
		// NALU data


	}

	/* to avoid warning by using type cast */
	int bRet = SendPacket(RTMP_PACKET_TYPE_VIDEO, (unsigned char *)body, 9, data, size, nTimeStamp);


	return bRet;
}



int GetNextNaluTagPos(char *buf, int pos, int buf_size)
{
	int _pos = pos;
	while (_pos < buf_size - 3) {


		if ( buf[_pos] == 0x00 &&
		     buf[_pos + 1] == 0x00 &&
		     buf[_pos + 2] == 0x00 &&
		     buf[_pos + 3] == 0x01   ) {
			return _pos;
		}
		_pos++;
	}
	return _pos;
}


/**
 * 从内存中读取出一个Nal单元
 *
 * @param nalu 存储nalu数据
 * @param
 *                  2个参数功能：
 *                  uint8_t *buf：外部数据送至该地址
 *                  int buf_size：外部数据大小
 *                  返回值：成功读取的内存大小
 * @成功则返回 1 , 失败则返回0
 */
int ReadOneNaluFromGM(NaluUnit &nalu, char *buf, int buf_size)
{
	unsigned int naltail_pos = nalhead_pos;
#if 0 /* unused variable */
	int ret;
#endif
	int nalustart;//nal的开始标识符是几个00
	nalu.size = 0;
	int lastHeadByte = buf_size - 4;

	while (1) {
		/* to avoid warning by using type cast,
		 * remark: signed cast to unsigned */
		while (naltail_pos < (unsigned int)buf_size - 4) {


			if (buf[naltail_pos++] == 0x00 &&
			    buf[naltail_pos++] == 0x00) {
				//if (buf[naltail_pos++] == 0x01)
				//{
				//  nalustart=3;
				//    printf("line %d nalustart 3 \n", __LINE__);
				//  goto gotnal;
				//}
				//else
				//{

				//naltail_pos--;
				if (buf[naltail_pos++] == 0x00 &&
				    buf[naltail_pos++] == 0x01) {
					nalustart = 4;
					goto gotnal;
				} else {
					continue;
				}
				//}
			} else {
				continue;
			}


		}
gotnal:
		/**
		 *special case1:parts of the nal lies in a m_pFileBuf and we have to read from buffer
		 *again to get the rest part of this nal
		 */

		nalu.type = buf[nalhead_pos] & 0x1f;
		/* to avoid warning by using type cast,
		 * remark: signed cast to unsigned */
		if (naltail_pos < (unsigned int)lastHeadByte) {
			nalu.size = naltail_pos - nalhead_pos - nalustart;
		} else {
			nalu.size = naltail_pos - nalhead_pos;
		}


		if (nalu.type == 0x06) {
			//nalhead_pos=naltail_pos;
			//continue;
		}

		/* to avoid warning by using type cast,
		 * remark: int cast to unsigned int */
		nalu.data = (unsigned char *)buf + nalhead_pos;
		if (naltail_pos < (unsigned int)lastHeadByte) {

			nalhead_pos = naltail_pos;
		} else {

			nalhead_pos = 4;
		}


		return TRUE;
	}
	return FALSE;
}

/**
 * 将内存中的一段H.264编码的视频数据利用RTMP协议发送到服务器
 *
 * @param
 *                  uint8_t *buf：外部数据送至该地址
 *                  int buf_size：外部数据大小
 *                  返回值：成功读取的内存大小
 * @成功则返回1 , 失败则返回0
 */
int GM8136_SPS(char *buf, int pos, int buf_size)
{
#if 0 /* unused variables */
	int ret;
	uint32_t now, last_update;
#endif
	int _pos = 0;


	//NaluUnit naluUnit;
	//skip the first 4 bytes
	pos += 4;
	_pos = GetNextNaluTagPos(buf, pos, buf_size);

	metaData.nSpsLen = _pos - pos;
	if (!metaData.Sps) {
		metaData.Sps = (unsigned char *)malloc(128);
	}

	memcpy(metaData.Sps, buf + pos, metaData.nSpsLen);
	//printf("line %d sps size %d\n", __LINE__, metaData.nSpsLen);
	//_LogHex(metaData.Sps, metaData.nSpsLen);
	_pos += 4; //skip the tag
	pos = _pos;

	_pos = GetNextNaluTagPos(buf, pos, buf_size);


	metaData.nPpsLen = _pos - pos;

	if (!metaData.Pps) {
		metaData.Pps = (unsigned char *)malloc(64);
	}

	memcpy(metaData.Pps, buf + pos, metaData.nPpsLen);

	//printf("line %d ppssize %d\n", __LINE__, metaData.nPpsLen);
	//_LogHex(metaData.Pps, metaData.nPpsLen);

	// 解码SPS,获取视频图像宽、高信息
#if 0 /* unused variables */
	int width = 0, height = 0, fps = 0;
#endif
	//h264_decode_sps(metaData.Sps,metaData.nSpsLen,width,height,fps);

	//if(fps)
	//  metaData.nFrameRate = fps;
	//else
	metaData.nFrameRate = 30;
	metaData.nWidth = 1280;
	metaData.nHeight = 720;
	return _pos;

}

int GM8136_InitWidth(unsigned int  width, unsigned  int height, unsigned  int fps)
{
	metaData.nWidth = width;
	metaData.nHeight = height;
	metaData.nFrameRate = fps;

	RTMP_Log(RTMP_LOGDEBUG,  "width=%d\n", metaData.nWidth);
	RTMP_Log(RTMP_LOGDEBUG, "height=%d\n", metaData.nHeight);
	RTMP_Log(RTMP_LOGDEBUG, "fps=%d\n", metaData.nFrameRate);
	return 0;

}

#if 0 /* it's definition here, i guess */
extern int video_first = 1;
#endif
int video_first = 1;

/**
 * 将内存中的一段H.264编码的视频数据利用RTMP协议发送到服务器
 *
 * @param read_buffer 回调函数，当数据不足的时候，系统会自动调用该函数获取输入数据。
 *                  2个参数功能：
 *                  uint8_t *buf：外部数据送至该地址
 *                  int buf_size：外部数据大小
 *                  返回值：成功读取的内存大小
 * @成功则返回1 , 失败则返回0
 */
int GM8136_Send(char *buf, int buf_size, int keyframe, unsigned int tick)
{
#if 0 /* unused variables */
	uint32_t now, last_update;
	int bKeyframe ;
#endif
	RTMP_Log(RTMP_LOGDEBUG, "line %d, size %d, ts %d\n", __LINE__, buf_size, tick);
	//_LogHex(buf, buf_size);
	int pos = 0;
	do {
		if (keyframe) {
			pos = GM8136_SPS(buf, 0, buf_size);
			if (video_first) {
				video_first = 0;
				Send_metadata();
				SendVideoSpsPps(metaData.Pps, metaData.nPpsLen, metaData.Sps, metaData.nSpsLen, tick);
			}
			/*TODO need compare if the new SPS PPS are the same as before*/
		}
		pos += 4;
		//printf("line %d, size %d ts %d\n", __LINE__, buf_size, tick);
		/* to avoid warning by using type cast */
		SendH264Packet((unsigned char *)buf + pos, buf_size - pos, keyframe, tick);
	} while (0);
#if 0 /* defined but not used */
end:
#endif
	//free(metaData.Sps);
	//free(metaData.Pps);
	return TRUE;
}


char audio_tag;
int GM8136_SendAACSpec(char *data, int len, unsigned int ts)
{
	RTMPPacket *packet;
	unsigned char *body;
	packet = (RTMPPacket *)malloc(RTMP_HEAD_SIZE + len + 2);
	memset(packet, 0, RTMP_HEAD_SIZE);

	packet->m_body = (char *)packet + RTMP_HEAD_SIZE;
	body = (unsigned char *)packet->m_body;

	//AF 00 + AAC sequence header
	body[0] = audio_tag;
	body[1] = 0x00;
	//memcpy(&body[2],spec_buf,len); //spec_buf是AAC sequence header数据
	body[2] = data[0];//0x12;
	body[3] = data[1];//0x10;
	packet->m_packetType = RTMP_PACKET_TYPE_AUDIO;
	packet->m_nBodySize = len + 2;
	packet->m_nChannel = 0x06;
	packet->m_nTimeStamp = ts;
	packet->m_hasAbsTimestamp = 0;
	packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
	packet->m_nInfoField2 = _pRtmp->m_stream_id;

	//调用发送接口
	RTMP_SendPacket(_pRtmp, packet, FALSE, 0); //TRUE
	free(packet);
	return 0;
}


int SendAACPacket(unsigned char *data, unsigned int size, unsigned int nTimeStamp )
{

	if (size > 0) {
		RTMPPacket *packet;
		unsigned char *body;

		packet = (RTMPPacket *)malloc(RTMP_HEAD_SIZE + size + 2 - 7);
		memset(packet, 0, RTMP_HEAD_SIZE);

		packet->m_body = (char *)packet + RTMP_HEAD_SIZE;
		body = (unsigned char *)packet->m_body;

		/*AF 01 + AAC RAW data*/
		body[0] = audio_tag;
		body[1] = 0x01;//0x01;
		memcpy(&body[2], data + 7, size - 7);

		///SendPacket(RTMP_PACKET_TYPE_AUDIO,body,2+size,nTimeStamp);

		packet->m_packetType = RTMP_PACKET_TYPE_AUDIO;
		packet->m_nBodySize = size + 2 - 7;
		packet->m_nChannel = 0x06;
		packet->m_nTimeStamp = nTimeStamp;
		packet->m_hasAbsTimestamp = 0;
		packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
		packet->m_nInfoField2 = _pRtmp->m_stream_id;

		/*调用发送接口*/
		RTMP_SendPacket(_pRtmp, packet, TRUE, 0/*TRUE*/);
		free(packet);
	}
	return true;
}

#if 0 /* defined but not used */
static FILE *_wfd = NULL;
#endif

#if 0 /* it's definition here, i guess */
extern int audio_first = 0;
#endif
int audio_first = 0;

int GM8136_SendAAC(unsigned char *data, unsigned int size, unsigned int audio_tick)
{
	/*get the adts size*/
	int index = 0;
	int frame_size = 0;

#if 0
	if (_wfd == NULL) {
		_wfd = fopen("local.aac", "w");
	}
	fwrite(data, size, 1, _wfd);
	fflush(_wfd);
#endif


	RTMP_Log(RTMP_LOGDEBUG, "line %d, size %d ts %d\n", __LINE__, size, audio_tick);

	//_LogHex(data, size);

	if (audio_first) {
		audio_first = 0;
		audio_tag  = rtmp_make_audio_headerTag(10, 3, 1, 3); //aac not care the sample rate.
		//dio_tag  = 0xaf;
		int profile = ((data[2] & 0xc0) >> 6) + 1;
		int sample_rate = (data[2] & 0x3c) >> 2;
		int channel = ((data[2] & 0x1) << 2) | ((data[3] & 0xc0) >> 6);
		char config[2];
		config[0] = (profile << 3) | ((sample_rate & 0xe) >> 1);
		config[1] = ((sample_rate & 0x1) << 7) | (channel << 3);
		RTMP_Log(RTMP_LOGDEBUG, "tag 0x%x, profile %d, rate %d, channel %d, config[0]=0x%x config[1]=0x%x\n",
		         audio_tag,
		         profile,
		         sample_rate,
		         channel,
		         config[0], config[1]);
		GM8136_SendAACSpec(config, 2, audio_tick);
	}
	/*send correctly*/
#if 0
	put_bits(&pb, 12, 0xfff);   /* syncword */
	put_bits(&pb, 1, 0);        /* ID */
	put_bits(&pb, 2, 0);        /* layer */
	put_bits(&pb, 1, 1);        /* protection_absent */
	put_bits(&pb, 2, ctx->objecttype); /* profile_objecttype */
	put_bits(&pb, 4, ctx->sample_rate_index);
	put_bits(&pb, 1, 0);        /* private_bit */
	put_bits(&pb, 3, ctx->channel_conf); /* channel_configuration */
	put_bits(&pb, 1, 0);        /* original_copy */
	put_bits(&pb, 1, 0);        /* home */

	/* adts_variable_header */
	put_bits(&pb, 1, 0);        /* copyright_identification_bit */
	put_bits(&pb, 1, 0);        /* copyright_identification_start */
	put_bits(&pb, 13, ADTS_HEADER_SIZE + size + pce_size); /* aac_frame_length */
	put_bits(&pb, 11, 0x7ff);   /* adts_buffer_fullness */
	put_bits(&pb, 2, 0);        /* number_of_raw_data_blocks_in_frame */
#endif


	/*the packet may have two frame*/
	do {
		index += frame_size;
		frame_size = (((int)(data[index + 3] & 0x3)) << 11) | ( ((int)(data[index + 4])) << 3) | (((int)(data[index + 5] & 0xe0)) >> 5);

		/* to avoid warning by using type cast,
		 * remark: signed cast to unsigned */
		if ((unsigned int)index + (unsigned int)frame_size > size) {
			RTMP_Log(RTMP_LOGDEBUG, "index %d, frame_size %d, size %d\n", index, frame_size, size);
			abort();
		}
		if (index > 0) {
			SendAACPacket(data + index, frame_size, audio_tick); /*need caculate the ts if sended here*/
		} else {
			SendAACPacket(data + index, frame_size, audio_tick);
		}
	} while ((unsigned int)index + (unsigned int)frame_size < size); /* to avoid warning by using type cast, remark: signed cast to unsigned */

	return 0;
}



int RTMP_CheckRcv()
{
	rtmp_check_rcv_handler(_pRtmp);
	return 0;
}





