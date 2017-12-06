#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ont/platform.h"
#include <ont/log.h>
#include "BasicUsageEnvironment.hh"
#include "ont/video.h"
#include "ont_packet_queue.h"
#include "ont_list.h"
#include "ont_onvif_if.h"
#include "librtmp/log.h"
#include "librtmp/rtmp_sys.h"
#include "video_rvod_mp4.h"

#define PACK_BUFFER_SIZE 10000

int RTMPLivePackEnqueue(ont_onvif_playctx *playctx, unsigned char* pFrameBuf, int size,
        unsigned int deltaTs, int codecType, void *data, RTMPVideoAudioCtl *ctl);
int RTMPRvodPackEnqueue(t_rtmp_mp4_ctx *playctx, unsigned char* pFrameBuf, int size,
        unsigned int deltaTs, int codecType, void *data, RTMPVideoAudioCtl *ctl);

int RTMPRvodPackDequeue(void *ctx);
int RTMPLivePackDequeue(void *ctx);

void _rtmp_packet_handle_proc(void* cliendData, int mask)
{
    t_rtmp_mode_ctx _rtmp_mode_ctx;
    int cnt=0;
    if (mask&SOCKET_WRITABLE)
    {
        ont_onvif_playctx * ctx = (ont_onvif_playctx *)cliendData;
        _rtmp_mode_ctx.onvif_ctx = ctx;
        _rtmp_mode_ctx.type = LIVE_MODE;

        RTMPPackDequeue(&_rtmp_mode_ctx);
    }
    return ;
}

int ont_set_rtmp_packet_handle(ont_onvif_playctx *ctx, HandlerProc proc, void *arg)
{
    if (!ctx || !ctx->rtmp_client) {
        return -1;
    }
    BasicUsageEnvironment* env = (BasicUsageEnvironment*)ctx->play_env;
    TaskScheduler &scheduler = env->taskScheduler();

    RTMP* rtmp_cli = (RTMP *)ctx->rtmp_client;
    struct ont_socket_t *  ont_sock = rtmp_cli->m_sb.ont_sock;
    int fd = ont_platform_tcp_socketfd(ont_sock);

    ctx->fd = fd;
    scheduler.setBackgroundHandling(fd,  SOCKET_WRITABLE | SOCKET_EXCEPTION, proc, arg);
    return 0;
}

int ont_disable_rtmp_packet_handle(ont_onvif_playctx *ctx)
{
    if (!ctx || !ctx->rtmp_client) {
        return -1;
    }
    BasicUsageEnvironment* env = (BasicUsageEnvironment*)ctx->play_env;
    TaskScheduler &scheduler = env->taskScheduler();

    scheduler.disableBackgroundHandling(ctx->fd);
    return 0;
}

int RTMPPackEnqueue(void *ctx, unsigned char* pFrameBuf, int size,
        unsigned int deltaTs, int codecType, void *data, RTMPVideoAudioCtl *ctl)
{
    t_rtmp_mode_ctx *mode_ctx = (t_rtmp_mode_ctx *)ctx;
    if (!mode_ctx) {
        return -1;
    }

    if (mode_ctx->type == LIVE_MODE &&  mode_ctx->onvif_ctx) {
        RTMPLivePackEnqueue(mode_ctx->onvif_ctx, pFrameBuf, size, deltaTs, codecType, data, ctl);
    } else if (mode_ctx->type == RVOD_MODE && mode_ctx->mp4_ctx) {
        RTMPRvodPackEnqueue(mode_ctx->mp4_ctx, pFrameBuf, size, deltaTs, codecType, data, ctl);
    }
    return 0;
}


