#include <sys/time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include <ont/edp.h>
#include <ont/log.h>

static void sample_log(void *ctx, ont_log_level_t ll , const char *format, ...)
{
    static const char *level[] = {
        "DEBUG", "INFO", "WARNING", "ERROR", "FATAL"
    };
    va_list ap;
    struct timeval tv;
    time_t t;
    struct tm *st;

    gettimeofday(&tv, NULL);

    t = tv.tv_sec;
    st = localtime(&t);

    printf("[%02d:%02d:%02d.%03d] [%s] ", st->tm_hour, st->tm_min,
           st->tm_sec, (int)tv.tv_usec/1000, level[ll]);

    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
    printf("\n");
}

int main( int argc, char *argv[] )
{
    ont_device_t *dev;
    int err, i;
    struct hostent *hosts;
    int32_t next_dp_time;

    ont_log_set_logger(NULL, sample_log);

    err = ont_device_create(123, ONTDEV_EDP, "EDP-TEST-DEV", &dev);
    if (ONT_ERR_OK != err) {
        ONT_LOG1(ONTLL_ERROR, "Failed to create device instance, error=%d", err);
        return -1;
    }


    hosts = gethostbyname(ONT_SERVER_ADDRESS);
    if (NULL == hosts)
    {
        ONT_LOG2(ONTLL_ERROR, "Failed to get the IP of the server('%s'), "
                 "for: %d", ONT_SERVER_ADDRESS, h_errno);
        return -1;
    }

    for (i = 0; hosts->h_addr_list[i]; ++i)
    {
        if (AF_INET == hosts->h_addrtype)
            break;
    }

    if (NULL == hosts->h_addr_list[i])
    {
        ONT_LOG0(ONTLL_ERROR, "No invalide address of the server.");
        return -1;
    }

    err = ont_device_connect(dev,
                             inet_ntoa(*(struct in_addr*)hosts->h_addr_list[i]),
                             ONT_SERVER_PORT,
                             "0123456789abcdef",
                             "JTEXT-OneNET;0000005",
                             60);
    if (ONT_ERR_OK != err)
    {
        ont_device_destroy(dev);

        ONT_LOG1(ONTLL_ERROR, "Failed to connect to the server, error=%d", err);
        return -1;
    }

    next_dp_time = (int32_t)time(NULL) + 15;
    while (1)
    {
        err = ont_device_keepalive(dev);
        if (ONT_ERR_OK != err)
        {
            ONT_LOG1(ONTLL_ERROR, "device is not alive, error=%d", err);
            break;
        }

        if (next_dp_time <= time(NULL))
        {
            err = ont_device_add_dp_double(dev, "temperature", 12.78);
            if (ONT_ERR_OK != err)
            {
                ONT_LOG1(ONTLL_ERROR, "Failed to add the [temperature] data point, error=%d", err);
                break;
            }

            err = ont_device_add_dp_int(dev, "speed", 80);
            if (ONT_ERR_OK != err)
            {
                ONT_LOG1(ONTLL_ERROR, "Failed to add [speed] data point, error=%d", err);
                break;
            }

            err = ont_device_send_dp(dev);
            if (ONT_ERR_OK != err)
            {
                ONT_LOG1(ONTLL_ERROR, "Failed to send the data points, error=%d", err);
                break;
            }

            next_dp_time = (int32_t)time(NULL) + 15;
            ONT_LOG0(ONTLL_INFO, "Send data points successfully.");
        }

        usleep(1000 * 50);
    }

    ont_device_destroy(dev);
    return 0;
}
