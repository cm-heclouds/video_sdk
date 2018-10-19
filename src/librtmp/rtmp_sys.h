#ifndef __RTMP_SYS_H__
#define __RTMP_SYS_H__
/*
 *      Copyright (C) 2010 Howard Chu
 *
 *  This file is part of librtmp.
 *
 *  librtmp is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2.1,
 *  or (at your option) any later version.
 *
 *  librtmp is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with librtmp see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *  http://www.gnu.org/copyleft/lgpl.html
 */

#ifdef _WIN32
#ifdef _MSC_VER	/* MSVC */
#define strcasecmp stricmp
#define strncasecmp strnicmp
#endif
#include <WinSock2.h>
#endif

/* add headers for voiding implicit declaration of functions on linuex,
 * such as ntohl */
#ifdef __linux__
#include <arpa/inet.h>
#endif

#include "rtmp.h"


#ifdef PROTOCOL_RTMP_CRYPT


#ifdef PROTOCOL_SECURITY_MBEDTLS

#include "mbedtls/net.h"
#include "mbedtls/ssl.h"
#include "mbedtls/havege.h"
typedef struct tls_ctx {
	mbedtls_havege_state hs;
	mbedtls_ssl_session ssn;
} tls_ctx;
#define TLS_CTX tls_ctx *
#define TLS_client(ctx,s)	s = ont_platform_malloc(sizeof(ssl_context)); mbedtls_ssl_init(s);\
	mbedtls_ssl_conf_endpoint(s, MBEDTLS_SSL_IS_CLIENT); mbedtls_ssl_conf_authmode(s, MBEDTLS_SSL_VERIFY_NONE);\
	mbedtls_ssl_conf_rng(s, mbedtls_havege_random, &ctx->hs);\
	mbedtls_ssl_conf_ciphersuites(s, mbedtls_ssl_list_ciphersuites);\
	mbedtls_ssl_set_session(s, 1, 600, &ctx->ssn)
#define TLS_setfd(s,fd)	mbedtls_ssl_set_bio(s, mbedtls_net_recv, &fd, mbedtls_net_send, &fd)
#define TLS_connect(s)	mbedtls_ssl_handshake(s)
#define TLS_read(s,b,l)	mbedtls_ssl_read(s,(unsigned char *)b,l)
#define TLS_write(s,b,l)	mbedtls_ssl_write(s,(unsigned char *)b,l)
#define TLS_shutdown(s)	mbedtls_ssl_close_notify(s)
#define TLS_close(s)	mbedtls_ssl_free(s); ont_platform_free(s)
#elif defined(PROTOCOL_SECURITY_OPENSSL)
#define TLS_CTX	SSL_CTX *
#define TLS_client(ctx,s)	s = SSL_new(ctx)
#define TLS_setfd(s,fd)	SSL_set_fd(s,fd)
#define TLS_connect(s)	SSL_connect(s)
#define TLS_read(s,b,l)	SSL_read(s,b,l)
#define TLS_write(s,b,l)	SSL_write(s,b,l)
#define TLS_shutdown(s)	SSL_shutdown(s)
#define TLS_close(s)	SSL_free(s)
#else
#error ""
#endif

#endif

#endif