int RTMPRvodPackEnqueue(t_rtmp_mp4_ctx *playctx, unsigned char* pFrameBuf, int size,
        unsigned int deltaTs, int codecType, void *data,  RTMPVideoAudioCtl *ctl)
{
    RTMPPackData  *pkdata;
    ont_list_t *plist = NULL;

    if (!playctx || !playctx->packDataList) {
        goto _end;
    }
    plist = (ont_list_t *)playctx ->packDataList;

    if (ont_list_size(plist) > PACK_BUFFER_SIZE) {
        goto _end;
    }

    pkdata = (RTMPPackData *)ont_platform_malloc(sizeof(RTMPPackData));
    if (pkdata < 0) {
        return -1;
    }
    memset(pkdata, 0, sizeof(RTMPPackData));
    pkdata->data =(char *) ont_platform_malloc(size * sizeof(char));
    memcpy(pkdata->data, pFrameBuf, size);
    pkdata->size = size;
    pkdata->deltaTs = deltaTs;
    pkdata->codecType = codecType;
    pkdata->ctl = ctl;

    ont_list_insert(plist, pkdata);
    return 0;
_end:
    switch (codecType) {
    case CODEC_H264:
        if (data) {
            free((RTMPMetaSpsPpsData*)data);
        }
        break;
    case CODEC_MPEG4A:
        if (data) {
            free((RTMPAudioHeaderData*)data);
        }
        break;
    }
    if (ctl) {
        free(ctl);
    }
    return -1;
}

int _getListHead(void * d1, void * d2) {
    return 0;
}

int RTMPLivePackEnqueue(ont_onvif_playctx *playctx, unsigned char* pFrameBuf, int size,
        unsigned int deltaTs, int codecType, void *data, RTMPVideoAudioCtl *ctl)
{
    RTMPPackData  *pkdata, *pkdataHead = NULL;
	ont_list_node_t *node = NULL;
    ont_list_t *plist = NULL;

    if (!playctx || !playctx->packDataList) {
        goto _end;
    }
    plist = playctx ->packDataList;

    if (ont_list_size(plist) > PACK_BUFFER_SIZE) {
        goto _end;
    }

    node = ont_list_find(plist, &pkdata, _getListHead);
    pkdataHead = (RTMPPackData *) ont_list_data(node);
    if (pkdataHead &&  playctx->last_sndts < pkdataHead->sndts) {
        goto _end;
    }
    if (pkdataHead && playctx->last_sndts - pkdataHead->sndts > 5000) {
        playctx->state = 1;
        ONT_LOG2(ONTLL_INFO, "live packet enqueue, last sndts %lu, buffer header sndts %lu", playctx->last_sndts, pkdataHead->sndts);
        return -4;
    }

    pkdata =  new RTMPPackData();
    pkdata->data = new char[size];
    memcpy(pkdata->data, pFrameBuf, size);
    pkdata->size = size;
    pkdata->deltaTs = deltaTs;
    pkdata->codecType = codecType;
    pkdata->sndts = playctx->last_sndts;
    pkdata->ctl = ctl;

    if (CODEC_H264 == codecType && data) {
        pkdata->mspd = (RTMPMetaSpsPpsData *)data;
    }
    else if (CODEC_MPEG4A == codecType) {
        pkdata->ahd = (RTMPAudioHeaderData *)data;
    }

    ont_list_insert(plist, pkdata);

    ont_set_rtmp_packet_handle(playctx, _rtmp_packet_handle_proc, playctx);
    return 0;
_end:
    switch (codecType) {
    case CODEC_H264:
        if (data) {
            delete (RTMPMetaSpsPpsData*)data;
        }
        break;
    case CODEC_MPEG4A:
        if (data) {
            delete (RTMPAudioHeaderData*)data;
        }
        break;
    }
    if (ctl) {
        delete ctl;
    }
    return -1;
}

int RTMPPackDequeue(void *ctx)
{
    t_rtmp_mode_ctx *mode_ctx = (t_rtmp_mode_ctx *)ctx;
    if (!mode_ctx) {
        return -1;
    }

    if (mode_ctx->type == LIVE_MODE &&  mode_ctx->onvif_ctx) {
        RTMPLivePackDequeue(mode_ctx->onvif_ctx);
    } else if (mode_ctx->type == RVOD_MODE && mode_ctx->mp4_ctx) {
        RTMPRvodPackDequeue(mode_ctx->mp4_ctx);
    }
    return 0;
}



int RTMPLivePackClearqueue(void *ctx)
{
    ont_onvif_playctx *playctx = (ont_onvif_playctx *)ctx;

    ont_list_t *plist;
    RTMPPackData *pkdata = NULL;

    if (!playctx || !playctx->packDataList) {
        return 0;
    }
    plist = playctx ->packDataList;
    do
    {
        if (ont_list_size(plist) == 0) {
            return 0;
        }
        ont_list_pop_front(plist, (void **)&pkdata);
        if (!pkdata) {
            continue;
        }

        if (pkdata->mspd) {
            delete (pkdata->mspd);
        }
        if (pkdata->ahd) {
            delete (pkdata->ahd);
        }
        if (pkdata->ctl) {
            delete (pkdata->ctl);
        }
        delete[] (pkdata->data);
        delete (pkdata);
    }while(1);
    
    return 0;
}



