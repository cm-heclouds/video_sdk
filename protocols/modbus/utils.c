#include "utils.h"

#ifdef ONT_PROTOCOL_MODBUS

/**
 * 计算以'\0'结尾的字符串长度
 * @param str 以'\0'结尾的字符串
 * @return 返回字符串长度
 **/
int32_t ont_strlen( const char *str ) {
    const char *eos = str;

    if ( eos ) {
        while ( '\0' != *eos ) {
            ++eos;
        }

        return (int)(eos - str);
    }

    return 0;
}

/**
 * 字符串拷贝
 * @param dst 目标字符串
 * @param size 目标字符串空间大小
 * @param src 源字符串
 * @return 返回字符串dst
 **/
char* ont_strcpy( char *dst, const char *src ) {
    char *str = dst;

    if ( str && src ) {
        while ( *src ) {
            *str = *src;
            ++str;
            ++src;
        }

        *str = '\0';
    }

    return dst;
}

/**
 * 字符串比较
 * @param dst 目标字符串
 * @param src 源字符串
 * @param size 比较字符个数
 * @return >返回+值
 *         =返回0
 *         <返回-值
 **/
int32_t ont_strcmp( const char *dst,
                    const char *src,
                    uint32_t size ) {
    if ( dst && src ) {
        while ( *dst &&
                *src &&
                *dst == *src &&
                size ) {
            ++dst;
            ++src;
            --size;
        }

        if ( size > 0 ) {
            return (*dst - *src);
        }

        return 0;
    }

    if ( 0 != dst ) {
        return 1;
    }

    return -1;
}

/**
 * 在字符串中查找字符
 * @param str 源字符串
 * @param ch  查找字符
 * @return 存在返回字符所在地址,不存在返回0
 **/
const char* ont_strchr( const char *str, char ch ) {
    if ( str ) {
        while ( *str && ch != *str ) {
            ++str;
        }

        if ( ch == *str ) {
            return str;
        }
    }

    return (const char*)0;
}

/**
* 在字符串中查找字符
* @param str 源字符串
* @param size 源字符串大小
* @param ch  查找字符
* @return 存在返回字符所在地址,不存在返回0
**/
const char* ont_strchr_s( const char *str,
                          uint32_t size,
                          char ch ) {
    if ( str && size ) {
        while ( *str &&
                size &&
                ch != *str ) {
            ++str;
            --size;
        }

        if ( ch == *str ) {
            return str;
        }
    }

    return (const char*)0;
}

/**
 * 在字符串中查找字符串
 * @param str 源字符串
 * @param sub 查找字符串
 * @return 存在返回字符串所在地址，不存在返回0
 **/
const char* ont_strstr( const char *str, const char *sub ) {
    const char *dst = str;
    const char *src = sub;

    if ( str && sub ) {
        while ( *str ) {
            dst = str;
            src = sub;

            while ( *dst &&
                    *src &&
                    *dst == *src ) {
                ++dst;
                ++src;
            }

            if ( *src ) {
                ++str;
            }
            else {
                return str;
            }
        }
    }

    return (const char*)0;
}

/**
 * 在字符串中查找字符串
 * @param str 源字符串
 * @param size 源字符串大小
 * @param sub 查找字符串
 * @return 存在返回字符串所在地址，不存在返回0
 **/
const char* ont_strstr_s( const char *str,
                          uint32_t size,
                          const char *sub ) {
    const char *dst = str;
    const char *src = sub;
    uint32_t tmp = size;

    if ( str && size && sub ) {
        while ( *str && size ) {
            dst = str;
            src = sub;
            tmp = size;

            while ( *dst &&
                    *src &&
                    tmp &&
                    *dst == *src ) {
                ++dst;
                ++src;
                --tmp;
            }

            if ( *src ) {
                ++str;
                --size;
            }
            else {
                return str;
            }
        }
    }

    return (const char*)0;
}

/**
 * （十进制）字符串转换成int
 * @param str 源字符串
 * @return 返回字符串对应int
 * @remark 如果字符串无效，返回0
 **/
int32_t ont_stoi( const char *str ) {
    int r = 0;
    int neg = 0;

    if ( str ) {
        while ( ' ' == *str ||
                '\r' == *str ||
                '\t' == *str ||
                '\n' == *str ) {
            ++str;
        }

        if ( '-' == *str ) {
            neg = 1;
            ++str;
        }

        do {
            if ( '0' <= *str && '9' >= *str ) {
                r *= 10;
                r += (*str - '0');
                ++str;
            }
            else {
                break;
            }
        }
        while ( *str );
    }

    return (neg ? -r : r);
}

/**
 * （十进制）字符串转换成int64
 * @param val [OUT] int64值
 * @param str 源字符串
 **/
