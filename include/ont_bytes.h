#ifndef ONT_INCLUDE_ONT_BYTES_H_
#define ONT_INCLUDE_ONT_BYTES_H_


#ifdef __cplusplus
extern "C" {
#endif


	void ont_encodeInt16(char *output, uint16_t nVal);
	void ont_encodeInt32(char *output, uint32_t nVal);
	void ont_encodeInt64(char *output, uint64_t nVal);
	void ont_encodeInt24(char *output, uint32_t nVal);

	uint16_t ont_decodeInt16(const char *data);
	uint32_t ont_decodeInt24(const char *data);
	uint32_t ont_decodeInt32(const char *data);
	uint64_t ont_decodeInt64(const char *data);



#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ONT_INCLUDE_ONT_BYTES_H_ */
