#ifndef ONT_SRC_MODBUS_UTILS_H_
#define ONT_SRC_MODBUS_UTILS_H_

#include "ont/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ONT_PROTOCOL_MODBUS

/**
 * 计算以'\0'结尾的字符串长度
 * @param str 以'\0'结尾的字符串
 * @return 返回字符串长度
**/
int32_t ont_strlen( const char *str );

/**
 * 字符串拷贝
 * @param dst 目标字符串
 * @param src 源字符串
 * @return 返回字符串dst
**/
char* ont_strcpy( char *dst, const char *src );

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
                    uint32_t size );

/**
 * 在字符串中查找字符
 * @param str 源字符串
 * @param ch  查找字符
 * @return 存在返回字符所在地址,不存在返回0
**/
const char* ont_strchr( const char *str, char ch );

/**
* 在字符串中查找字符
* @param str 源字符串
* @param size 源字符串大小
* @param ch  查找字符
* @return 存在返回字符所在地址,不存在返回0
**/
const char* ont_strchr_s( const char *str,
                          uint32_t size,
                          char ch );

/**
 * 在字符串中查找字符串
 * @param str 源字符串
 * @param sub 查找字符串
 * @return 存在返回字符串所在地址，不存在返回0
**/
const char* ont_strstr( const char *str, const char *sub );


/**
 * 在字符串中查找字符串
 * @param str 源字符串
 * @param size 源字符串大小
 * @param sub 查找字符串
 * @return 存在返回字符串所在地址，不存在返回0
**/
const char* ont_strstr_s( const char *str,
                          uint32_t size,
                          const char *sub );

/**
 * （十进制）字符串转换成int32_t
 * @param str 源字符串
 * @return 返回字符串对应int32_t
 * @remark 如果字符串无效，返回0
**/
int32_t ont_stoi( const char *str );

/**
 * （十进制）字符串转换成int64
 * @param val [OUT] int64值
 * @param str 源字符串
**/
void ont_stoi64( int32_t val[2], const char *str );

/**
 * int32_t转字符串（十进制）
 * @param str 目标字符串
 * @param val int32_t值
 * @return 返回str
**/
char* ont_itos( char *str, int32_t val );

/**
* int64转字符串（十进制）
* @param str 目标字符串
* @param val int64值
* @return 返回str
**/
char* ont_i64tos( char *str, const int32_t val[2] );

/**
 * 内存拷贝
 * @param dst 目标内存
 * @param src 源内存
 * @param size 拷贝字节数
 * @return 返回dst
**/
void* ont_memmove( void *dst,
                   const void *src,
                   uint32_t size );

/**
 * 清空内存，置0
 * @param mem 内存
 * @param size 内存大小
 * @return 返回mem
**/
void *ont_memzero( void *mem, uint32_t size );

/**
 * 字符串拷贝函数
 * @param str 输入'\0'结尾的字符串
 * @return 拷贝成功返回非NULL字符串指针
 * @remark 成功拷贝的字符串不再使用时，使用@see ont_platform_free释放内存
**/
char* ont_strdup( const char *str );

#endif /* ONT_PROTOCOL_MODBUS */

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* _MODBUS_UTILS_H */