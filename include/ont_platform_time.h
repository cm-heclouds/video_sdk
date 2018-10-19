#ifndef ONT_INCLUDE_TIME_H_
#define ONT_INCLUDE_TIME_H_

# ifdef __cplusplus
extern "C" {
# endif

#include "config.h"


/**
 * 获取当前时间，单位秒
 * @return 返回距离某固定时间点的秒数
 */
uint32_t ont_platform_time();

/**
 * 睡眠 milliseconds 毫秒
 * @param  milliseconds 要睡眠的时间，单位：毫秒
 */
void ont_platform_sleep(int milliseconds);

struct ont_timeval {
	uint64_t  tv_sec;
	int tv_usec;
};

/*get time*/
int ont_gettimeofday(struct ont_timeval *val, int *zp);


/*set time*/
int ont_settimeofday(struct ont_timeval *val, int *zp);


# ifdef __cplusplus
}      /* extern "C" */
# endif

#endif /* ONT_INCLUDE_TIME_H_ */

