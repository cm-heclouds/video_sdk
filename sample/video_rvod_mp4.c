#ifdef ONT_VIDEO_RVOD_MP4SUPPORTED

#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "log.h"
#include "rtmp_log.h"
#include "amf.h"
#include "rtmp.h"
#include "rtmp_sys.h"
#include "mp4v2/mp4v2.h"

#include "rvod.h"
#include "rtmp_if.h"
#include "ont_list.h"
#include "ont_packet_queue.h"
#include "video_rvod_mp4.h"

#ifdef WIN32
#include <winsock.h>
#endif

static void MP4GetMetadata(t_rtmp_mp4_ctx *ctx);

static int rtmp_rvod_mp4_closefile(t_rtmp_mp4_ctx* rctx);

static void MP4GetVideoInfo(t_rtmp_mp4_ctx *ctx)
{
    const char *media_data_name;
    char originalFormat[8];
    char  oformatbuffer[32];
    RTMPMetadata *pMeta = NULL;
    originalFormat[0] = 0;
    *oformatbuffer = 0;

    if (!ctx)
    {
        return;
    }
    pMeta = &ctx->meta;
    MP4FileHandle mp4file = ctx->fileHanle;
    MP4TrackId    trackid = ctx->videoTrackId;

    media_data_name = MP4GetTrackMediaDataName(mp4file, trackid);
    if (media_data_name == NULL)
    {

    }
    else if ((strcasecmp(media_data_name, "avc1") == 0) ||
        (strcasecmp(originalFormat, "264b") == 0))
    {
        pMeta->videoCodecid = 7; /*H264.*/
    }
    else
    {
        return;
    }


    MP4Duration trackDuration =MP4GetTrackDuration(ctx->fileHanle, ctx->videoTrackId);

    double msDuration = (double)MP4ConvertFromTrackDuration(ctx->fileHanle, ctx->videoTrackId,
        trackDuration, MP4_MSECS_TIME_SCALE);

    pMeta->duration = msDuration / 1000;

    pMeta->videoDataRate = (MP4GetTrackBitRate(ctx->fileHanle, ctx->videoTrackId) + 500) / 1000;

    /*Note not all mp4 implementations set width and height correctly
      The real answer can be buried inside the ES configuration info
    */
    pMeta->width = MP4GetTrackVideoWidth(ctx->fileHanle, ctx->videoTrackId);

    pMeta->height = MP4GetTrackVideoHeight(mp4file, trackid);

    pMeta->frameRate = (int)MP4GetTrackVideoFrameRate(mp4file, trackid);

    ctx->videosamplemaxsize = MP4GetTrackMaxSampleSize(mp4file, trackid);
    ctx->videoSamples = MP4GetTrackNumberOfSamples(mp4file, trackid);

}


#define NUM_ELEMENTS_IN_ARRAY(name) ((sizeof((name))) / (sizeof(*(name))))

