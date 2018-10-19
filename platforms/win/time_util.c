#include <time.h>
#include <windows.h>
#include <winnt.h>
#include <sys/timeb.h>
#include "platform.h"
#include "error.h"


uint32_t ont_platform_time()
{
	time_t sec;
	time(&sec);
	return (uint32_t)sec;
}

void ont_platform_sleep(int milliseconds)
{
	Sleep( milliseconds );
}



int ont_gettimeofday(struct ont_timeval *tv, int *tz)
{
	if (tv) {
		FILETIME ft;
		uint64_t ns = 0;

#ifdef _WIN32_WCE
		SYSTEMTIME st;

		GetSystemTime(&st);
		SystemTimeToFileTime(&st, &ft);
#else
		GetSystemTimeAsFileTime(&ft);
#endif

		ns |= ft.dwHighDateTime;
		ns <<= 32;
		ns |= ft.dwLowDateTime;
		ns /= 10;
		ns -= 11644473600000000ULL;
		tv->tv_sec = (long)(ns / 1000000UL);
		tv->tv_usec = (long)(ns % 1000000UL);
	}
	return 0;
}


int ont_settimeofday(struct ont_timeval *tp, int *tz)
{
	struct tm *t;
	int result;
	int error;
	t = localtime(&tp->tv_sec);
	SYSTEMTIME systm;
	systm.wYear = t->tm_year;
	systm.wMonth = t->tm_mon;
	systm.wDay = t->tm_mday;
	systm.wHour = t->tm_hour;
	systm.wMilliseconds = tp->tv_usec / 1000;
	systm.wMinute = t->tm_min;
	systm.wSecond = t->tm_sec;
	systm.wDayOfWeek = t->tm_wday;
	result = SetLocalTime(&systm);/*need administrator privilege*/
	error = GetLastError();

	return result != 0 ? ONT_ERR_OK : ONT_ERR_INTERNAL;
}








