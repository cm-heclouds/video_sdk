#include "ont/log.h"


static void ont_log_defualt_log(void *context, ont_log_level_t ll,
                                const char *format, ...)
{
    (void)context;
    (void)ll;
    (void)format;
}

ont_logger_t ont_logger = {
    ONTLL_DEBUG,
    NULL,
    ont_log_defualt_log
};