static void MP4GetAudioInfo(t_rtmp_mp4_ctx *ctx)
{
    static const char* mpeg4AudioNames[] = {
        "MPEG-4 AAC main",
        "MPEG-4 AAC LC",
        "MPEG-4 AAC SSR",
        "MPEG-4 AAC LTP",
        "MPEG-4 AAC HE",
        "MPEG-4 AAC Scalable",
        "MPEG-4 TwinVQ",
        "MPEG-4 CELP",
        "MPEG-4 HVXC",
        NULL, NULL,
        "MPEG-4 TTSI",
        "MPEG-4 Main Synthetic",
        "MPEG-4 Wavetable Syn",
        "MPEG-4 General MIDI",
        "MPEG-4 Algo Syn and Audio FX",
        "MPEG-4 ER AAC LC",
        NULL,
        "MPEG-4 ER AAC LTP",
        "MPEG-4 ER AAC Scalable",
        "MPEG-4 ER TwinVQ",
        "MPEG-4 ER BSAC",
        "MPEG-4 ER ACC LD",
        "MPEG-4 ER CELP",
        "MPEG-4 ER HVXC",
        "MPEG-4 ER HILN",
        "MPEG-4 ER Parametric",
        "MPEG-4 SSC",
        "MPEG-4 PS",
        "MPEG-4 MPEG Surround",
        NULL,
        "MPEG-4 Layer-1",
        "MPEG-4 Layer-2",
        "MPEG-4 Layer-3",
        "MPEG-4 DST",
        "MPEG-4 Audio Lossless",
        "MPEG-4 SLS",
        "MPEG-4 SLS non-core",
    };

    static const uint8_t mpegAudioTypes[] = {
        MP4_MPEG2_AAC_MAIN_AUDIO_TYPE,  /* 0x66 */
        MP4_MPEG2_AAC_LC_AUDIO_TYPE,    /* 0x67 */
        MP4_MPEG2_AAC_SSR_AUDIO_TYPE,   /* 0x68 */
        MP4_MPEG2_AUDIO_TYPE,           /* 0x69 */
        MP4_MPEG1_AUDIO_TYPE,           /* 0x6B */
        /* private types */
        MP4_PCM16_LITTLE_ENDIAN_AUDIO_TYPE,
        MP4_VORBIS_AUDIO_TYPE,
        MP4_ALAW_AUDIO_TYPE,
        MP4_ULAW_AUDIO_TYPE,
        MP4_G723_AUDIO_TYPE,
        MP4_PCM16_BIG_ENDIAN_AUDIO_TYPE,
    };
    static const char* mpegAudioNames[] = {
        "MPEG-2 AAC Main",
        "MPEG-2 AAC LC",
        "MPEG-2 AAC SSR",
        "MPEG-2 Audio (13818-3)",
        "MPEG-1 Audio (11172-3)",
        /* private types*/
        "PCM16 (little endian)",
        "Vorbis",
        "G.711 aLaw",
        "G.711 uLaw",
        "G.723.1",
        "PCM16 (big endian)",
    };

    if (!ctx)
    {
        return;
    }

    uint8_t numMpegAudioTypes =
        sizeof(mpegAudioTypes) / sizeof(uint8_t);

    const char* typeName = "Unknown";
    bool foundType = false;
    uint8_t type = 0;
    const char *media_data_name;
    MP4FileHandle mp4File = ctx->fileHanle;
    MP4TrackId trackId = ctx->audioTrackId;
    RTMPMetadata *pMeta =&ctx->meta;
    unsigned int audioSampleRate = 0;

    media_data_name = MP4GetTrackMediaDataName(mp4File, trackId);
    uint8_t *audio_cfg  =NULL;
    uint32_t audio_len = 0;
    unsigned i=0;
    if (media_data_name == NULL) {
        typeName = "Unknown - no media data name";
    }
    else if (strcasecmp(media_data_name, "samr") == 0) {
        typeName = "AMR";
        foundType = true;
    }
    else if (strcasecmp(media_data_name, "sawb") == 0) {
        typeName = "AMR-WB";
        foundType = true;
    }
    else if (strcasecmp(media_data_name, "mp4a") == 0) {

        type = MP4GetTrackEsdsObjectTypeId(mp4File, trackId);
        switch (type) {
        case MP4_INVALID_AUDIO_TYPE:
            typeName = "AAC from .mov";
            foundType = true;
            break;
        case MP4_MPEG4_AUDIO_TYPE:  {

            type = MP4GetTrackAudioMpeg4Type(mp4File, trackId);
            if (type == MP4_MPEG4_INVALID_AUDIO_TYPE ||
                type > NUM_ELEMENTS_IN_ARRAY(mpeg4AudioNames) ||
                mpeg4AudioNames[type - 1] == NULL) {
                typeName = "MPEG-4 Unknown Profile";
            }
            else {
                typeName = mpeg4AudioNames[type - 1];
                foundType = true;
            }
            break;
        }
        default:
            for (i = 0; i < numMpegAudioTypes; i++) {
                if (type == mpegAudioTypes[i]) {
                    typeName = mpegAudioNames[i];
                    foundType = true;
                    break;
                }
            }
        }
    }
    else {
        typeName = media_data_name;
        foundType = true;
    }

    uint32_t timeScale =
        MP4GetTrackTimeScale(mp4File, trackId);

    MP4Duration trackDuration =
        MP4GetTrackDuration(mp4File, trackId);

    double msDuration =
        (double)MP4ConvertFromTrackDuration(mp4File, trackId,
        trackDuration, MP4_MSECS_TIME_SCALE);
    (void)msDuration;

    uint32_t avgBitRate =
        MP4GetTrackBitRate(mp4File, trackId);

    pMeta->hasAudio = true;
    if (foundType)
    {
        pMeta->audioCodecid = 10; /*AAC*/
        MP4GetTrackESConfiguration(mp4File, trackId, &audio_cfg, &audio_len);
        if (audio_len)
        {
            ctx->audiocfg = ont_platform_malloc(audio_len);
            memcpy(ctx->audiocfg, audio_cfg, audio_len);
            free(audio_cfg);
        }
    }
    pMeta->audioDataRate = (avgBitRate + 500) / 1000;
    pMeta->audioSampleRate = timeScale;
    ctx->audioSamples = MP4GetTrackNumberOfSamples(mp4File, trackId);
    ctx->audiosamplemaxsize = MP4GetTrackMaxSampleSize(mp4File, trackId);


    if (pMeta->audioSampleRate/1000 == 44)
    {
        audioSampleRate = 3;
    }
    //TODO:

    ctx->audioHeaderTag = rtmp_make_audio_headerTag(pMeta->audioCodecid, audioSampleRate, 1 /*16 bit*/, 1);
    (void)typeName;

}

