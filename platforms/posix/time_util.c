#include <time.h>
#include <sys/time.h>

#include "platform.h"
#include "error.h"

extern void usleep(int micro_seconds);

uint32_t ont_platform_time()
{
	time_t sec;
	time(&sec);
	return (uint32_t)sec;
}

void ont_platform_sleep(int milliseconds)
{
	usleep( milliseconds * 1000 );
}



int ont_gettimeofday(struct ont_timeval *_tv, int *_tz)
{

	struct timeval tv;
	struct timezone tz;

	gettimeofday(&tv, &tz);
	_tv->tv_usec = tv.tv_usec;
	_tv->tv_sec = tv.tv_sec;

	return 0;
}



int ont_settimeofday(struct ont_timeval *_tv, int *tz)
{
	struct timeval tv;
	tv.tv_usec = _tv->tv_usec;
	tv.tv_sec = _tv->tv_sec;
	settimeofday(&tv, NULL);
	return 0;
}








