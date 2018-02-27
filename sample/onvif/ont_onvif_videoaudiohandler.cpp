#include <stdlib.h>

#include "rtmp_log.h"
#include "rtmp_sys.h"

#include "ont_onvif_videoandiohandler.h"
#include "h264_parse.h"

#define VIDEO_SINK_RECEIVE_BUFFER_SIZE 1000000
#define AUDIO_SINK_RECEIVE_BUFFER_SIZE 1000


ONTVideoAudioSink* ONTVideoAudioSink::createNew( MediaSubsession& subsession,
    ont_onvif_playctx *ctx,
	// identifies the kind of data that's being received
    char const* streamId) // identifies the stream itself (optional)
{
	if (strcmp(subsession.mediumName(), "audio") && strcmp(subsession.mediumName(), "video"))
	{
		return NULL;
	}
	return new ONTVideoAudioSink(subsession, ctx, streamId);
}

ONTVideoAudioSink::ONTVideoAudioSink(MediaSubsession& subsession,
	ont_onvif_playctx *ctx,
    // identifies the kind of data that's being received
    char const* streamId):
    MediaSink(*((UsageEnvironment*)ctx->play_env)),
    fSubsession(subsession),
    p_extra(NULL)
{
    u_int8_t* sps = NULL; unsigned spsSize = 0;
    u_int8_t* pps = NULL; unsigned ppsSize = 0;
    unsigned numSPropRecords;
	char const *parameter = NULL;
	RTMPMetadata *meta=NULL;
    this->ctx = ctx;
    _rtmp_mode_ctx.onvif_ctx = ctx;
    _rtmp_mode_ctx.type = LIVE_MODE;

    meta = &ctx->meta;

    sourcecodec = -1;
    if (!strcmp(subsession.mediumName(), "video") && !strcmp(subsession.codecName(), "H264"))
    {
		parameter = subsession.fmtp_spropparametersets();

		if (parameter != NULL && parameter[0] != '\0')
		{
			SPropRecord* sPropRecords = parseSPropParameterSets(parameter, numSPropRecords);
			for (unsigned i = 0; i < numSPropRecords; ++i) {
				if (sPropRecords[i].sPropLength == 0)
					continue; // bad data
				u_int8_t nal_unit_type = (sPropRecords[i].sPropBytes[0]) & 0x1F;
				if (nal_unit_type == 7/*SPS*/) {
					sps = sPropRecords[i].sPropBytes;
					spsSize = sPropRecords[i].sPropLength;
				}
				else if (nal_unit_type == 8/*PPS*/) {
					pps = sPropRecords[i].sPropBytes;
					ppsSize = sPropRecords[i].sPropLength;
				}
			}
			if (sps){
				memcpy(this->latestSps, sps, spsSize);
				this->sps_len = spsSize;
				spspps_changed = 1;
			}
			if (pps){
				memcpy(this->latestPps, pps, ppsSize);
				this->pps_len = ppsSize;
				spspps_changed = 1;
			}
			delete[]sPropRecords;
		}

		if (meta)
		{
			meta->videoCodecid = 7;
		}
        sourcecodec = ONVIF_CODEC_H264;
		fReceiveBuffer = new u_int8_t[VIDEO_SINK_RECEIVE_BUFFER_SIZE + 16]; //reserve 16 bytes
        buffersize = VIDEO_SINK_RECEIVE_BUFFER_SIZE;
    }
    else if (!strcmp(subsession.mediumName(), "audio"))
    {
        if (!strcmp(subsession.codecName(), "MPEG4-GENERIC"))
        {
            unsigned int _i_extra;
            uint8_t      *_p_extra;
            sourcecodec = ONVIF_CODEC_MPEG4A;

			if (subsession.fmtp_config())
			{
				if ((_p_extra = parseGeneralConfigStr(subsession.fmtp_config(),
					_i_extra)))
				{
					p_extra = new char[_i_extra];
					memcpy(p_extra, _p_extra, _i_extra);
					i_extra = _i_extra;
					delete[] _p_extra;
				}
			}
			if (meta)
			{
				meta->hasAudio = TRUE;
				meta->audioCodecid = 10;
				meta->audioSampleRate = 44100; // not care thesample rate
				audioTag = rtmp_make_audio_headerTag(meta->audioCodecid, 3, 1, 1);
			}

        }
        fReceiveBuffer = new u_int8_t[AUDIO_SINK_RECEIVE_BUFFER_SIZE+16]; //reserve 16 bytes
        buffersize = AUDIO_SINK_RECEIVE_BUFFER_SIZE;
    }
    else
    {
    }

    sendmeta = 0;
    sendaudioheader = 0;
    fStreamId = strDup(streamId);

}

