#include <WinSock2.h>
#include <fcntl.h>
#include <stdlib.h>
#include "ont/platform.h"

static int _gInitStatus = FALSE;
/* initialize some environment about current platform */
extern int initialize_environment( void )
{
    WSADATA wsa;
    int err = 0;
    if (_gInitStatus)
    {
        return 0;
    }

    err = WSAStartup( MAKEWORD(2,2), &wsa );
    if (0 != err)
    {
        return -1;
    }

    _set_fmode( _O_BINARY );

    _gInitStatus = TRUE;

    return 0;
}

/* cleanup some environment abount current platform*/
extern void cleanup_environment( void )
{
    if (_gInitStatus)
    {
        WSACleanup();
    }
    _gInitStatus = FALSE;
}