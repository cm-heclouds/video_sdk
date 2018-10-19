#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <arpa/inet.h>


#include "error.h"
#include "platform.h"
#include "port_sock.h"

int ont_platform_udp_create(ont_socket_t **sock)
{
	ont_socket_t *_sock;
	_sock = (ont_socket_t *)ont_platform_malloc(sizeof(ont_socket_t));
	_sock->port = ONT_SERVER_PORT;
	sprintf(_sock->ip, "%s", ONT_SERVER_ADDRESS);

	if ((_sock->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		ont_platform_free(_sock);
		return ONT_ERR_SOCKET_OP_FAIL;
	}

	struct timeval timeout = {3, 0};
	int flag = 1;
	struct sockaddr_in Addr;
	memset(&Addr, 0, sizeof(struct sockaddr_in));
	Addr.sin_family = AF_INET;
	Addr.sin_port = 0;
	Addr.sin_addr.s_addr = 0;
	if (bind(_sock->fd, (struct sockaddr *)&Addr, sizeof(Addr)) == -1) {
		return ONT_ERR_SOCKET_OP_FAIL;
	}
	setsockopt(_sock->fd, SOL_SOCKET, SO_REUSEADDR, (char *)&flag, sizeof(int));
	setsockopt(_sock->fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

	*sock = _sock;

	return ONT_ERR_OK;
}

int ont_platform_udp_send(ont_socket_t *sock, char *buf,
                          unsigned int size, unsigned int *bytes_sent)
{
	int ret;

	struct sockaddr_in Addr;
	memset(&Addr, 0, sizeof(struct sockaddr_in));
	Addr.sin_family = AF_INET;
	Addr.sin_port = htons(sock->port);
	Addr.sin_addr.s_addr = inet_addr(sock->ip);

	if ( !sock->fd) {
		return ONT_ERR_BADPARAM;
	}

	if ( !buf || !size || !bytes_sent || !sock->fd) {
		return ONT_ERR_BADPARAM;
	}

	ret = sendto(sock->fd, buf, size, 0, (struct sockaddr *)&Addr, sizeof(Addr));

	if ( ret >= 0) {
		*bytes_sent = (int)ret;
		return ONT_ERR_OK;
	} else if ( EAGAIN == errno || EINTR == errno ) {
		*bytes_sent = 0;
		return ONT_ERR_OK;
	}
	return ONT_ERR_SOCKET_OP_FAIL;
}

int ont_platform_udp_recv(ont_socket_t *sock, char *buf,
                          unsigned int size, unsigned int *bytes_read)
{
	int ret = 0;

	if ( !buf || !size || !bytes_read || !sock) {
		return ONT_ERR_BADPARAM;
	}

	ret = recv(sock->fd, buf, size, 0);
	if ( ret >= 0 ) {
		*bytes_read = (int)ret;
		return ONT_ERR_OK;
	} else if ( EAGAIN == errno || EINTR == errno ) {
		*bytes_read = 0;
		return ONT_ERR_SOCKET_INPROGRESS;
	}
	return ONT_ERR_SOCKET_OP_FAIL;
}

int ont_platform_udp_close(ont_socket_t *sock)
{
	if ( !sock) {
		return ONT_ERR_BADPARAM;
	}
	close(sock->fd);
	ont_platform_free(sock);

	return ONT_ERR_OK;
}
