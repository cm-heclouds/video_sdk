#include "getifaddrs.h"

IPAddress* getIPAddressPtr( SearchIPAddress* searchIpAddressPtr, int* index) {
    int i = *index;
    IPAddress *IPAddressPtr = NULL;

    if (searchIpAddressPtr->list == NULL) {
        searchIpAddressPtr->list = malloc(M_SEARCH_IP_NUMBERS * sizeof(IPAddress));
        memset(searchIpAddressPtr->list, 0x00, M_SEARCH_IP_NUMBERS * sizeof(IPAddress));
        searchIpAddressPtr->number = M_SEARCH_IP_NUMBERS;
    }

    while (i < searchIpAddressPtr->number) {
        if (searchIpAddressPtr->list[i].ip[0] == '\0') {
            IPAddressPtr = &searchIpAddressPtr->list[i++];
            break;
        } else {
            i++;
        }
    }

    *index = i;

    if (IPAddressPtr == NULL) {
        searchIpAddressPtr->list = realloc(searchIpAddressPtr->list, searchIpAddressPtr->number * 2 * sizeof(IPAddress));
        IPAddressPtr = &searchIpAddressPtr->list[searchIpAddressPtr->number];
        memset(searchIpAddressPtr->list, 0x00,  searchIpAddressPtr->number * sizeof(IPAddress));
        searchIpAddressPtr->number = searchIpAddressPtr->number * 2;
    }
    return IPAddressPtr;
}


int getIPAddress(SearchIPAddress*  searchIpAddressPtr)
{
    IPAddress *IPAddressPtr = NULL;
    int i = 0;

#ifdef ONT_OS_POSIX
    struct sockaddr_in *sin = NULL;
    struct ifaddrs *ifa = NULL, *ifList;

    //SearchIPAddress searchIpAddress = {NULL, 0};

    if (getifaddrs(&ifList) < 0)
    {
        return -1;
    }

    for (ifa = ifList; ifa != NULL; ifa = ifa->ifa_next)
    {
        if(ifa->ifa_addr->sa_family == AF_INET)
        {
            IPAddressPtr = getIPAddressPtr(searchIpAddressPtr, &i);
            memcpy(IPAddressPtr->name, ifa->ifa_name, strlen(ifa->ifa_name));

            sin = (struct sockaddr_in *)ifa->ifa_addr;
            char * ifa_addr = inet_ntoa(sin->sin_addr);
            memcpy(IPAddressPtr->ip, ifa_addr, strlen(ifa_addr));

            sin = (struct sockaddr_in *)ifa->ifa_netmask;
            char * ifa_netmask = inet_ntoa(sin->sin_addr);
            memcpy(IPAddressPtr->netmask, ifa_netmask, strlen(ifa_netmask));
        }
    }

    freeifaddrs(ifList);
#endif

#ifdef WIN32
    PIP_ADAPTER_INFO pAdapterInfo;
    PIP_ADAPTER_INFO pAdapter = NULL;
    DWORD dwRetVal = 0;


    ULONG ulOutBufLen = sizeof (IP_ADAPTER_INFO);
    pAdapterInfo = (IP_ADAPTER_INFO *) MALLOC(sizeof (IP_ADAPTER_INFO));
    if (pAdapterInfo == NULL) {
        printf("Error allocating memory needed to call GetAdaptersinfo\n");
        return -1;
    }
// Make an initial call to GetAdaptersInfo to get
// the necessary size into the ulOutBufLen variable
    if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW) {
        FREE(pAdapterInfo);
        pAdapterInfo = (IP_ADAPTER_INFO *) MALLOC(ulOutBufLen);
        if (pAdapterInfo == NULL) {
            printf("Error allocating memory needed to call GetAdaptersinfo\n");
            return -1;
        }
    }

    if ((dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen)) == NO_ERROR) {
        pAdapter = pAdapterInfo;
        while (pAdapter) {
            IPAddressPtr = getIPAddressPtr(searchIpAddressPtr, &i);
            memcpy(IPAddressPtr->name, pAdapter->AdapterName, strlen(pAdapter->AdapterName));
            memcpy(IPAddressPtr->ip, pAdapter->IpAddressList.IpAddress.String, strlen(pAdapter->IpAddressList.IpAddress.String));
            memcpy(IPAddressPtr->netmask, pAdapter->IpAddressList.IpMask.String, strlen(pAdapter->IpAddressList.IpMask.String));
            pAdapter = pAdapter->Next;
        }
    } else {
        return -1;
    }
    if (pAdapterInfo)
        FREE(pAdapterInfo);
#endif

    return 0;
}