static void MP4GetMetadata(t_rtmp_mp4_ctx *ctx)
{
    unsigned int trackId = 0;
    unsigned int numTracks = MP4GetNumberOfTracks(ctx->fileHanle, NULL, 0);
    RTMPMetadata *pMeta = &ctx->meta;
    unsigned int i;

    memset(pMeta, 0x00, sizeof(RTMPMetadata));

    for (i=0; i < numTracks; i++) {
        trackId = MP4FindTrackId(ctx->fileHanle, i, NULL, 0);

        const char* trackType =
            MP4GetTrackType(ctx->fileHanle, trackId);
        if (trackType == NULL)
        {
            continue;
        }

        if (!strcmp(trackType, MP4_VIDEO_TRACK_TYPE)) {
            ctx->videoTrackId = trackId;
            MP4GetVideoInfo(ctx);
        }
        else if (!strcmp(trackType, MP4_AUDIO_TRACK_TYPE)) {
            ctx->audioTrackId = trackId;
            MP4GetAudioInfo(ctx);
        }
    }
}


void MP4GetH264SpsPPs(MP4FileHandle mp4File, MP4TrackId trackId, char **Sps, unsigned int *SpsLen, char **Pps, unsigned int *PpsLen)
{

    uint8_t     **pp_sps, **pp_pps;
    uint32_t     *pn_sps, *pn_pps;
    uint32_t     i = 0;
    int sps_totallen = 0;
    int pps_totallen = 0;
    *SpsLen = 0;
    *PpsLen = 0;

    if (MP4GetTrackH264SeqPictHeaders(mp4File, trackId, &pp_sps, &pn_sps, &pp_pps, &pn_pps))
    {
        for (i = 0; *(pp_sps + i); i++) {
            sps_totallen += *(pn_sps + i);
        }
        *Sps = (char*)ont_platform_malloc(sps_totallen);

        if (pp_sps) {
            for (i = 0; *(pp_sps + i); i++) {
                memcpy(*Sps + *SpsLen, *(pp_sps + i), *(pn_sps + i));
                *SpsLen += *(pn_sps + i);
				free(*(pp_sps + i));
            }
            free(pp_sps);
            free(pn_sps);
        }


        for (i = 0; *(pp_pps + i); i++) {
            pps_totallen += *(pn_pps + i);
        }
        *Pps = (char*)ont_platform_malloc(pps_totallen);

        if (pp_pps) {
            for (i = 0; *(pp_pps + i); i++) {
                memcpy(*Pps + *PpsLen, *(pp_pps + i), *(pn_pps + i));
                *PpsLen += *(pn_pps + i);
                free(*(pp_pps + i));
            }
            free(pp_pps);
            free(pn_pps);
        }
    }

}


const char h264_delimiter[] = { 0x01, 0, 0, 0 };

#define RTMP_HEAD_SIZE   (sizeof(RTMPPacket)+RTMP_MAX_HEADER_SIZE)