ONTVideoAudioSink::~ONTVideoAudioSink() {
    if (fReceiveBuffer)
    {
        delete[] fReceiveBuffer;
    }
    if (fStreamId)
    {
        delete[] fStreamId;
    }
    if (p_extra)
    {
        delete[] p_extra;
    }
}

uint32_t  next4Bytes(const unsigned char * data, unsigned size)
{
    if (size < 4)
    {
        return 0xffffffff;
    }
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}


int ONTVideoAudioSink::h264parse(const unsigned char *buf, unsigned size, unsigned &offset)
{
    uint32_t bytes4 = 0;
    unsigned parseSize = 0;

	/*skip the first 4 or 3 bytes*/
    bytes4 = next4Bytes(&buf[offset], size - parseSize-offset);
    if (bytes4 == 0x00000001){
        offset += 4;
    }
    else if ((bytes4 & 0xFFFFFF00) == 0x00000100){
        offset += 3;
    }

    bytes4 = next4Bytes(&buf[offset], size - parseSize - offset);

    while (bytes4 != 0x00000001 && (bytes4 & 0xFFFFFF00) != 0x00000100) {
        parseSize++;
        if (offset + parseSize>= size)
        {
            break;
        }
        bytes4 = next4Bytes(&buf[offset + parseSize], size - parseSize - offset);
    }

    return parseSize;
}

const char h264_delimiter[] = { 0x01, 0, 0, 0 };

int packFLVvideodata(const unsigned char *in, unsigned inSize, unsigned char* out, unsigned outSize, unsigned keyFrame)
{
    unsigned char *pFrameBuf = out;
    unsigned body_size = 0;
    uint32_t h264_nal_leng = 0;
    if (keyFrame)
    {
        pFrameBuf[0] = 0x17;

    }
    else
    {
        pFrameBuf[0] = 0x27;
    }
    body_size += 1;
    memcpy(pFrameBuf + body_size, h264_delimiter, 4);
    body_size += 4;
    h264_nal_leng = inSize;

    pFrameBuf[body_size] = h264_nal_leng >> 24 & 0xff;
    body_size++;
    pFrameBuf[body_size] = h264_nal_leng >> 16 & 0xff;
    body_size++;
    pFrameBuf[body_size] = h264_nal_leng >> 8 & 0xff;
    body_size++;
    pFrameBuf[body_size] = h264_nal_leng & 0xff;
    body_size++;

    memcpy(pFrameBuf + body_size, in, h264_nal_leng);
    body_size += h264_nal_leng;
	if (body_size == 9 || h264_nal_leng == 9)
	{
		printf("the length is %d, %d", body_size, h264_nal_leng);
	}
    return body_size;

}

