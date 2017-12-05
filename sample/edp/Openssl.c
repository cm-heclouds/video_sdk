#include "Openssl.h"
#include <stdio.h>
#include <ctype.h>
#include "ont/platform.h"
#define AES_KEY_LEN                             128

static AES_KEY g_aes_encrypt_key;
static AES_KEY g_aes_decrypt_key;


int MD5String(const unsigned char* key,unsigned char* result)
{
    if(key == NULL || result == NULL)
    {
        return -1;
    }
    MD5(key,strlen((const char*)key),result);
    return 0;
}

int InitAes(const unsigned char* key)
{
    if(key == NULL)
    {
        return -1;
    }
    if(AES_set_encrypt_key(key, AES_KEY_LEN, &g_aes_encrypt_key) < 0) {
        return -1; 
    }   
    if(AES_set_decrypt_key(key, AES_KEY_LEN, &g_aes_decrypt_key) < 0) {
        return -1; 
    } 
    return 0;
}

int Aes_encrypt(const char* data , int32_t * data_len)
{
    uint32_t in_len = 0;
    unsigned char* in = NULL;
    unsigned char* out = NULL;
    size_t div = 0;
    size_t mod = 0;
    size_t adder = 0;
    size_t block = 0;
    size_t padding_len = 0;
    unsigned char* padding_addr = NULL;
    if(data == NULL)
    {
        return -1;
    }
    in_len = *data_len;
    in = (unsigned char*)data;
    out = in;
    div = in_len / AES_BLOCK_SIZE;
    mod = in_len % AES_BLOCK_SIZE;

    padding_len = AES_BLOCK_SIZE - mod;
    padding_addr = in + div * AES_BLOCK_SIZE;
    if (mod){
        padding_addr += mod;
    }
    ++div;
      memset(padding_addr, padding_len, padding_len);

    for (block=0; block<div; ++block){
        adder = block * AES_BLOCK_SIZE;
        AES_encrypt(in + adder, out + adder, &g_aes_encrypt_key);
    }
    *data_len = block * AES_BLOCK_SIZE;
    return 0;
}

int Aes_decrypt(const char* data , int32_t * data_len)
{
    size_t in_len = 0;
    unsigned char* in = NULL;
    unsigned char* out = NULL;
    size_t offset = 0;
    size_t padding_len = 0;
     if(data == NULL)
    {   
        return -1; 
    }   

    in_len = *data_len;
    in = (unsigned char*)data;
    out = in; 
    for (offset=0; (offset+AES_BLOCK_SIZE)<=in_len; offset+=AES_BLOCK_SIZE){
        AES_decrypt(in + offset, out + offset, &g_aes_decrypt_key);
    }   

    padding_len = *(in + offset -1) - 0;
    *(in+*data_len-padding_len)='\0';
    if (padding_len > AES_BLOCK_SIZE){
        return -1; 
    }   
    return 0;
}
char * Base64Encode(const char * input, int length, int8_t with_new_line)  
{  
    BIO * bmem = NULL;  
    BIO * b64 = NULL;  
    BUF_MEM * bptr = NULL;  
    char* buff = NULL;
    b64 = BIO_new(BIO_f_base64());  
    if(!with_new_line) {  
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);  
    }  
    bmem = BIO_new(BIO_s_mem());  
    b64 = BIO_push(b64, bmem);  
    BIO_write(b64, input, length);  
    BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);  

    buff = (char *)ont_platform_malloc(bptr->length + 1);  
    memcpy(buff, bptr->data, bptr->length);  
    buff[bptr->length] = 0;  

    BIO_free_all(b64);  

    return buff;  
}  

char * Base64Decode(const char * input, int length, int8_t with_new_line)  
{  
    BIO * b64 = NULL;  
    BIO * bmem = NULL;  
    char * buffer = (char *)ont_platform_malloc(length);  
    memset(buffer, 0, length);  

    b64 = BIO_new(BIO_f_base64());  
    if(!with_new_line) {  
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);  
    }  
    bmem = BIO_new_mem_buf((void *)input, length);  
    bmem = BIO_push(b64, bmem);  
    BIO_read(bmem, buffer, length);  

    BIO_free_all(bmem);  

    return buffer;  
} 

void ByteToHexStr(const unsigned char* source, char* dest, int sourceLen)  
{  
    size_t i;  
    unsigned char highByte, lowByte;  
  
    for (i = 0; i < sourceLen; i++)  
    {  
        highByte = source[i] >> 4;  
        lowByte = source[i] & 0x0f ;  
  
        highByte += 0x30;  
  
        if (highByte > 0x39)  
                dest[i * 2] = highByte + 0x07;  
        else  
                dest[i * 2] = highByte;  
  
        lowByte += 0x30;  
        if (lowByte > 0x39)  
            dest[i * 2 + 1] = lowByte + 0x07;  
        else  
            dest[i * 2 + 1] = lowByte;  
    }  
    return ;  
}

void HexStrToByte(const char* source, unsigned char* dest, int sourceLen)  
{  
    size_t i;  
    unsigned char highByte, lowByte;  
      
    for (i = 0; i < sourceLen; i += 2)  
    {  
        highByte = toupper(source[i]);  
        lowByte  = toupper(source[i + 1]);  
  
        if (highByte > 0x39)  
            highByte -= 0x37;  
        else  
            highByte -= 0x30;  
  
        if (lowByte > 0x39)  
            lowByte -= 0x37;  
        else  
            lowByte -= 0x30;  
  
        dest[i / 2] = (highByte << 4) | lowByte;  
    }  
    return ;  
}
char* Zuwang_encrypt(const char* ct)
{
    int32_t aes_len = (strlen(ct)/16)*16+16+16;
    char* aes_data = (char*)ont_platform_malloc(aes_len*sizeof(char));
    char* aes_data_hex = (char*)ont_platform_malloc(2*aes_len*sizeof(char));
    char* result = NULL;

    memcpy(aes_data,ct,strlen(ct));
    aes_len = strlen(ct);
    Aes_encrypt(aes_data,&aes_len);
    ByteToHexStr((const unsigned char*)aes_data,aes_data_hex,aes_len);
    result = Base64Encode(aes_data_hex,aes_len*2,0);

    ont_platform_free(aes_data);
    ont_platform_free(aes_data_hex);
    return result;
}
char* Zuwang_decrypt(const char* ct)
{
    char* base64 = NULL;
    char* aes_data = NULL;
    int32_t aes_len = 0;

    base64 = Base64Decode(ct,strlen(ct),0);
    aes_len = strlen(base64)/2;
    aes_data = (char*)ont_platform_malloc(aes_len*sizeof(char));
    HexStrToByte(base64,(unsigned char*)aes_data,strlen(base64));
    Aes_decrypt(aes_data,&aes_len);

    ont_platform_free(base64);
    return aes_data;
}