static int Mp4GetAudioData(t_rtmp_mp4_ctx *ctx, unsigned char *pFrameBuf, unsigned int *pSize, MP4Timestamp  *timeStamp, MP4Timestamp *pduration, unsigned int sample_id)
{
    unsigned char      *p_audio_sample;
    unsigned int       n_audio_sample;
    int                b;
    unsigned int      body_size = 0;

    p_audio_sample = pFrameBuf + 1;
    n_audio_sample = *pSize - 1;
    if (!ctx)
    {
        return -1;
    }

    if ((sample_id <= 0) || (sample_id >ctx->audioSamples)) {
        RTMP_LogPrintf("sample_id = %d invalid", sample_id);
        *pSize = 0;
        return -1;
    }


    b = MP4ReadSample(ctx->fileHanle, ctx->audioTrackId, sample_id, &p_audio_sample, &n_audio_sample,
        timeStamp, NULL, NULL, NULL);
    if (!b) {
        *pSize = 0;
        return -1;
    }
    /* for aac type, need set the first byte*/
    pFrameBuf[0] = 0x01;
    body_size++;
    body_size += n_audio_sample;


    *pSize = body_size;
    return body_size;
}



static int Mp4GetVideoTag(t_rtmp_mp4_ctx *ctx, unsigned char *pFrameBuf, unsigned int *pSize, unsigned int *isIframe, MP4Timestamp  *timeStamp, MP4Timestamp *pduration, unsigned int sample_id)
{
    unsigned char      *p_video_sample;
    unsigned int       n_video_sample;
    int                b;
    bool       iskeyFram;
    unsigned int      body_size = 0;
    int h264_nal_leng=0;
    int nal_leng_acc = 0;

    p_video_sample = pFrameBuf+5;
    n_video_sample = *pSize-5;
    if (!ctx)
    {
        return -1;
    }

    if ((sample_id <= 0) || (sample_id >ctx->videoSamples)) {
        RTMP_LogPrintf("sample_id = %d invalid", sample_id);
        *pSize = 0;
        return -1;
    }


    b = MP4ReadSample(ctx->fileHanle, ctx->videoTrackId, sample_id, &p_video_sample, &n_video_sample,
        timeStamp, NULL, NULL, &iskeyFram);
    if (!b) {
        *pSize = 0;
        return -1;
    }
    /*
    (codec == h.264), the first 4 bytes are the length octets.
    They need to be changed to H.264 delimiter (01 00 00 00).
    */
    if (iskeyFram)
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
    do {
        h264_nal_leng = p_video_sample[0];
        h264_nal_leng = (h264_nal_leng << 8) | p_video_sample[1];
        h264_nal_leng = (h264_nal_leng << 8) | p_video_sample[2];
        h264_nal_leng = (h264_nal_leng << 8) | p_video_sample[3];
        pFrameBuf[body_size] = h264_nal_leng >> 24 & 0xff;
        body_size++;
        pFrameBuf[body_size] = h264_nal_leng >> 16 & 0xff;
        body_size++;
        pFrameBuf[body_size] = h264_nal_leng >> 8 & 0xff;
        body_size++;
        pFrameBuf[body_size] = h264_nal_leng & 0xff;
        body_size++;

        memmove(pFrameBuf + body_size, p_video_sample + 4, h264_nal_leng);
        body_size += h264_nal_leng;
        p_video_sample += (h264_nal_leng + 4);
        nal_leng_acc += (h264_nal_leng + 4);
    } while (nal_leng_acc < n_video_sample);


    *pSize = body_size;
    *isIframe = iskeyFram ? 1 : 0;
    return body_size;
}


