#if !(PROTOCOL_SECURITY_OPENSSL||PROTOCOL_SECURITY_MBEDTLS)
#include "platform.h"
#include "config.h"
#include "security.h"
#include "log.h"
#include "error.h"



int ont_security_init()
{
	/*do nothing*/
	return ONT_ERR_OK;
}

void ont_security_rsa_destroy(void *rsa)
{

}

int ont_security_rsa_generate(void **rsa, const char *pub_key, size_t pbk_len,
                              const char *priv_key, size_t pvk_len)
{
	return ONT_ERR_OK;
}

int ont_security_rsa_get_pubkey(void *rsa, char *pub_key, size_t pbk_len)
{
	return ONT_ERR_OK;
}


/*for 1024 bit of the key*/
int ont_security_rsa_encrypt(void *rsa,
                             const unsigned char *input, size_t in_size,
                             unsigned char *output, size_t *out_size)
{

	return ONT_ERR_OK;
}

int ont_security_rsa_decrypt(void *rsa,
                             const unsigned char *input, size_t in_size,
                             unsigned char *output, size_t *out_size)
{

	return ONT_ERR_OK;
}


#endif









