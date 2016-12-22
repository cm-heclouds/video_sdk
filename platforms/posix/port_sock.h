#ifndef ONT_PLATFORM_POSIX_H_
#define ONT_PLATFORM_POSIX_H_

#ifdef __cplusplus
extern "C" {
#endif

struct ont_socket_t
{
    int fd;
    char ip[40]; /*remote ip address*/
    uint16_t port; /*remote port*/
};

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ONT_PLATFORM_POSIX_H_ */