void ont_stoi64( int32_t val[2], const char *str ) {
    int neg = 0;
    int high = 0;
    uint32_t low = 0;
    uint32_t res = 0;

    if ( str ) {
        while ( ' ' == *str ||
                '\r' == *str ||
                '\t' == *str ||
                '\n' == *str ) {
            ++str;
        }

        if ( '-' == *str ) {
            neg = 1;
            ++str;
        }

        do {
            if ( '0' <= *str && '9' >= *str ) {
                high *= 10;
                high += (low >> 31); /*low x 2*/
                high += (low >> 29); /*low x 8*/

                res = (low << 1); /*low x 2*/
                low <<= 3;        /*low x 8*/

                if ( 0xffffffff - res < low ) {
                    high += 1;
                    low = res - (0xffffffff - low) - 1;
                }
                else {
                    low += res;
                }

                res = (*str - '0');
                ++str;

                if ( 0xffffffff - res < low ) {
                    high += 1;
                    low = res - (0xffffffff - low) - 1;
                }
                else {
                    low += res;
                }
            }
            else {
                break;
            }
        }
        while ( *str );
    }

    if ( neg ) {
        high = ~high;
        low = ~low;

        if ( low == 0xffffffff ) {
            low = 0;
            high += 1;
        }
        else {
            low += 1;
        }

        high |= 0x80000000;
    }

    ont_memmove( val, &low, sizeof(low) );
    ont_memmove( val + 1, &high, sizeof(high) );
}

/**
 * int转字符串（十进制）
 * @param str 目标字符串
 * @param val int值
 * @return 返回str
 **/
char* ont_itos( char *str, int val ) {
    int rem = 0;
    char ch = '\0';
    char *dst = str;
    char *eos = str;

    if ( dst ) {
        if ( val < 0 ) {
            val = -val;
            *eos = '-';
            ++eos;
            ++dst;
        }

        do {
            rem = val % 10;
            val = val / 10;

            *eos = '0' + rem;
            ++eos;
        }
        while ( val );
        *eos = '\0';

        /*倒序*/
        --eos;
        while ( dst < eos ) {
            ch = *dst;
            *dst = *eos;
            *eos = ch;

            ++dst;
            --eos;
        }
    }

    return str;
}

/**
* int64转字符串（十进制）
* @param str 目标字符串
* @param val int64值
* @return 返回str
**/
char* ont_i64tos( char *str, const int32_t val[2] ) {
    int res = 0;
    int rem = 0;
    char ch = '\0';
    char *dst = str;
    char *eos = str;
    int high = val[1];
    uint32_t low = val[0];

    if ( dst ) {
        if ( high < 0 ) {
            *eos = '-';
            ++eos;
            ++dst;

            high = ~high;
            low = ~low;
            high &= 0x7fffffff;

            if ( low == 0xffffffff ) {
                low = 0;
                high += 1;
            }
            else {
                low += 1;
            }
        }

        do {
            rem = high % 10;
            high /= 10;

            res = 0xffffffff / 10 * rem;
            rem = 0xffffffff % 10 * rem + rem;
            res += rem / 10;
            rem %= 10;

            res += low / 10;
            rem += low % 10;
            res += rem / 10;
            rem %= 10;
            low = res;

            *eos = '0' + rem;
            ++eos;
        }
        while ( low || high );
        *eos = '\0';

        /*倒序*/
        --eos;
        while ( dst < eos ) {
            ch = *dst;
            *dst = *eos;
            *eos = ch;

            ++dst;
            --eos;
        }
    }

    return str;
}

/**
 * 内存拷贝
 * @param dst 目标内存
 * @param src 源内存
 * @param size 拷贝字节数
 * @return 返回dst
 **/
void* ont_memmove( void *dst,
                   const void *src,
                   uint32_t size ) {
    uint8_t *_dst = (uint8_t*)dst;
    uint8_t *_src = (uint8_t*)src;

    if ( _dst && _src && size ) {
        while ( size ) {
            *_dst = *_src;
            ++_dst;
            ++_src;
            --size;
        }
    }

    return dst;
}

/**
 * 清空内存，置0
 * @param mem 内存
 * @param size 内存大小
 * @return 返回mem
 **/
void *ont_memzero( void *mem, uint32_t size ) {
    uint8_t *dst = (uint8_t*)mem;

    if ( dst && size ) {
        while ( size ) {
            *dst = 0;
            ++dst;
            --size;
        }
    }

    return mem;
}

char* ont_strdup( const char *str ) {
    int32_t len = 0;
    char *ret = NULL;

    if ( NULL != str ) {
        len = ont_strlen( str );
        ret = ont_platform_malloc( len + 1 );
        if ( NULL != ret ) {
            ont_strcpy( ret, str );
        }
    }

    return ret;
}

#endif /* ONT_PROTOCOL_MODBUS */