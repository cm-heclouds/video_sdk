#ifndef ONT_INCLUDE_ONT_SECURITY_H_
#define ONT_INCLUDE_ONT_SECURITY_H_

#ifdef __cpluplus
extern "C" {
#endif



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
 * @param  rsa [OUT] 保存rsa加解密实例
 * @param  pub_key[in/out]  rsa公钥
 * @param  pbk_len[in/out]  rsa公钥长度
 * @param  priv_key         rsa私钥
 * @param  pvk_len  rsa私钥长度
 * @return 成功则返回ONT_ERR_OK
 * @remark 公钥、私钥为PEM格式， padding 采用pkcs1
 */
int ont_security_rsa_generate(void **rsa, const char *pub_key, size_t pbk_len,
                            const char *priv_key, size_t pvk_len);


int ont_security_rsa_get_pubkey(void *rsa, char *pub_key, size_t pbk_len);

/**
 * 销毁rsa加解密实例
 * @param rsa 将要被销毁的rsa加解密实例
 */
void ont_security_rsa_destroy(void *rsa);


/**
 * 加密数据
 * @param  rsa 加解密实例
 * @param  input 将要被加密的数据
 * @param  in_size 要被加密数据的长度，必须小于公钥字节数减去11
 * @param  output [OUT]存储加密后的数据
 * @param  out_size [OUT]存储加密后的数据的字节数
 * @return 成功则返回ONT_ERR_OK
 */
int ont_security_rsa_encrypt(void *rsa,
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
int ont_security_rsa_decrypt(void *rsa,
                             const unsigned char *input, size_t in_size,
                             unsigned char *output, size_t *out_size);
                                  
								  


#ifdef __cpluplus
}  /* for extern "C" */
#endif

#endif /* ONT_INCLUDE_ONT_SECURITY_H_ */