int rtmp_rvod_mp4_send_media_singlestep(t_rtmp_vod_ctx* rctx)
{
	if (!rctx || !rctx->rtmp)
    {
        return -1;
    }
	t_rtmp_mp4_ctx *ctx = (t_rtmp_mp4_ctx *)rctx->user_data;
    if (!ctx->packDataList) {
        ctx->packDataList = ont_list_create();
    }
    RTMP *rtmp = rctx->rtmp;
    unsigned int end_timestamp = 0;
    unsigned int buf_size = 0;
    MP4Timestamp cur_frame_time = 0;
    unsigned int isKeyFram = 0;
    unsigned int delta_time_video = 0;
    unsigned int delta_time_audio = 0;
    unsigned char *video_buf = ctx->video_buf;
    unsigned char *audio_buf = ctx->audio_buf;
    const static int send_max_frames =4;
    int counter =0 ;

    t_rtmp_mode_ctx  _rtmp_mode_ctx;
    _rtmp_mode_ctx.mp4_ctx = ctx;
    _rtmp_mode_ctx.type = RVOD_MODE;

    if (video_buf == NULL)
    {
		video_buf = ctx->video_buf = (unsigned char *)ont_platform_malloc(ctx->videosamplemaxsize + 5);
    }
    if (audio_buf == NULL)
    {
		audio_buf = ctx->audio_buf = (unsigned char *)ont_platform_malloc(ctx->audiosamplemaxsize + 1);
    }

    /* check the sample if invalid*/
    if (ctx->next_videosampleid >= ctx->videoSamples && ctx->next_audiosampleid >= ctx->audioSamples)
    {
        RTMPVideoAudioCtl *ctl = (RTMPVideoAudioCtl *)ont_platform_malloc(sizeof (RTMPVideoAudioCtl));
        memset(ctl, 0, sizeof(RTMPVideoAudioCtl));
        ctl->next_videosampleid = ctx->next_videosampleid;
        ctl->next_audiosampleid = ctx->next_audiosampleid;

        RTMPPackEnqueue(&_rtmp_mode_ctx, 0, 0, 0, 0, NULL, ctl);
        return 0;
    }

    end_timestamp = ctx->start_timestamp + (RTMP_GetTime() - ctx->epoch) + M_RTMP_CLIENTBUFLEN;

    if(RTMP_IsConnected(rtmp))
    {
        rtmp_check_rcv_handler(rctx->rtmp);

		if (ctx->paused)
		{
			return 0;
		}

		if (ctx->stoped)
		{
			return -1;
		}

        if (ctx->last_videotimestamp > end_timestamp)
        {
            delta_time_video = ctx->last_videotimestamp - end_timestamp;
        }
        else
        {
            counter = 0;
            /*send video*/
            for (;;)
            {
                if (counter>send_max_frames)
                {
                    break;
                }
				counter++;
                buf_size = ctx->videosamplemaxsize + 5;

                if (Mp4GetVideoTag(ctx, video_buf, &buf_size, &isKeyFram, &cur_frame_time, NULL, ctx->next_videosampleid) < 0)
                {
                    /*need send stream eof*/
                    break;
                }

                cur_frame_time = MP4ConvertFromTrackTimestamp(ctx->fileHanle, ctx->videoTrackId, cur_frame_time, MP4_MSECS_TIME_SCALE);


                if (ctx->seeked[0])
    			{
                    ctx->seeked[0] = 0;
                    //rtmp_send_spspps(rtmp, ctx->sps, ctx->spsLen, ctx->pps, ctx->ppsLen, cur_frame_time);
                    //ctx->sendmeta[0] = 0;

                }

                RTMPVideoAudioCtl *ctl = (RTMPVideoAudioCtl *) ont_platform_malloc(sizeof (RTMPVideoAudioCtl));
                memset(ctl, 0, sizeof(RTMPVideoAudioCtl));
                ctl->isKeyFram = isKeyFram;
                ctl->next_videosampleid = ctx->next_videosampleid++;
                ctl->next_audiosampleid = ctx->next_audiosampleid;

                RTMPPackEnqueue(&_rtmp_mode_ctx, video_buf, buf_size, cur_frame_time, CODEC_H264, NULL, ctl);

                //rtmp_send_videodata(rtmp, video_buf, buf_size, cur_frame_time, isKeyFram);
                ctx->last_videotimestamp = cur_frame_time;

            }
        }

        if (ctx->last_audiotimestamp > end_timestamp)
        {
            delta_time_audio= ctx->last_audiotimestamp - end_timestamp;
        }
        else
        {
            counter = 0;
            /*send audio*/
            for (;;)
            {

                if (counter>send_max_frames)
                {
                    /*send over.*/
                    break;
                }
				counter++;

                buf_size = ctx->audiosamplemaxsize + 1;
                if (Mp4GetAudioData(ctx, audio_buf, &buf_size, &cur_frame_time, NULL, ctx->next_audiosampleid) < 0)
                {
                    /*need send stream eof*/
                    break;
                }


                /*send aac speicial config first*/
                if (ctx->seeked[1] || ctx->next_audiosampleid == 1)
                {
                    if (ctx->seeked[1])
                    {
                        ctx->seeked[1] = 0;
                    }
                }

    			cur_frame_time = MP4ConvertFromTrackTimestamp(ctx->fileHanle, ctx->audioTrackId, cur_frame_time, MP4_MSECS_TIME_SCALE);

                RTMPVideoAudioCtl *ctl = (RTMPVideoAudioCtl *) ont_platform_malloc(sizeof (RTMPVideoAudioCtl));
                memset(ctl, 0, sizeof(RTMPVideoAudioCtl));
                ctl->next_videosampleid = ctx->next_videosampleid;
                ctl->next_audiosampleid = ctx->next_audiosampleid++;

                //rtmp_send_audiodata(rtmp, ctx->audioHeaderTag, audio_buf, buf_size, cur_frame_time, RTMP_PACKET_SIZE_MEDIUM);
                RTMPPackEnqueue(&_rtmp_mode_ctx, audio_buf, buf_size, cur_frame_time, CODEC_MPEG4A, NULL, ctl);
                ctx->last_audiotimestamp = cur_frame_time;
            }
        }
        /* check if both video and audio send finished*/
        if (ctx->next_audiosampleid >= ctx->audioSamples && ctx->next_videosampleid >= ctx->videoSamples)
        {
            return 0;
        }

        return delta_time_audio<delta_time_video?delta_time_audio:delta_time_video;
    }
    else
    {
        return -1;
    }
}

