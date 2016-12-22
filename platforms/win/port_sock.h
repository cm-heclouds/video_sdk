#ifndef ONT_PLATFORM_WIN_H_
#define ONT_PLATFORM_WIN_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ont_socket_t
{
    SOCKET fd;
    char ip[40];   /*remote ip address*/
    uint16_t port; /*remote port.*/
}ont_socket_t;


extern int initialize_environment( void );

extern void cleanup_environment( void );

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ONT_PLATFORM_WIN_H_ */