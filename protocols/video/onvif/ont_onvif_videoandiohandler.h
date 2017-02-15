#ifndef __ONT_ONVIF_VIDEOAUDIOHNALDER_H__
#define __ONT_ONVIF_VIDEOAUDIOHNALDER_H__

#include "device_onvif.h"

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#include "librtmp/rtmp.h"

#include "ont/video.h"
#include "ont_onvif_if.h"

class ONTVideoAudioSink : public
    MediaSink
{
public:
	static ONTVideoAudioSink* createNew(
        MediaSubsession& subsession,
		ont_onvif_playctx *ctx,
        // identifies the kind of data that's being received
        char const* streamId = NULL); // identifies the stream itself (optional)

private:
    ONTVideoAudioSink(MediaSubsession& subsession,
		ont_onvif_playctx *ctx,
        // identifies the kind of data that's being received
        char const* streamId = NULL);
    // called only by "createNew()"
    virtual ~ONTVideoAudioSink();
	int handleVideoFrame(unsigned frameSize, unsigned numTruncatedBytes, unsigned int ts);
	
	int handleAACFrame(unsigned frameSize, unsigned numTruncatedBytes, unsigned int ts);


    int h264parse(const unsigned char *buf, unsigned size, unsigned &offset);
	static void closure(void* clientData);

    static void afterGettingFrame(void* clientData, unsigned frameSize,
        unsigned numTruncatedBytes,
        struct timeval presentationTime,
        unsigned durationInMicroseconds);
private:
    // redefined virtual functions:
    virtual Boolean continuePlaying();

private:
    u_int8_t* fReceiveBuffer;
	unsigned buffersize;
    MediaSubsession& fSubsession;
    char* fStreamId;
	char *p_extra;
	unsigned i_extra;
	unsigned char audioTag;
    unsigned char  latestSps[1000];
	uint32_t  sps_len;
    unsigned char  latestPps[100];
	uint32_t  pps_len;
	uint32_t  sourcecodec; // 
	unsigned  sendmeta;
	unsigned  sendaudioheader;
	ont_onvif_playctx *ctx;
};

/*VIDEO*/
#define ONVIF_CODEC_H264       1


/*AUDIO*/
#define ONVIF_CODEC_MPEG4A     11

#endif