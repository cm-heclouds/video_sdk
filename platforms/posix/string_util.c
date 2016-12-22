#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "ont/platform.h"


extern int vsnprintf(char *str, size_t size, const char *format, va_list ap);

int ont_platform_snprintf(char *str, size_t size, const char *format, ...)
{
    int n=0;
    va_list ap;
    va_start(ap, format);
    n = vsnprintf(str, size, format, ap);
    va_end(ap);
    return n;   
}
