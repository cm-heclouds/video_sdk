#ifdef PROTOCOL_SECURITY_OPENSSL

#include <memory.h>
#include <string.h>
#include <openssl/rsa.h>

#include "config.h"
#include "security.h"
#include "platform.h"
#include "log.h"
#include "error.h"


#define EXPONENT 65537
#define KEY_SIZE 1024



int ont_security_init()
{
	/*do nothing*/
	return ONT_ERR_OK;
}

void ont_security_rsa_destroy(void *rsa)
{
	RSA_free(rsa);
}

int ont_security_rsa_generate(void **rsa, const char *pub_key, size_t pbk_len,
                              const char *priv_key, size_t pvk_len)
{
	RSA *_rsa;
	int ret = 0;

	if (pub_key) {
		BIGNUM *bne = BN_new();
		if (NULL == bne) {
			RTMP_Log(RTMP_LOGERROR, " failed\n  ! openssl_rsa_gen_e failed\n\n");
			ret = ONT_ERR_INTERNAL;
			goto __end;
		}

		BN_set_word(bne, EXPONENT);
		_rsa = RSA_new();
		if (NULL == _rsa) {
			RTMP_Log(RTMP_LOGERROR, " failed\n  ! openssl_rsa_gen_rsa failed\n\n");
			ret = ONT_ERR_INTERNAL;
			BN_free(bne);
			goto __end;
		}

		_rsa->n = BN_bin2bn(pub_key, pbk_len, NULL);
		_rsa->e = bne;
	} else {
		_rsa = RSA_generate_key(KEY_SIZE, EXPONENT, NULL, NULL);
		if (NULL == _rsa) {
			RTMP_Log(RTMP_LOGERROR, " failed\n  ! openssl_rsa_gen_key failed %d\n\n");
			ret = ONT_ERR_INTERNAL;
			goto __end;
		}
	}
	*rsa = _rsa;

__end:
	return ret;
}

int ont_security_rsa_get_pubkey(void *rsa, char *pub_key, size_t pbk_len)
{
	RSA *_rsa = (RSA *)rsa;
	BN_bn2bin(_rsa->n, pub_key);
	return ONT_ERR_OK;
}


/*for 1024 bit of the key*/
int ont_security_rsa_encrypt(void *rsa,
                             const unsigned char *input, size_t in_size,
                             unsigned char *output, size_t *out_size)
{
	if (!rsa || !input || !output) {
		return ONT_ERR_BADPARAM;
	}

	int cnt, block;
	cnt = (in_size + 116) / 117;
	for (block = 0; block < cnt; block++) {
		if ((RSA_public_encrypt(block < cnt - 1 ? 117 : (in_size - block * 117), input + 117 * block, output + 128 * block, rsa, RSA_PKCS1_PADDING)) < 0) {
			return ONT_ERR_ENC_FAIL;
		}
	}

	*out_size = cnt * 128;
	return ONT_ERR_OK;
}

int ont_security_rsa_decrypt(void *rsa,
                             const unsigned char *input, size_t in_size,
                             unsigned char *output, size_t *out_size)
{
	int dec_size = 0;
	int dec_block_size = 0;
	int cnt, block;

	cnt = in_size / 128;
	for (block = 0; block < cnt; block++) {
		dec_block_size = RSA_private_decrypt(128, input + 128 * block, output + dec_size, rsa, RSA_PKCS1_PADDING);
		if (dec_block_size < 0) {
			RTMP_Log(RTMP_LOGERROR, " dec failed %d\n", dec_block_size);
			return ONT_ERR_DEC_FAIL;
		}
		dec_size += dec_block_size;
	}

	*out_size = dec_size;
	return ONT_ERR_OK;
}


#endif









