#ifndef ONT_INCLUDE_ONT_SECURITY_H_
#define ONT_INCLUDE_ONT_SECURITY_H_

#ifdef __cpluplus
extern "C" {
#endif

#include "config.h"

#ifdef ONT_SECURITY_SUPPORT

/**
 * 初始化加密库
 * @return 成功则返回ONT_ERR_OK
 */
int ont_security_init();

/**
 * 销毁加密库
 */
void ont_security_deinit();

/**
 * 创建rsa加解密实例
 * @param  pub_key  rsa公钥
 * @param  pbk_len  rsa公钥长度
 * @param  priv_key rsa私钥
 * @param  pvk_len  rsa私钥长度
 * @param  rsa [OUT] 保存rsa加解密实例
 * @return 成功则返回ONT_ERR_OK
 * @remark 公钥、私钥为PEM格式， padding 采用pkcs1
 */
int ont_security_rsa_create(const char *pub_key, size_t pbk_len,
                            const char *priv_key, size_t pvk_len，
                            ont_security_rsa_t **rsa);

/**
 * 销毁rsa加解密实例
 * @param rsa 将要被销毁的rsa加解密实例
 */
void ont_security_rsa_destroy(ont_security_rsa_t *rsa);

/**
 * 加密数据
 * @param  rsa 加解密实例
 * @param  input 将要被加密的数据
 * @param  in_size 要被加密数据的长度，必须小于公钥字节数减去11
 * @param  output [OUT]存储加密后的数据
 * @param  out_size [OUT]存储加密后的数据的字节数
 * @return 成功则返回ONT_ERR_OK
 */
int ont_security_rsa_encrypt(ont_security_rsa_t *rsa,
                             const unsigned char *input, size_t in_size,
                             unsigned char *output, size_t *out_size);

/**
 * RSA解密数据
 * @param  rsa  加解密实例
 * @param  input 将要被解密的数据
 * @param  in_size 要被解密数据的长度，必须和公钥字节数相同
 * @param  output [OUT] 存储解密后的数据
 * @param  out_size [OUT] 存储解密后数据的字节数
 * @return 解密成功则返回ONT_ERR_OK
 */
int ont_security_rsa_decrypt(ont_security_rsa_t *rsa,
                             const unsigned char *input, size_t in_size,
                             unsigned char *output, size_t *out_size);
                                  
/**
 * 创建AES加解密实例，并自动生成密钥， AES/CBC
 * @param bytes AES密钥的长度，必须是16、24或32字节
 * @param aes [OUT] 保存创建后的AES加解密实例
 * @return 成功则返回ONT_ERR_OK
 */
int ont_security_aes_create(size_t bytes, ont_security_aes_t **aes);

/**
 * 通过指定密钥的方式，创建AES加解密实例， AES/CBC
 * @param  key   AES密钥
 * @param  bytes AES密钥长度，必须是16、24或32字节
 * @param  aes [OUT] 保存创建后的AES加解密实例
 * @return 成功则返回ONT_ERR_OK
 */
int ont_security_aes_create_ex(const char *key, size_t bytes,
                               ont_security_aes_t **aes);

/**
 * 销毁AES加解密实例
 * @param aes 将要被销毁的AES加解密实例
 */
void ont_security_aes_destroy(ont_security_aes_t *aes);

/**
 * 获取AES密钥
 * @param aes AES加解密实例
 * @param key [OUT] 保存AES密钥，长度必须大于等于密钥长度
 */
void ont_security_aes_get_key(ont_security_aes_t *aes, char *key);

/**
 * 用AES加密数据, CBC模式，初始化向量为0
 * @param aes AES加解密实例
 * @param input 被加密的数据块
 * @param output [OUT] 存储加密后的数据块
 */
void ont_security_aes_encrypt(ont_security_aes_t *aes,
                              const unsigned char input[16],
                              unsigned char output[16]);

/**
 * 用AES解密数据, CBC模式
 * @param aes AES加解密实例
 * @param input 被解密的数据块
 * @param output [OUT] 存储解密后的数据块
 */
void ont_security_aes_decrypt(ont_security_aes_t *aes,
                              const unsigned char input[16],
                              unsigned char output[16]);

#endif /* ONT_SECURITY_SUPPORT */

#ifdef __cpluplus
}  /* for extern "C" */
#endif

#endif /* ONT_INCLUDE_ONT_SECURITY_H_ */