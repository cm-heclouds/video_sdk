#ifndef _OPENSSL_20150801_sadf32gsadfa_H_
#define _OPENSSL_20150801_sadf32gsadfa_H_

#include <stdlib.h>
#include <string.h>
#include <openssl/aes.h>
#include <openssl/md5.h>
#include <openssl/evp.h>  
#include <openssl/bio.h>  
#include <openssl/buffer.h> 
#include <stdint.h>

#define MD5_LENGTH 16

/**
 * @生成字符串的MD5值
 *
 * @param key 目标字符串
 * @param result 目标字符串的MD5值
 *
 * @return 0： success，others：failed
 */
int MD5String(const unsigned char* key,unsigned char* result);

/**
 * @初始化Aes128的加解密密钥
 *
 * @param key Aes128 密钥
 *
 * @return 0： success，others：failed  
 */
int InitAes(const unsigned char* key);

/**
 * @组网设备加密字符串 
 *
 * @param ct 明文
 *
 * @return 密文
 */
char* Zuwang_encrypt(const char* ct);

/**
 * @组网设备解密字符串
 *
 * @param ct 密文
 *
 * @return 明文 
 */
char* Zuwang_decrypt(const char* ct);

/*内部使用*/
int Aes_encrypt(const char* data , int32_t * data_len);
int Aes_decrypt(const char* data , int32_t * data_len);
char * Base64Encode(const char* input, int32_t length,int8_t  with_new_line);  
char * Base64Decode(const char* input, int32_t length, int8_t with_new_line);
void ByteToHexStr(const unsigned char* source, char* dest, int sourceLen);
void HexStrToByte(const char* source,unsigned char* dest, int sourceLen);
#endif
