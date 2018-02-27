#include "platform.h"
#include "ont_bytes.h"

void ont_encodeInt16(char *output, uint16_t nVal)
{
	output[1] = nVal & 0xff;
	output[0] = nVal >> 8;
}

void ont_encodeInt32(char *output, uint32_t nVal)
{
	output[3] = nVal & 0xff;
	output[2] = nVal >> 8;
	output[1] = nVal >> 16;
	output[0] = nVal >> 24;
}

void ont_encodeInt64(char *output, uint64_t nVal)
{
	output[7] = (char)(nVal & 0xff);
	output[6] = (char)(nVal >> 8);
	output[5] = (char)(nVal >> 16);
	output[4] = (char)(nVal >> 24);
	output[3] = (char)(nVal >> 32);
	output[2] = (char)(nVal >> 40);
	output[1] = (char)(nVal >> 48);
	output[0] = (char)(nVal >> 56);
}


uint16_t ont_decodeInt16(const char *data)
{
	unsigned char *c = (unsigned char *)data;
	uint16_t val;
	val = (c[0] << 8) | c[1];
	return val;
}

void ont_encodeInt24(char *output, uint32_t nVal)
{
	output[2] = nVal & 0xff;
	output[1] = nVal >> 8;
	output[0] = nVal >> 16;
}

uint32_t  ont_decodeInt32(const char *data)
{
	unsigned char *c = (unsigned char *)data;
	uint32_t      val;
	val = (c[0] << 24) | (c[1] << 16) | (c[2] << 8) | c[3];
	return val;
}


uint64_t  ont_decodeInt64(const char *data)
{
	unsigned char *c = (unsigned char *)data;
	uint64_t      val;
	val = (uint64_t)c[0]  << 56 |
		   (uint64_t)c[1] << 48 | 
		   (uint64_t)c[2] << 40 | 
		   (uint64_t)c[3] << 32 |
		   (uint64_t)c[4] << 24 |
		   (uint64_t)c[5] << 16 | 
		   (uint64_t)c[6] << 8  |
		   c[7];

	return val;
}


uint32_t ont_decodeInt24(const char *data)
{
	unsigned char *c = (unsigned char *)data;
	uint32_t      val;
	val = (uint32_t)c[0] << 16 |
		(uint32_t)c[1] << 8 |
		(uint32_t)c[2];

	return val;
}