int ONTVideoAudioSink::handleVideoFrame(unsigned frameSize, unsigned numTruncatedBytes, unsigned int deltaTs)
{
	ont_onvif_playctx *playctx = this->ctx;
    unsigned int offset = 0;
    unsigned int parseSize = 0;
    unsigned char *buf = this->fReceiveBuffer;


    if (playctx->tempBuf == NULL){
        playctx->tempBuf = (unsigned char*)malloc(1024);
        playctx->tempBufSize = 1024;
    }
    do
    {
        parseSize = this->h264parse(buf, frameSize, offset);

        /*check the buffer */
        if ((buf[offset] & 0x0f) == 0x07){ /*sps*/

			if (this->sps_len == parseSize && memcmp(this->latestSps, &buf[offset], parseSize) == 0)
			{
				/**/
			}
			else
			{
				if (parseSize <= sizeof(latestSps))
				{
					memcpy(this->latestSps, &buf[offset], parseSize);
					this->sps_len = parseSize;
					spspps_changed = 1;
				}
				else
				{
					/*not use the sps*/
				}
			}
        }
        else if ((buf[offset] & 0x0f) == 0x08) /*pps*/{

			if (this->pps_len == parseSize && memcmp(this->latestPps, &buf[offset], parseSize) == 0)
			{
				/**/
			}
			else
			{
				if (parseSize < sizeof(latestPps))
				{
					memcpy(this->latestPps, &buf[offset], parseSize);
					this->pps_len = parseSize;
					spspps_changed = 1;
				}
				else
				{
					/*not use the pps*/
				}
			}
        }
        else if ((buf[offset] & 0x0f) == 0x05) /*I frame*/{
            RTMPVideoAudioCtl *ctl = new RTMPVideoAudioCtl();
            ctl->isKeyFram = 1;

            RTMPMetaSpsPpsData *mspd =NULL;
            if (sendmeta == 0 || playctx->sendmeta[0] ==0)
            {
                sendmeta = 1;
                playctx->meta.duration = 0;
                if (playctx->meta.width == 0)
                {
                    h264_decode_seq_parameter_set(latestSps, this->sps_len, playctx->meta.width, playctx->meta.height);
                }
            //    rtmp_send_metadata(playctx->rtmp_client, &playctx->meta);
            }
			if (spspps_changed)
			{
				spspps_changed = 0;
				mspd = new RTMPMetaSpsPpsData();
				mspd->sps_len = this->sps_len > sizeof(mspd->latestSps) ? sizeof(mspd->latestSps) : this->sps_len;
				memcpy(mspd->latestSps, this->latestSps, mspd->sps_len);
				mspd->pps_len = this->pps_len > sizeof(mspd->latestPps) ? sizeof(mspd->latestPps) : this->pps_len;
				memcpy(mspd->latestPps, this->latestPps, mspd->pps_len);
			}
			//rtmp_send_spspps(playctx->rtmp_client, (unsigned char*)this->latestSps, this->sps_len, (unsigned char *)this->latestPps, this->pps_len, deltaTs);
            if (playctx->tempBufSize < parseSize + 9) /*need reserve 9 bytes for FLV video tag*/
            {
                free(playctx->tempBuf);
                playctx->tempBuf = (unsigned char*)malloc(parseSize + 9);
                playctx->tempBufSize = parseSize + 9;
            }
            int outsize = packFLVvideodata(&buf[offset], parseSize, playctx->tempBuf, playctx->tempBufSize, TRUE);
            RTMPPackEnqueue(&_rtmp_mode_ctx, playctx->tempBuf, outsize, deltaTs, CODEC_H264, mspd, ctl);
        }
        else { /*p frame, etc.*/
			if (!playctx->key_send)
			{
				offset += parseSize;
				continue;
			}
            RTMPVideoAudioCtl *ctl = new RTMPVideoAudioCtl();
            ctl->isKeyFram = 0;

            if (playctx->tempBufSize < parseSize + 9) /*need reserve 9 bytes for FLV video tag*/
            {
                free(playctx->tempBuf);
                playctx->tempBuf = (unsigned char*)malloc(parseSize + 9);
                playctx->tempBufSize = parseSize + 9;
            }
            int outsize = packFLVvideodata(&buf[offset], parseSize, playctx->tempBuf, playctx->tempBufSize, FALSE);
            RTMPPackEnqueue(&_rtmp_mode_ctx, playctx->tempBuf, outsize, deltaTs, CODEC_H264, NULL, ctl);
        }
        offset += parseSize;
    } while (offset < frameSize);
    return 0;
}