int rtmp_rvod_mp4_parse(t_rtmp_mp4_ctx* ctx, const char *file)
{
	if (!ctx)
    {
        return -1;
    }
    MP4FileHandle mp4handle = MP4Read(file);
    if (!mp4handle){
        RTMP_LogPrintf("Open File Error.\n");
        return -1;
    }
    ctx->fileHanle = mp4handle;

#if 0
    const char* trackinfo = MP4Info(mp4handle, 0);

    RTMP_LogPrintf("%s", trackinfo);

    free((void*)trackinfo);

#endif

    MP4GetMetadata(ctx);
    MP4GetH264SpsPPs(mp4handle, ctx->videoTrackId, &ctx->sps, &ctx->spsLen, &ctx->pps, &ctx->ppsLen);


    return 0;
}

int rtmp_rvod_mp4_start(t_rtmp_mp4_ctx *ctx)
{
    t_rtmp_vod_ctx *rctx = (t_rtmp_vod_ctx*)ctx->rvodctx;
    ctx->epoch = RTMP_GetTime();
    ctx->start_timestamp = 0;
	//ont_platform_sleep(300);/* for rvod, need sleep a while to avoid server drop frame*/
    rtmp_send_metadata(rctx->rtmp, &ctx->meta);

    ctx->next_videosampleid = 1; /*start from 1*/
    ctx->next_audiosampleid = 1;
    ctx->sendmeta[0] = 0; /*0 haven't send meta data, 1 sended*/
    ctx->sendmeta[1] = 0;
    ctx->state = 0;
	return 0;
}


static int rtmp_rvod_mp4_closefile(t_rtmp_mp4_ctx* ctx)
{
	if (!ctx)
    {
        return -1;
    }
    if (!ctx->fileHanle)
    {
        MP4Close(ctx->fileHanle, 0);
        ctx->fileHanle=0;      
    }
    return 0;
}


void  rtmp_rvod_mp4_stop_notify(void* rctx)
{
	if (!rctx)
    {
        return;
    }
    RTMP_LogPrintf("rtmp stopeed\n");
	t_rtmp_vod_ctx *rc = rctx;
	t_rtmp_mp4_ctx *c = (t_rtmp_mp4_ctx *)rc->user_data;
    c->stoped = 1;
}

void  rtmp_rvod_mp4_pause_notify(void* rctx, int paused, double ts)
{
    RTMP_LogPrintf("rtmp pause %d ts %lf\n", paused, ts);
    if (!rctx)
    {
        return;
    }
	t_rtmp_vod_ctx *rc = rctx;
	t_rtmp_mp4_ctx *c = (t_rtmp_mp4_ctx *)rc->user_data;

    c->paused = paused;

    if (paused)
    {
        c->start_timestamp = ts;
    }
    else
    {
		c->start_timestamp = ts;
        c->epoch = RTMP_GetTime();
    }
}

