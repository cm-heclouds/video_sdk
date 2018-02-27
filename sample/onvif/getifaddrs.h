#ifndef _GETIFADDRS_H__
#define _GETIFADDRS_H__

# ifdef __cplusplus
extern "C" {
# endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ONT_OS_POSIX
#include <ifaddrs.h>
#include <arpa/inet.h>
#endif

#ifdef WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#pragma comment(lib, "IPHLPAPI.lib")

#define _WIN32_WINNT 0x501
#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))
#endif

#define M_SEARCH_IP_NUMBERS 4

typedef struct
{
    char name[32];
    char ip[64];
    char netmask[64];
}IPAddress;

typedef struct  {
    IPAddress * list;
    int number;
}SearchIPAddress;

extern int getIPAddress(SearchIPAddress*  searchIpAddressPtr);

# ifdef __cplusplus
}
# endif


#endif
