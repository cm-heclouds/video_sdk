
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <errno.h>
#include <stdio.h>


#include "ont/error.h"
#include "ont/platform.h"
#include "port_sock.h"


int ont_platform_tcp_create(ont_socket_t **sock)
{
    u_long flags = 1;
    int ret = NO_ERROR;
    DWORD returned = 0;  
    ont_socket_t *_sock;
    
    initialize_environment();
    
    if ( !sock)
    {
        return ONT_ERR_BADPARAM;   
    }

    _sock =(ont_socket_t*)ont_platform_malloc(sizeof(ont_socket_t));

    _sock->fd = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, 0 );        
    if ( _sock->fd < 0 )        
    {   
        ont_platform_free(_sock);
        return ONT_ERR_SOCKET_OP_FAIL;        
    }  
    
    
    ret = WSAIoctl( _sock->fd, FIONBIO, &flags, sizeof(flags), NULL, 0, &returned, NULL, NULL );        
    if ( NO_ERROR != ret )        
    {            
        
        return ONT_ERR_SOCKET_OP_FAIL;       
    }
    *sock = _sock;
    return ONT_ERR_OK;
}

int ont_platform_tcp_connect(ont_socket_t *sock, const char *ip, uint16_t port)
{
    SOCKET _sock=0;
    struct sockaddr_in saClient;
    int ret = 0;

    if ( !sock )
    {
        return ONT_ERR_BADPARAM;   
    }
    
    _sock  = sock->fd;
    memset(&saClient, 0x00, sizeof(struct sockaddr_in));
    saClient.sin_family  = AF_INET;
    saClient.sin_port = htons(port);
    saClient.sin_addr.s_addr = inet_addr(ip);;
    

    if (SOCKET_ERROR == WSAConnect(_sock, (const struct sockaddr *)(&saClient), sizeof(saClient), NULL, NULL, NULL, NULL))
    {       
        if ( WSAEWOULDBLOCK == WSAGetLastError() || WSAEALREADY == WSAGetLastError())
        {  
            return ONT_ERR_SOCKET_INPROGRESS; /*check  uplayer*/   
        }        
        else if ( WSAEISCONN == WSAGetLastError() )      
        { 
            return ONT_ERR_SOCKET_ISCONN;       
        }  
        else      
        {            
            return ONT_ERR_SOCKET_OP_FAIL;
        }  
    }           

    
    return ONT_ERR_OK;      
}

int ont_platform_tcp_send(ont_socket_t *sock, const char *buf,
                          unsigned int size, unsigned int *bytes_sent)
{
    int ret;   
    WSABUF wsb;    
    DWORD sent = 0; 
    SOCKET _sock=0;
    if ( !buf || !size || !bytes_sent || !sock ) 
    {
        return ONT_ERR_BADPARAM;
    }
    _sock  = sock->fd;
    
    wsb.buf = (char*)buf;   
    wsb.len = size;    
    ret = WSASend(_sock , &wsb, 1, &sent, 0,  NULL, NULL );   
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

int ont_platform_tcp_recv(ont_socket_t *sock, char *buf,
                          unsigned int size, unsigned int *bytes_read)
{
    int ret;   
    WSABUF wsb;   
    DWORD recvd = 0;  
    DWORD flags = 0;
    SOCKET _sock = 0;
    
    if ( !buf || !size || !bytes_read || !sock ) 
    {
        return ONT_ERR_BADPARAM; 
    }
    _sock  = sock->fd;
    wsb.buf = buf;   
    wsb.len = size;    
    ret = WSARecv(_sock, &wsb, 1, &recvd, &flags,  NULL, NULL );  
    if ( NO_ERROR == ret )    
    {       
        *bytes_read = (int)recvd;            
        return ONT_ERR_OK;        
    }    
    else if ( WSAEWOULDBLOCK == WSAGetLastError() )    
    {        
        *bytes_read = 0;        
        return ONT_ERR_OK;   
    }    
    return ONT_ERR_SOCKET_OP_FAIL;        
}


int ont_platform_tcp_close(ont_socket_t *sock)
{
    if ( !sock)
    {
        return ONT_ERR_BADPARAM;   
    }
    closesocket(sock->fd);
    ont_platform_free(sock);

    return ONT_ERR_OK;
}


