/*  RTMPDump - Diffie-Hellmann Key Exchange
 *  Copyright (C) 2009 Andrej Stepanchuk
 *  Copyright (C) 2009-2010 Howard Chu
 *
 *  This file is part of librtmp.
 *
 *  librtmp is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2.1,
 *  or (at your option) any later version.
 *
 *  librtmp is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with librtmp see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *  http://www.gnu.org/copyleft/lgpl.html
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

#ifdef PROTOCOL_SECURITY_MBEDTLS
#include <mbedtls/dhm.h>
typedef mbedtls_mpi * MP_t;
#define MP_new(m)	m = ont_platform_malloc(sizeof(mbedtls_mpi)); mbedtls_mpi_init(m)
#define MP_set_w(mpi, w)	mbedtls_mpi_lset(mpi, w)
#define MP_cmp(u, v)	mbedtls_mpi_cmp_mpi(u, v)
#define MP_set(u, v)	mbedtls_mpi_copy(u, v)
#define MP_sub_w(mpi, w)	mbedtls_mpi_sub_int(mpi, mpi, w)
#define MP_cmp_1(mpi)	mbedtls_mpi_cmp_int(mpi, 1)
#define MP_modexp(r, y, q, p)	mbedtls_mpi_exp_mod(r, y, q, p, NULL)
#define MP_free(mpi)	mbedtls_mpi_free(mpi); ont_platform_free(mpi)
#define MP_gethex(u, hex, res)	MP_new(u); res = mbedtls_mpi_read_string(u, 16, hex) == 0
#define MP_bytes(u)	mbedtls_mpi_size(u)
#define MP_setbin(u,buf,len)	mbedtls_mpi_write_binary(u,buf,len)
#define MP_getbin(u,buf,len)	MP_new(u); mbedtls_mpi_read_binary(u,buf,len)
typedef struct MDH {
  MP_t p;
  MP_t g;
  MP_t pub_key;
  MP_t priv_key;
  long length;
  mbedtls_dhm_context ctx;
} MDH;
#define MDH_new()	calloc(1,sizeof(MDH))
#define MDH_free(vp)	{MDH *_dh = vp; mbedtls_dhm_free(&_dh->ctx); MP_free(_dh->p); MP_free(_dh->g); MP_free(_dh->pub_key); MP_free(_dh->priv_key); free(_dh);}
static int MDH_generate_key(MDH *dh)
{
  unsigned char out[2];
  MP_set(&dh->ctx.P, dh->p);
  MP_set(&dh->ctx.G, dh->g);
  dh->ctx.len = 128;
  mbedtls_dhm_make_public(&dh->ctx, 1024, out, 1, mbedtls_havege_random, &RTMP_TLS_ctx->hs);
  MP_new(dh->pub_key);
  MP_new(dh->priv_key);
  MP_set(dh->pub_key, &dh->ctx.GX);
  MP_set(dh->priv_key, &dh->ctx.X);
  return 1;
}
static int MDH_compute_key(uint8_t *secret, size_t len, MP_t pub, MDH *dh)
{
  MP_set(&dh->ctx.GY, pub);
  mbedtls_dhm_calc_secret(&dh->ctx, secret, len, &len, mbedtls_havege_random, NULL);
  return 0;
}
#elif defined(PROTOCOL_SECURITY_OPENSSL)
#include <openssl/bn.h>
#include <openssl/dh.h>
typedef BIGNUM * MP_t;
#define MP_new(m)	m = BN_new()
#define MP_set_w(mpi, w)	BN_set_word(mpi, w)
#define MP_cmp(u, v)	BN_cmp(u, v)
#define MP_set(u, v)	BN_copy(u, v)
#define MP_sub_w(mpi, w)	BN_sub_word(mpi, w)
#define MP_cmp_1(mpi)	BN_cmp(mpi, BN_value_one())
#define MP_modexp(r, y, q, p)	do {BN_CTX *ctx = BN_CTX_new(); BN_mod_exp(r, y, q, p, ctx); BN_CTX_free(ctx);} while(0)
#define MP_free(mpi)	BN_free(mpi)
#define MP_gethex(u, hex, res)	res = BN_hex2bn(&u, hex)
#define MP_bytes(u)	BN_num_bytes(u)
#define MP_setbin(u,buf,len)	BN_bn2bin(u,buf)
#define MP_getbin(u,buf,len)	u = BN_bin2bn(buf,len,0)
#define MDH	DH
#define MDH_new()	DH_new()
#define MDH_free(dh)	DH_free(dh)
#define MDH_generate_key(dh)	DH_generate_key(dh)
#define MDH_compute_key(secret, seclen, pub, dh)	DH_compute_key(secret, pub, dh)
#else

#endif
#include "log.h"
#include "dhgroups.h"
static int
isValidPublicKey(MP_t y, MP_t p, MP_t q)
{
  int ret = TRUE;
  MP_t bn;
  assert(y);
  MP_new(bn);
  assert(bn);
  MP_set_w(bn, 1);
  if (MP_cmp(y, bn) < 0)
    {
      RTMP_Log(RTMP_LOGERROR, "DH public key must be at least 2");
      ret = FALSE;
      goto failed;
    }
  MP_set(bn, p);
  MP_sub_w(bn, 1);
  if (MP_cmp(y, bn) > 0)
    {
      RTMP_Log(RTMP_LOGERROR, "DH public key must be at most p-2");
      ret = FALSE;
      goto failed;
    }
  if (q)
    {
      MP_modexp(bn, y, q, p);
      if (MP_cmp_1(bn) != 0)
	{
	  RTMP_Log(RTMP_LOGWARNING, "DH public key does not fulfill y^q mod p = 1");
	}
    }
failed:
  MP_free(bn);
  return ret;
}
static MDH *
DHInit(int nKeyBits)
{
  size_t res;
  MDH *dh = MDH_new();
  if (!dh)
    goto failed;
  MP_new(dh->g);

  if (!dh->g)
    goto failed;

  MP_gethex(dh->p, P1024, res);	/* prime P1024, see dhgroups.h */
  if (!res)
    {
      goto failed;
    }

  MP_set_w(dh->g, 2);	/* base 2 */

  dh->length = nKeyBits;
  return dh;

