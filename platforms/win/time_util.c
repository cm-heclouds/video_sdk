
#include <time.h>
#include <windows.h>
#include "ont/platform.h"
#include "ont/error.h"


int32_t ont_platform_time()
{
    time_t sec;
    time(&sec);
    return (int32_t)sec;   
}

void ont_platform_sleep(int milliseconds)
{
    Sleep( milliseconds ); 
}


