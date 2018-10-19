#ifndef ONT_INCLUDE_RTMP_RVOD_H_
#define ONT_INCLUDE_RTMP_RVOD_H_

# ifdef __cplusplus
extern "C" {
# endif

#define M_RTMP_CLIENTBUFLEN 4000



/**********************************/
/*远程录像回放暂停                 */
/*ctx,    rtmp回放指针             */
/*paused, 暂停或者恢复播放         */
/*ts, 暂停或者播放时的时间戳        */
/**********************************/
typedef void(*rtmp_pause_notify)(void *ctx, int paused, double ts);

/**********************************/
/*远程录像拖动到指定路劲            */
/*cxt, rtmp回放指针                */
/*ts,  拖动到需要播放的位置        */
/**********************************/
typedef int(*rtmp_seek_notify)(void *ctx, double ts);

/**********************************/
/*通知回放客户端播放完成，停止播放*/
/*cxt, rtmp回放指针                */
/**********************************/
typedef void(*rtmp_stop_notify)(void *ctx);


typedef struct _t_ont_rvod_callbacks {
	rtmp_pause_notify               pause;
	rtmp_seek_notify                seek;
	rtmp_stop_notify                stop;
} t_ont_rvod_callbacks;

typedef struct _t_rtmp_vod_ctx {
	void *rtmp;
	void *dev;
	void *user_data;
	int   status; /*0 created, 1 started, 2 stopeed*/
	t_ont_rvod_callbacks callback;
} t_rtmp_vod_ctx;


t_rtmp_vod_ctx *rtmp_rvod_createctx(void *dev, void *user_data, t_ont_rvod_callbacks *vodcb);
int rtmp_rvod_destoryctx(t_rtmp_vod_ctx *ctx);

int rtmp_rvod_start(t_rtmp_vod_ctx *ctx, const char *pushurl, int timeout);
int rtmp_rvod_stop(t_rtmp_vod_ctx *ctx);

int rtmp_rvod_send_videoeof(t_rtmp_vod_ctx *ctx);


# ifdef __cplusplus
}
# endif

#endif /*ONT_INCLUDE_RTMP_RVOD_H_*/