failed:
  if (dh)
    MDH_free(dh);

  return 0;
}

static int
DHGenerateKey(MDH *dh)
{
  size_t res = 0;
  if (!dh)
    return 0;

  while (!res)
    {
      MP_t q1 = NULL;

      if (!MDH_generate_key(dh))
	return 0;

      MP_gethex(q1, Q1024, res);
      assert(res);

      res = isValidPublicKey(dh->pub_key, dh->p, q1);
      if (!res)
	{
	  MP_free(dh->pub_key);
	  MP_free(dh->priv_key);
	  dh->pub_key = dh->priv_key = 0;
	}

      MP_free(q1);
    }
  return 1;
}

/* fill pubkey with the public key in BIG ENDIAN order
 * 00 00 00 00 00 x1 x2 x3 .....
 */

static int
DHGetPublicKey(MDH *dh, uint8_t *pubkey, size_t nPubkeyLen)
{
  int len;
  if (!dh || !dh->pub_key)
    return 0;

  len = MP_bytes(dh->pub_key);
  if (len <= 0 || len > (int) nPubkeyLen)
    return 0;

  memset(pubkey, 0, nPubkeyLen);
  MP_setbin(dh->pub_key, pubkey + (nPubkeyLen - len), len);
  return 1;
}

#if 0	/* unused */
static int
DHGetPrivateKey(MDH *dh, uint8_t *privkey, size_t nPrivkeyLen)
{
  if (!dh || !dh->priv_key)
    return 0;

  int len = MP_bytes(dh->priv_key);
  if (len <= 0 || len > (int) nPrivkeyLen)
    return 0;

  memset(privkey, 0, nPrivkeyLen);
  MP_setbin(dh->priv_key, privkey + (nPrivkeyLen - len), len);
  return 1;
}
#endif

/* computes the shared secret key from the private MDH value and the
 * other party's public key (pubkey)
 */
static int
DHComputeSharedSecretKey(MDH *dh, uint8_t *pubkey, size_t nPubkeyLen,
			 uint8_t *secret)
{
  MP_t q1 = NULL, pubkeyBn = NULL;
  size_t len;
  int res;

  if (!dh || !secret || nPubkeyLen >= INT_MAX)
    return -1;

  MP_getbin(pubkeyBn, pubkey, nPubkeyLen);
  if (!pubkeyBn)
    return -1;

  MP_gethex(q1, Q1024, len);
  assert(len);

  if (isValidPublicKey(pubkeyBn, dh->p, q1))
    res = MDH_compute_key(secret, nPubkeyLen, pubkeyBn, dh);
  else
    res = -1;

  MP_free(q1);
  MP_free(pubkeyBn);

  return res;
}