int RTMPRvodPackClearqueue(void *ctx)
{
    t_rtmp_mp4_ctx  *playctx = (t_rtmp_mp4_ctx  *)ctx;
    ont_list_t *plist;
    RTMPPackData *pkdata = NULL;

    if (!playctx || !playctx->packDataList) {
        return 0;
    }
    plist = (ont_list_t *) playctx ->packDataList;

    do
    {
        if (ont_list_size(plist) == 0) {
            return 0;
        }
        ont_list_pop_front(plist, (void **)&pkdata);
        if (!pkdata) {
            continue;
        }
        if (pkdata->ctl) {
            free(pkdata->ctl);
            pkdata->ctl = NULL;
        }
        free(pkdata->data);
        free(pkdata);
    }while(1);
    return 0;
}


int RTMPPackClearqueue(void *ctx)
{
    t_rtmp_mode_ctx *mode_ctx = (t_rtmp_mode_ctx *)ctx;
    if (!mode_ctx) {
        return -1;
    }

    if (mode_ctx->type == LIVE_MODE &&  mode_ctx->onvif_ctx) {
        RTMPLivePackClearqueue(mode_ctx->onvif_ctx);
    } else if (mode_ctx->type == RVOD_MODE && mode_ctx->mp4_ctx) {
        RTMPRvodPackClearqueue(mode_ctx->mp4_ctx);
    }
    return 0;    
}

int RTMPRvodPackDequeue(void *ctx)
{
    t_rtmp_mp4_ctx  *playctx = (t_rtmp_mp4_ctx  *)ctx;
    ont_list_t *plist;
    RTMPPackData *pkdata = NULL;

    if (!playctx || !playctx->packDataList) {
        return 0;
    }
	t_rtmp_vod_ctx  *vodctx = playctx->rvodctx;
    plist = (ont_list_t *) playctx ->packDataList;

    if (ont_list_size(plist) == 0) {
        return 0;
    }
    if (playctx->paused) {
        return 0;
    }
    ont_list_pop_front(plist, (void **)&pkdata);
    if (!pkdata) {
        return 0;
    }
    RTMPVideoAudioCtl *ctl = pkdata->ctl;

    if (ctl->next_audiosampleid >= playctx->audioSamples && ctl->next_videosampleid >= playctx->videoSamples)
    {
		rtmp_rvod_send_videoeof(vodctx);
        playctx->state = 1;
        goto _end;
    }

    switch (pkdata->codecType) {

    case CODEC_H264:

        if (playctx->sendmeta[0] == 0) {
 			rtmp_send_spspps(vodctx->rtmp, (unsigned char *)playctx->sps, playctx->spsLen, (unsigned char *)playctx->pps, playctx->ppsLen, pkdata->deltaTs);
            playctx->sendmeta[0] = 1;
        }
		else if (pkdata->ctl->isKeyFram)
		{
			rtmp_send_spspps(vodctx->rtmp, (unsigned char *)playctx->sps, playctx->spsLen, (unsigned char *)playctx->pps, playctx->ppsLen, pkdata->deltaTs);
		}
		rtmp_send_videodata(vodctx->rtmp, (unsigned char *)pkdata->data, pkdata->size, pkdata->deltaTs, pkdata->ctl->isKeyFram);
        break;

    case CODEC_MPEG4A:
        if (playctx->sendmeta[1] == 0) {
            unsigned char audio_data[3];
            audio_data[0] = 0x00;
            audio_data[1] = playctx->audiocfg[0];
            audio_data[2] = playctx->audiocfg[1];

			rtmp_send_audiodata(vodctx->rtmp, playctx->audioHeaderTag, audio_data, 3, 0, RTMP_PACKET_SIZE_LARGE);
            playctx->sendmeta[1] = 1;
        }
        if (playctx->sendmeta[0] == 0) {
            goto _end;
        }
		rtmp_send_audiodata(vodctx->rtmp, playctx->audioHeaderTag, (unsigned char *)pkdata->data, pkdata->size,
                pkdata->deltaTs, RTMP_PACKET_SIZE_MEDIUM);
        break;
    default:
        break;

    }
_end:
    if (pkdata->ctl) {
        free(pkdata->ctl);
        pkdata->ctl = NULL;
    }
    free(pkdata->data);
    pkdata->data = NULL;
    free(pkdata);
    pkdata = NULL;
    return 0;
}

