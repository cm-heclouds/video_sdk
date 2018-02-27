#include <WinSock2.h>
#include <WS2tcpip.h>
#include <errno.h>
#include <stdio.h>

#include "error.h"
#include "platform.h"
#include "port_sock.h"

int ont_platform_udp_create(ont_socket_t **sock)
{
    ont_socket_t *_sock;
	int timeout = 3000;
	int flag = 1;

    _sock = (ont_socket_t*)ont_platform_malloc(sizeof(ont_socket_t));
    _sock->port = ONT_SERVER_PORT;
    sprintf(_sock->ip, "%s", ONT_SERVER_ADDRESS);

    if ((_sock->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        ont_platform_free(_sock);
        return ONT_ERR_SOCKET_OP_FAIL;
    }

	setsockopt(_sock->fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&flag, sizeof(int));
	setsockopt(_sock->fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(int));

    struct sockaddr_in Addr;
    Addr.sin_family = AF_INET;
    Addr.sin_port = 0;
    Addr.sin_addr.s_addr = 0;

    if (bind(_sock->fd, (struct sockaddr *)&Addr, sizeof(Addr)) == -1)
    {
        return ONT_ERR_SOCKET_OP_FAIL;
    }

    *sock = _sock;

    return ONT_ERR_OK;
}

int ont_platform_udp_send(ont_socket_t *sock, char *buf,
                          unsigned int size, unsigned int *bytes_sent)
{
    int ret;
    WSABUF wsb;
    DWORD sent = 0;

    struct sockaddr_in Addr;
    Addr.sin_family = AF_INET;
    Addr.sin_port = htons(sock->port);
    Addr.sin_addr.s_addr = inet_addr(sock->ip);

    if ( !sock->fd)
    {
        return ONT_ERR_SOCKET_OP_FAIL;
    }

    if ( !buf || !size || !bytes_sent || !sock->fd)
    {
        return ONT_ERR_BADPARAM;
    }

    wsb.buf = (char*)buf;
    wsb.len = size;
    ret = WSASendTo(sock->fd, &wsb, 1, &sent, 0, (struct sockaddr *)&Addr, sizeof(Addr), NULL, NULL );

    if ( NO_ERROR == ret )
    {
        *bytes_sent = (int)sent;
        return ONT_ERR_OK;
    }
    else if ( WSAEWOULDBLOCK == WSAGetLastError() )
    {
        *bytes_sent = 0;
        return ONT_ERR_OK;
    }
    return ONT_ERR_SOCKET_OP_FAIL;
}

int ont_platform_udp_recv(ont_socket_t *sock, char *buf,
                          unsigned int size, unsigned int *bytes_read)
{
    int ret;
    WSABUF wsb;
    DWORD recvd = 0;
    DWORD flags = 0;

    if ( !buf || !size || !bytes_read || !sock)
    {
        return ONT_ERR_BADPARAM;
    }

    wsb.buf = buf;
    wsb.len = size;
    ret = WSARecv(sock->fd, &wsb, 1, &recvd, &flags,  NULL, NULL);
    if ( NO_ERROR == ret )
    {
        *bytes_read = (int)recvd;
        return ONT_ERR_OK;
    }
    else if ( WSAEWOULDBLOCK == WSAGetLastError() )
    {
        *bytes_read = 0;
        return ONT_ERR_SOCKET_OP_FAIL;
    }
    return ONT_ERR_SOCKET_OP_FAIL;
}

int ont_platform_udp_close(ont_socket_t *sock)
{
    if ( !sock)
    {
        return ONT_ERR_BADPARAM;
    }
    closesocket(sock->fd);
    ont_platform_free(sock);

    return ONT_ERR_OK;
}