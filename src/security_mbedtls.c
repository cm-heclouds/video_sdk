#ifdef PROTOCOL_SECURITY_MBEDTLS

#include <memory.h>
#include <string.h>
#include <mbedtls/platform.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/pk.h>

#include "config.h"
#include "security.h"
#include "platform.h"
#include "log.h"
#include "error.h"


#define EXPONENT 65537
#define KEY_SIZE 1024


static  char _gSeclib_init = 0;



void *_security_calloc(size_t nmemb, size_t size)
{
	void *addr = NULL;
	addr = (void *)ont_platform_malloc(size * nmemb);
	memset(addr, 0, size * nmemb);
	return addr;
}

int ont_security_init()
{
	if (_gSeclib_init) {
		return ONT_ERR_OK;
	}
	mbedtls_platform_set_calloc_free(_security_calloc, ont_platform_free);

	return ONT_ERR_OK;
}

void ont_security_deinit()
{
	_gSeclib_init = 0;
}

void ont_security_rsa_destroy(void *rsa)
{
	mbedtls_rsa_free(rsa);
	ont_platform_free(rsa);
}


int ont_security_rsa_generate(void **rsa, const char *pub_key, size_t pbk_len,
                              const char *priv_key, size_t pvk_len)
{
	const char *pers = "rsagenerate";
	mbedtls_rsa_context *_rsa;
	mbedtls_ctr_drbg_context ctr_drbg;
	mbedtls_entropy_context  entropy;
	int ret = 0;

	mbedtls_ctr_drbg_init(&ctr_drbg);
	mbedtls_entropy_init(&entropy);

	mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func,
	                      &entropy, (const unsigned char *)pers,
	                      strlen(pers));

	_rsa = ont_platform_malloc(sizeof(mbedtls_rsa_context));
	if (pub_key) {
		mbedtls_rsa_init(_rsa, MBEDTLS_RSA_PKCS_V15, 0);
#if 0 /* type cast to wipe off warning of compiler */
		mbedtls_mpi_read_binary(&_rsa->N, pub_key, pbk_len);
#endif
		mbedtls_mpi_read_binary(&_rsa->N, (const unsigned char *)pub_key, pbk_len);
		mbedtls_mpi_read_string(&_rsa->E, 16, "010001");
	} else {
		mbedtls_rsa_init(_rsa, MBEDTLS_RSA_PKCS_V15, 0);
		if ((ret = mbedtls_rsa_gen_key(_rsa, mbedtls_ctr_drbg_random, &ctr_drbg, KEY_SIZE, EXPONENT)) != 0) {
			RTMP_Log(RTMP_LOGERROR, " failed\n  ! mbedtls_rsa_gen_key returned %d\n\n", ret);
			ret = ONT_ERR_INTERNAL;
			goto __end;
		}
	}
	*rsa = _rsa;

__end:
	mbedtls_ctr_drbg_free(&ctr_drbg);
	mbedtls_entropy_free(&entropy);
	if ( ret != 0) {
		ont_platform_free(_rsa);
	}
	return ret;
}


int ont_security_rsa_get_pubkey(void *rsa, char *pub_key, size_t pbk_len)
{
	mbedtls_rsa_context *_rsa = (mbedtls_rsa_context *)rsa;;
#if 0 /* type cast to wipe off warning of compiler */
	mbedtls_mpi_write_binary(&_rsa->N, pub_key, pbk_len);
#endif
	mbedtls_mpi_write_binary(&_rsa->N, (unsigned char *)pub_key, pbk_len);
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
	const char *pers = "rsagenerate";
	mbedtls_rsa_context *_rsa = rsa;
	mbedtls_ctr_drbg_context ctr_drbg;
	mbedtls_entropy_context  entropy;
	int ret = 0;
	int cnt, block;

	mbedtls_ctr_drbg_init(&ctr_drbg);
	mbedtls_entropy_init(&entropy);
	mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func,
	                      &entropy, (const unsigned char *)pers,
	                      strlen(pers));

	_rsa->len = (mbedtls_mpi_bitlen(&_rsa->N) + 7) >> 3;
	cnt = (in_size + 116) / 117;
	for (block = 0; block < cnt; block++) {
		ret = mbedtls_rsa_pkcs1_encrypt(_rsa, mbedtls_ctr_drbg_random, &ctr_drbg, MBEDTLS_RSA_PUBLIC,
		                                block < cnt - 1 ? 117 : (in_size - block * 117), input + 117 * block, output + 128 * block);
		if (ret != 0) {
			ret = ONT_ERR_ENC_FAIL;
			goto __end;
		}
	}
	*out_size = cnt * 128;

__end:
	mbedtls_ctr_drbg_free(&ctr_drbg);
	mbedtls_entropy_free(&entropy);
	return ret;
}

int ont_security_rsa_decrypt(void *rsa,
                             const unsigned char *input, size_t in_size,
                             unsigned char *output, size_t *out_size)
{
	int ret = 0;
	const char *pers = "rsagenerate";
	mbedtls_rsa_context *_rsa = rsa;
	mbedtls_ctr_drbg_context ctr_drbg;
	mbedtls_entropy_context  entropy;
	int dec_size = 0;
	int cnt, block;
	int outbuf_size = *out_size;

	mbedtls_ctr_drbg_init(&ctr_drbg);
	mbedtls_entropy_init(&entropy);

	mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func,
	                      &entropy, (const unsigned char *)pers,
	                      strlen(pers));

	_rsa->len = 128;
	cnt = in_size / 128;
	for (block = 0; block < cnt; block++) {

		ret = mbedtls_rsa_pkcs1_decrypt(_rsa, mbedtls_ctr_drbg_random,
		                                &ctr_drbg, MBEDTLS_RSA_PRIVATE, out_size, input + 128 * block, output + dec_size, outbuf_size - dec_size);
		if (ret != 0) {
			RTMP_Log(RTMP_LOGERROR, " dec failed %d\n", ret);
			goto __end;
		}
		dec_size += *out_size;
	}
	*out_size = dec_size;
__end:
	mbedtls_ctr_drbg_free(&ctr_drbg);
	mbedtls_entropy_free(&entropy);
	return ret;
}

#endif