int ONTVideoAudioSink::handleAACFrame(unsigned frameSize, unsigned numTruncatedBytes, unsigned int deltaTs)
{
	ont_onvif_playctx *playctx = this->ctx;
    /*
	if (!this->sendmeta)
	{
		return 0 ;
	}
    */

    if (sendaudioheader == 0 || playctx->sendmeta[1] == 0)
    {
        RTMPVideoAudioCtl *ctl = new RTMPVideoAudioCtl();
        ctl->isAudioHeader = 1;

        RTMPAudioHeaderData  *ahd  =   new RTMPAudioHeaderData();
        ahd->audioTag = audioTag;
        sendaudioheader = 1;
        unsigned char audio_data[3];
        audio_data[0] = 0x00;
        audio_data[1] = p_extra[0];
        audio_data[2] = p_extra[1];

        ahd->audiotype = RTMP_PACKET_SIZE_LARGE;
        RTMPPackEnqueue(&_rtmp_mode_ctx, audio_data, 3, 0, CODEC_MPEG4A, ahd, ctl);
        //rtmp_send_audiodata(playctx->rtmp_client, audioTag, audio_data, 3, 0, RTMP_PACKET_SIZE_LARGE);

    }
    RTMPVideoAudioCtl *ctl = new RTMPVideoAudioCtl();
    ctl->isAudioHeader = 0;

    RTMPAudioHeaderData  *ahd  =   new RTMPAudioHeaderData();
    ahd->audioTag = audioTag;
    fReceiveBuffer[0] = 0x01;
    ahd->audiotype = (deltaTs == 0) ? RTMP_PACKET_SIZE_LARGE : RTMP_PACKET_SIZE_MEDIUM;
    RTMPPackEnqueue(&_rtmp_mode_ctx, fReceiveBuffer, frameSize + 1, deltaTs, CODEC_MPEG4A, ahd, ctl);
    //rtmp_send_audiodata(playctx->rtmp_client, audioTag, fReceiveBuffer, frameSize + 1, deltaTs, deltaTs == 0 ? RTMP_PACKET_SIZE_LARGE : RTMP_PACKET_SIZE_MEDIUM);

    return 0;
}

#if defined(WIN32) | defined(_WIN32)
void gettimeofday(struct timeval *tv, void*);
#endif


void ONTVideoAudioSink::afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
struct timeval presentationTime, unsigned durationInMicroseconds) {
    ONTVideoAudioSink* sink = (ONTVideoAudioSink*)clientData;
    unsigned long ts = (presentationTime.tv_usec / 1000 + presentationTime.tv_sec * 1000);
	struct timeval tv;
	gettimeofday(&tv, NULL);
	unsigned long ts2 = (tv.tv_usec / 1000 + tv.tv_sec * 1000);

	ont_onvif_playctx *playctx = (ont_onvif_playctx*)sink->ctx;
	if (!playctx)
	{
        /*
        printf("get %d ", frameSize);
        if (playctx) { printf("channel %d", playctx->channel);}
        printf("\n");
        */

		sink->continuePlaying();
		return;
	}

    playctx->last_rcvts = ts2;

    if (!playctx->push_model)
    {
		playctx->startts = 0;
		sink->continuePlaying();
		return;
    }

	playctx->last_sndts = ts2;

	if (playctx->startts == 0)
    {
		playctx->startts = ts;
    } 

	unsigned deltaTs = ts - playctx->startts;

    if (sink->sourcecodec == ONVIF_CODEC_H264) //video
    {
        sink->handleVideoFrame(frameSize, numTruncatedBytes, deltaTs);
    }
	else if (sink->sourcecodec == ONVIF_CODEC_MPEG4A)
	{
		sink->handleAACFrame(frameSize, numTruncatedBytes, deltaTs);
	}

    sink->continuePlaying();
}



void ONTVideoAudioSink::closure(void* clientData)
{
	ONTVideoAudioSink* sink = (ONTVideoAudioSink*)clientData;
	ont_onvif_playctx *playctx = (ont_onvif_playctx*)sink->ctx;
	playctx->state = 1;
    RTMP_Log(RTMP_LOGDEBUG, "close");
	sink->onSourceClosure();
}

#define DEBUG_PRINT_EACH_RECEIVED_FRAME 1

Boolean ONTVideoAudioSink::continuePlaying() {
    if (fSource == NULL) return False; // sanity check (should not happen)

    // Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
    if (this->sourcecodec == ONVIF_CODEC_MPEG4A)
    {
        fSource->getNextFrame(fReceiveBuffer+1, buffersize-1,
            afterGettingFrame, this,
			closure, this);
    }
    else
    {
        fSource->getNextFrame(fReceiveBuffer, buffersize,
            afterGettingFrame, this,
			closure, this);
    }
    return True;
}
