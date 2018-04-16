#include <time.h>
#include <windows.h>
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