int RTMPLivePackDequeue(void *ctx)
{
    ont_onvif_playctx *playctx = (ont_onvif_playctx *)ctx;

    ont_list_t *plist;
    RTMPPackData *pkdata = NULL;

    if (!playctx || !playctx->packDataList) {
        return 0;
    }
    plist = playctx ->packDataList;

    if (ont_list_size(plist) == 0) {
        return 0;
    }
    ont_list_pop_front(plist, (void **)&pkdata);
    if (!pkdata) {
        return 0;
    }
    if (playctx->last_sndts < pkdata->sndts) {
        return 0;
    }
    if (playctx->last_sndts - pkdata->sndts  > 5000) {
        ONT_LOG2(ONTLL_INFO, "live packet dequeue, last sndts %lu, buffer header sndts %lu", playctx->last_sndts, pkdata->sndts);
        playctx->state = 1;
    }

	switch (pkdata->codecType) {
	case CODEC_H264:
		if (playctx->sendmeta[0] == 0) {
			rtmp_send_metadata(playctx->rtmp_client, &playctx->meta);
			playctx->sendmeta[0] = 1;
		}
		if (!playctx->key_send)
		{
			if (!pkdata->ctl->isKeyFram)
			{
				goto _end;
			}
		}
		if (pkdata->ctl->isKeyFram && pkdata->mspd)  {
			rtmp_send_spspps(playctx->rtmp_client, (unsigned char*)pkdata->mspd->latestSps, pkdata->mspd->sps_len,
				(unsigned char *)pkdata->mspd->latestPps, pkdata->mspd->pps_len, pkdata->deltaTs);
		}
		if (!playctx->key_send){
			playctx->key_send = 1;
		}

        if (rtmp_send_videodata(playctx->rtmp_client, (unsigned char *)pkdata->data, pkdata->size, 
			    pkdata->deltaTs, pkdata->ctl->isKeyFram) < 0) {
            playctx->state = 1;
            ONT_LOG1(ONTLL_INFO, "live packet dequeue, send video data, state is %d", playctx->state);
            goto _end;
        }
        break;
    case CODEC_MPEG4A:
        if (playctx->sendmeta[0] == 0) {
            goto _end;
        }
        if (playctx->sendmeta[1] == 0) {
			if (pkdata->ctl->isAudioHeader == 1 && pkdata->ahd) {
                if (rtmp_send_audiodata(playctx->rtmp_client, pkdata->ahd->audioTag, (unsigned char *)pkdata->data, pkdata->size,
                            pkdata->deltaTs, pkdata->ahd->audiotype) < 0) {
                    playctx->state = 1;
                    ONT_LOG1(ONTLL_INFO, "live packet dequeue, send audio data, state is %d", playctx->state);
                    goto _end;
                }
                playctx->sendmeta[1] = 1;
            } else {
                goto _end;
            }
        }
        if (rtmp_send_audiodata(playctx->rtmp_client, pkdata->ahd->audioTag, (unsigned char *)pkdata->data, pkdata->size,
                pkdata->deltaTs, pkdata->ahd->audiotype) < 0) {
            playctx->state = 1;
            ONT_LOG1(ONTLL_INFO, "live packet dequeue, send audio data, state is %d", playctx->state);
            goto _end;
        }
        break;
    default:
        break;

    }

_end:
    if (pkdata->mspd) {
        delete (pkdata->mspd);
    }
    if (pkdata->ahd) {
        delete (pkdata->ahd);
    }
    if (pkdata->ctl) {
        delete (pkdata->ctl);
    }
    delete[] (pkdata->data);
    delete (pkdata);
    if (plist && ont_list_size(plist) == 0) {
        ont_disable_rtmp_packet_handle(playctx);
    }
    return 0;
}
