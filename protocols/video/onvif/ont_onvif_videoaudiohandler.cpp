#include <stdlib.h>

#include "librtmp/log.h"
#include "librtmp/rtmp_sys.h"

#include "ont_onvif_videoandiohandler.h"
#include "h264_parse.h"

#define VIDEO_SINK_RECEIVE_BUFFER_SIZE 1000000
#define AUDIO_SINK_RECEIVE_BUFFER_SIZE 1000

ONTVideoAudioSink* ONTVideoAudioSink::createNew(UsageEnvironment& env,
    MediaSubsession& subsession,
    // identifies the kind of data that's being received
    char const* streamId) // identifies the stream itself (optional)
{

    return new ONTVideoAudioSink(env, subsession, streamId);
}

ONTVideoAudioSink::ONTVideoAudioSink(UsageEnvironment& env, MediaSubsession& subsession,
    // identifies the kind of data that's being received
    char const* streamId):
    MediaSink(env),
    p_extra(NULL),
    fSubsession(subsession) 
{
    u_int8_t* sps = NULL; unsigned spsSize = 0;
    u_int8_t* pps = NULL; unsigned ppsSize = 0;
    unsigned numSPropRecords;
	RTMPMetadata &meta = ((ont_onvif_playctx*)env.livertmp)->meta;
    sourcecodec = -1;
    if (!strcmp(subsession.mediumName(), "video") && !strcmp(subsession.codecName(), "H264"))
    {

        SPropRecord* sPropRecords = parseSPropParameterSets(subsession.fmtp_spropparametersets(), numSPropRecords);
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
        }
        if (pps){
            memcpy(this->latestPps, pps, ppsSize);
            this->pps_len = ppsSize;
        }
        delete []sPropRecords;
        meta.videoCodecid = 7;

        sourcecodec = ONVIF_CODEC_H264;
        fReceiveBuffer = new u_int8_t[VIDEO_SINK_RECEIVE_BUFFER_SIZE];
        buffersize = VIDEO_SINK_RECEIVE_BUFFER_SIZE;
    }
    else if (!strcmp(subsession.mediumName(), "audio"))
    {
        if (!strcmp(subsession.codecName(), "MPEG4-GENERIC"))
        {
            unsigned int _i_extra;
            uint8_t      *_p_extra;
            sourcecodec = ONVIF_CODEC_MPEG4A;

            if ((_p_extra = parseGeneralConfigStr(subsession.fmtp_config(),
                _i_extra)))
            {
                p_extra = new char[_i_extra];
                memcpy(p_extra, _p_extra, _i_extra);
                i_extra = _i_extra;
                delete[] _p_extra;
            }
            meta.hasAudio = TRUE;
            meta.audioCodecid = 10;
            meta.audioSampleRate = 44100;
            audioTag = rtmp_make_audio_headerTag(meta.audioCodecid, 3, 1, 1);
        }
        fReceiveBuffer = new u_int8_t[AUDIO_SINK_RECEIVE_BUFFER_SIZE];
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

uint32_t  next4Byptes(const unsigned char * data, unsigned size)
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
    bytes4 = next4Byptes(&buf[offset], size - parseSize-offset);
    if (bytes4 == 0x00000001){
        offset += 4;
    }
    else if ((bytes4 & 0xFFFFFF00) == 0x00000100){
        offset += 3;
    }
    /*skip the first 4 or 3 bytes*/
    bytes4 = next4Byptes(&buf[offset], size - parseSize - offset);

    while (bytes4 != 0x00000001 && (bytes4 & 0xFFFFFF00) != 0x00000100) {
        parseSize++;
        if (offset + parseSize>= size)
        {
            break;
        }
        bytes4 = next4Byptes(&buf[offset + parseSize], size - parseSize - offset);
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

    return body_size;

}

int ONTVideoAudioSink::handleVideoFrame(unsigned frameSize, unsigned numTruncatedBytes, unsigned int deltaTs)
{
	ont_onvif_playctx *rtmp = (ont_onvif_playctx*)this->envir().livertmp;
    unsigned int offset = 0;
    unsigned int parseSize = 0;
    unsigned char *buf = this->fReceiveBuffer;
    if (rtmp->tempBuf == NULL){
        rtmp->tempBuf = (unsigned char*)malloc(1024);
        rtmp->tempBufSize = 1024;
    }
    do
    {
        parseSize = this->h264parse(buf, frameSize, offset);
        /*check the buffer */
        if ((buf[offset] & 0x0f) == 0x07){ /*sps*/
            memcpy(this->latestSps, &buf[offset], parseSize);
            this->sps_len = parseSize;
        }
        else if ((buf[offset] & 0x0f) == 0x08) /*pps*/{
            memcpy(this->latestPps, &buf[offset], parseSize);
            this->pps_len = parseSize;
        }
        else if ((buf[offset] & 0x0f) == 0x05) /*I frame*/{
            if (sendmeta == 0)
            {
                sendmeta = 1;
                rtmp->meta.duration = 0;
                if (rtmp->meta.width == 0)
                {
                    h264_decode_seq_parameter_set(latestSps, this->sps_len, rtmp->meta.width, rtmp->meta.height);
                }
                rtmp_send_metadata(rtmp->rtmp_client, &rtmp->meta);

            }
			rtmp_send_spspps(rtmp->rtmp_client, (unsigned char*)this->latestSps, this->sps_len, (unsigned char *)this->latestPps, this->pps_len, deltaTs);
            if (rtmp->tempBufSize < parseSize + 9) /*need reserve 9 bytes for FLV video tag*/
            {
                free(rtmp->tempBuf);
                rtmp->tempBuf = (unsigned char*)malloc(parseSize + 9);
                rtmp->tempBufSize = parseSize + 9;
            }
            int outsize = packFLVvideodata(&buf[offset], parseSize, rtmp->tempBuf, rtmp->tempBufSize, TRUE);
            if (rtmp_send_videodata(rtmp->rtmp_client, rtmp->tempBuf, outsize, deltaTs, TRUE) < 0){
                /*send error, need reopen*/
                RTMP_Log(RTMP_LOGDEBUG, "need closed");
                rtmp->eventLoopWatchVariable = 1;
            }
        }
        else { /*p frame, etc.*/
            if (rtmp->tempBufSize < parseSize + 9) /*need reserve 9 bytes for FLV video tag*/
            {
                free(rtmp->tempBuf);
                rtmp->tempBuf = (unsigned char*)malloc(parseSize + 9);
                rtmp->tempBufSize = parseSize + 9;
            }
            int outsize = packFLVvideodata(&buf[offset], parseSize, rtmp->tempBuf, rtmp->tempBufSize, FALSE);
            if (rtmp_send_videodata(rtmp->rtmp_client, rtmp->tempBuf, outsize, deltaTs, FALSE) < 0){
                RTMP_Log(RTMP_LOGDEBUG, "need closed");
                rtmp->eventLoopWatchVariable = 1;
            }
        }
        offset += parseSize;
    } while (offset < frameSize);
    return 0;
}

int ONTVideoAudioSink::handleAACFrame(unsigned frameSize, unsigned numTruncatedBytes, unsigned int deltaTs)
{
	ont_onvif_playctx *rtmp = (ont_onvif_playctx*)this->envir().livertmp;
    if (sendaudioheader == 0)
    {
        sendaudioheader = 1;
        unsigned char audio_data[3];
        audio_data[0] = 0x00;
        audio_data[1] = p_extra[0];
        audio_data[2] = p_extra[1];
        rtmp_send_audiodata(rtmp->rtmp_client, audioTag, audio_data, 3, 0, RTMP_PACKET_SIZE_LARGE);
    }
    fReceiveBuffer[0] = 0x01;
    rtmp_send_audiodata(rtmp->rtmp_client, audioTag, fReceiveBuffer, frameSize + 1, deltaTs, deltaTs == 0 ? RTMP_PACKET_SIZE_LARGE : RTMP_PACKET_SIZE_MEDIUM);

    return 0;
}



void ONTVideoAudioSink::afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
struct timeval presentationTime, unsigned durationInMicroseconds) {
    ONTVideoAudioSink* sink = (ONTVideoAudioSink*)clientData;
    unsigned long ts = (presentationTime.tv_usec / 1000 + presentationTime.tv_sec * 1000);
	ont_onvif_playctx *rtmp = (ont_onvif_playctx*)sink->envir().livertmp;
	if (rtmp->startts == 0)
    {
		rtmp->startts = ts;
    }
	unsigned deltaTs = ts - rtmp->startts;

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

#define DEBUG_PRINT_EACH_RECEIVED_FRAME 1

Boolean ONTVideoAudioSink::continuePlaying() {
    if (fSource == NULL) return False; // sanity check (should not happen)

    // Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
    if (this->sourcecodec == ONVIF_CODEC_MPEG4A)
    {
        fSource->getNextFrame(fReceiveBuffer+1, buffersize-1,
            afterGettingFrame, this,
            onSourceClosure, this);
    }
    else
    {
        fSource->getNextFrame(fReceiveBuffer, buffersize,
            afterGettingFrame, this,
            onSourceClosure, this);
    }
    return True;
}