int  rtmp_rvod_mp4_seek_notify(void* rctx, double ts)
{
    t_rtmp_mode_ctx mc;
    /*seek to the specified type*/
    if (!rctx)
    {
        return -1;
    }
	t_rtmp_vod_ctx *rc = rctx;
	t_rtmp_mp4_ctx *c = (t_rtmp_mp4_ctx *)rc->user_data;

    c->seeked[0] = 1;
    c->seeked[1] = 1;
    c->sendmeta[0] = 0;
    c->sendmeta[1] = 0;

    if ((int) ts == c->start_timestamp) {
        return 0;
    }
    RTMP_LogPrintf("rtmp seekd offset %lf\n", ts);
    c->start_timestamp = ts;
    c->epoch = RTMP_GetTime();
    c->last_videotimestamp = 0;
    c->last_audiotimestamp = 0;
    mc.type = RVOD_MODE;
    mc.mp4_ctx=c;
    
    RTMPPackClearqueue(&mc);

    /*seek the video*/
	do
	{
		c->next_videosampleid = MP4GetSampleIdFromTime(c->fileHanle, c->videoTrackId, MP4ConvertToTrackTimestamp(c->fileHanle, c->videoTrackId, ts, MP4_MSECS_TIME_SCALE), TRUE);
		if (c->next_videosampleid == MP4_INVALID_SAMPLE_ID)
		{

			ts -= 100;
		}
		else
		{
			break;
		}
	} while (TRUE);

	if (c->next_videosampleid == MP4_INVALID_SAMPLE_ID)
	{
		c->next_videosampleid = c->videoSamples;
	}
	/*seek the audio*/
	do
	{
		c->next_audiosampleid = MP4GetSampleIdFromTime(c->fileHanle, c->audioTrackId, MP4ConvertToTrackTimestamp(c->fileHanle, c->audioTrackId, ts, MP4_MSECS_TIME_SCALE), FALSE);
		if (c->next_audiosampleid == MP4_INVALID_SAMPLE_ID)
		{

			ts -= 100;
		}
		else
		{
			break;
		}
	} while (TRUE);

	if (c->next_audiosampleid == MP4_INVALID_SAMPLE_ID)
	{
		c->next_audiosampleid = c->audioSamples;
	}
	c->start_timestamp = ts;
	return 0;

}

int rtmp_rvod_mp4_clear(t_rtmp_mp4_ctx *ctx)
{

	if (!ctx)
	{
		return -1;
	}

	rtmp_rvod_mp4_closefile(ctx);

    if (ctx->packDataList)
    {
        ont_list_destroy(ctx->packDataList);
        ctx->packDataList=NULL;
    }
    if (ctx->sps)
    {
        ont_platform_free(ctx->sps);
        ctx->sps=NULL;
    }
    if (ctx->pps)
    {
        ont_platform_free(ctx->pps);
        ctx->pps=NULL;
    }
    if (ctx->video_buf)
    {
        ont_platform_free(ctx->video_buf);
        ctx->video_buf=NULL;
    }
    if (ctx->audio_buf)
    {
        ont_platform_free(ctx->audio_buf);
        ctx->audio_buf=NULL;
    }
    if (ctx->audiocfg) /**/
    {
        ont_platform_free(ctx->audiocfg);
        ctx->audiocfg=NULL;
    }
	return 0;
}


int rtmp_rvod_select(t_rtmp_vod_ctx * ctx)
{
    t_rtmp_mode_ctx _rtmp_mode_ctx;

    struct timeval timeout = {0, 0};
    fd_set fdwrite;
    FD_ZERO(&fdwrite);


    RTMP* rtmp_cli = (RTMP *)ctx->rtmp;
    struct ont_socket_t *  ont_sock = rtmp_cli->m_sb.ont_sock;
    int fd = ont_platform_tcp_socketfd(ont_sock);

    FD_SET(fd, &fdwrite);
#ifdef ONT_OS_POSIX
#endif
    int maxfd = fd;
    int active = select(maxfd + 1, NULL, &fdwrite, NULL, &timeout);
    switch (active)
    {
        case -1:
            break;
        case 0:
            break;
        default:
            _rtmp_mode_ctx.mp4_ctx = ctx->user_data;
            _rtmp_mode_ctx.type = RVOD_MODE;
            RTMPPackDequeue(&_rtmp_mode_ctx);
            break;
    }
    return 0;
}


#endif
