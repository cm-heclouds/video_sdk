#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "rvod.h"
#include "device.h"
#include "rtmp_if.h"
#include "protocol.h"
#include "security.h"
#include "ont_bytes.h"
#include "cJSON/cJSON.h"
#include "platform_cmd.h"

#define BUFSIZE 78
#define IPSIZE 40

int ont_device_get_acc(ont_device_t *dev, char *token)
{
	int i, ret, index = 0;
	unsigned int bytes_sent = 0;
	unsigned int bytes_read = 0;
	char buf[BUFSIZE];

	ont_socket_t *sock;
	ret = ont_platform_udp_create(&sock);
	if (ret != 0) {
		return ONT_ERR_SOCKET_OP_FAIL;
	}

	int32_t version = 0x6F6E6576;
	char carrier[4] = "";
	ont_encodeInt32(buf, version);
	index += 4;
	ont_encodeInt64(buf + index, dev->product_id);
	index += 8;
	ont_encodeInt64(buf + index, dev->device_id);
	index += 8;
	memcpy(buf + index, carrier, 4);

	ret = ont_platform_udp_send(sock, buf, 24, &bytes_sent);
	if (ret) {
		ont_platform_udp_close(sock);
		return ONT_ERR_SOCKET_OP_FAIL;
	}

	memset(buf, 0, BUFSIZE);


	ret = ont_platform_udp_recv(sock, buf, BUFSIZE, &bytes_read);
	if (ret) {
		ont_platform_udp_close(sock);
		return ONT_ERR_SOCKET_OP_FAIL;
	}

	for (i = 0; i < 32; i++) {
		token[i] = buf[i + 4];
	}
	dev->port = ont_decodeInt16(buf + 36);
	i = 0;
	while (1) {
		dev->acc_ip[i] = buf[i + 38];
		if (!buf[i + 38]) {
			break;
		}
		i++;
	}
	ont_platform_udp_close(sock);
	return ONT_ERR_OK;
}

int ont_device_create(ont_device_t **dev, ont_device_callback_t *cbs)
{
	ont_device_t *_dev;
	if (*dev || !cbs) {
		return ONT_ERR_BADPARAM;
	}
	_dev = ont_platform_malloc(sizeof(ont_device_t));
	if (!_dev) {
		return ONT_ERR_NOMEM;
	}
	memset(_dev, 0x00, sizeof(ont_device_t));

	_dev->seq_snd = ont_platform_time();
	_dev->rcv_buf = ont_platform_malloc(ONT_PROTOCOL_DEFAULT_SIZE);
	if (!_dev->rcv_buf) {
		ont_platform_free(_dev);
		return ONT_ERR_NOMEM;
	}
	_dev->buf_length = ONT_PROTOCOL_DEFAULT_SIZE;
	_dev->rcv_length = 0;
	_dev->device_cbs = ont_platform_malloc(sizeof(ont_device_callback_t));
	if (!_dev->device_cbs) {
		ont_platform_free(_dev->rcv_buf);
		ont_platform_free(_dev);
		return ONT_ERR_NOMEM;
	}
		
	memcpy(_dev->device_cbs, cbs, sizeof(ont_device_callback_t));
	*dev = _dev;
	return ONT_ERR_OK;
}


int ont_device_set_platformcmd_handle(ont_device_t *dev, ont_cmd_callbacks_t *cb)
{
	if (!dev || !cb) {
		return ONT_ERR_BADPARAM;
	}
	dev->ont_cmd_cbs = ont_platform_malloc(sizeof(ont_cmd_callbacks_t));
	if (!dev->ont_cmd_cbs) {
		return ONT_ERR_NOMEM;
	}
	memcpy(dev->ont_cmd_cbs, cb, sizeof(ont_cmd_callbacks_t));
	return ONT_ERR_OK;
}

int ont_device_connect(ont_device_t *dev)
{
	int ret = 0;
	if (!dev) {
		return ONT_ERR_BADPARAM;
	}
	if (dev->fd) {
		ont_platform_tcp_close(dev->fd);
		dev->fd = NULL;
	}
	if (ont_platform_tcp_create(&dev->fd)) {
		return ONT_ERR_INTERNAL;
	}
	do {
		ret = ont_platform_tcp_connect(dev->fd, dev->acc_ip, dev->port);
	} while (ret == ONT_ERR_SOCKET_INPROGRESS);
	if (ret == ONT_ERR_SOCKET_ISCONN) {
		dev->status = ONTDEV_STATUS_CONNECTED;
		return ONT_ERR_OK;
	}
	return ret;
}


int ont_device_register(ont_device_t *dev,
                        uint64_t productid,
                        const char *reg_code,
                        const char *id)
{
	t_message_register reg;
	reg.pid = productid;
	reg.regcode_size = (uint8_t)strlen(reg_code);
	reg.regcode = (char *)reg_code;
	reg.deviceid_size = (uint8_t)strlen(id);
	reg.deviceid = (char *)id;
	return ont_device_msg_register(dev, &reg);
}


int ont_device_verify(ont_device_t *dev, const char *token)
{
	char resp;
	size_t resp_length = 1;
	int ret = ont_device_msg_sendmsg(dev, MSG_TYPE_CONNECT_VERIFY, token, 32, 1, &resp, &resp_length, DEVICE_MSG_TIMEOUT);
	if (ret == ONT_ERR_OK && resp == 0) {
		return ONT_ERR_OK;
	}
	return ret;
}


int ont_device_auth(ont_device_t *dev, const char *auth_code, char *auth_code_new)
{
	char resp[258];
	size_t resp_length = 258;
	char result;
	char auth_length;
	char data[64];
	char code_length = (char)strlen(auth_code);
	if (!auth_code || !auth_code_new) {
		return ONT_ERR_BADPARAM;
	}
	ont_encodeInt64(data, dev->device_id);
	*(data + 8) = code_length;
	memcpy(data + 9, auth_code, code_length);

	int ret = ont_device_msg_sendmsg(dev, MSG_TYPE_AUTH, data, 9 + code_length, 1, resp, &resp_length, DEVICE_MSG_TIMEOUT);
	if (ret == 0) {
		result = resp[0];
		if (result == 0) {
			dev->status = ONTDEV_STATUS_AUTHENTICATED;
			return ONT_ERR_OK;
		}
		if (result == ONT_PF_CODE_DEVICE_AUTH_CODE_UPDATE) {
			auth_length = resp[1];
			memcpy(auth_code_new, &resp[2], auth_length);
			memcpy(dev->auth_code, auth_code_new, auth_length);
			dev->status = ONTDEV_STATUS_AUTHENTICATED;
			return ONT_ERR_OK;
		} else {
			return result;
		}
	}

	return ret;
}


void ont_device_disconnect(ont_device_t *dev)
{
	ont_platform_tcp_close(dev->fd);
	dev->fd = 0;
}

void ont_device_destroy(ont_device_t *dev)
{
	if (dev->encrypt_ctx) {
		ont_security_rsa_destroy(dev->encrypt_ctx);
	}

	if (dev->decrypt_ctx) {
		ont_security_rsa_destroy(dev->decrypt_ctx);
	}

	if (dev->rcv_buf) {
		ont_platform_free(dev->rcv_buf);
	}

	if (dev->device_cbs) {
		ont_platform_free(dev->device_cbs);
	}

	if (dev->ont_cmd_cbs) {
		ont_platform_free(dev->ont_cmd_cbs);
	}

	ont_platform_free(dev);
}

int ont_device_keepalive(ont_device_t *dev)
{
	int ret = ont_device_msg_sendmsg(dev, MSG_TYPE_KEEPALIVE, NULL, 0, 0, NULL, NULL, 100);
	return ret;
}

int ont_device_request_rsa_publickey(ont_device_t *dev)
{
	char resp[130];
	size_t resp_length = sizeof(resp);
	uint16_t rsa_len;
	int ret = ont_device_msg_sendmsg(dev, MSG_TYPE_REQ_PUBKEY, NULL, 0, 1, resp, &resp_length, DEVICE_MSG_TIMEOUT);
	if (ret == ONT_ERR_OK) {
		rsa_len = ont_decodeInt16(resp);
		dev->encrypt_flag = 1;
		if (dev->encrypt_ctx) {
			ont_security_rsa_destroy(dev->encrypt_ctx);
		}
		ont_security_rsa_generate(&dev->encrypt_ctx, &resp[2], rsa_len, NULL, 0);
	}
	return ret;
}


int ont_device_replay_rsa_publickey(ont_device_t *dev)
{
	char key[130];

	if (dev->decrypt_ctx) {
		ont_security_rsa_destroy(dev->decrypt_ctx);
	}

	ont_security_rsa_generate(&dev->decrypt_ctx, NULL, 0, NULL, 0);
	ont_encodeInt16(key, 128);
	ont_security_rsa_get_pubkey(dev->decrypt_ctx, key + 2, 128);
	int ret = ont_device_msg_sendmsg(dev, MSG_TYPE_PLATFORM_REQ_PUBKEY_RESPONSE, key, sizeof(key), 0, NULL, NULL, DEVICE_MSG_TIMEOUT);
	return ret;
}

int ont_device_get_channels(ont_device_t *dev, uint32_t *channels, size_t *size)
{
	char resp[200];
	size_t resp_length = 200;
	int32_t cnt, index;

	int ret = ont_device_msg_sendmsg(dev, MSG_TYPE_QUERY_CHANNELLIST, NULL, 0, 1, resp, &resp_length, DEVICE_MSG_TIMEOUT);
	if (ret == ONT_ERR_OK) {
		index = 0;
		cnt = resp[0];
		for (; index < cnt; index++) {
			channels[index] = ont_decodeInt32(resp + 1 + index * 4);
		}
		*size = cnt;
	}
	return ret;
}


int ont_device_del_channel(ont_device_t *dev, uint32_t channel)
{
	char resp;
	char channle_data[4];
	size_t resp_length = 1;
	ont_encodeInt32(channle_data, channel);
	int ret = ont_device_msg_sendmsg(dev, MSG_TYPE_DEL_CHANNEL, channle_data, 4, 1, &resp, &resp_length, DEVICE_MSG_TIMEOUT);
	if (ret == ONT_ERR_OK) {
		if (resp == 0) {
			return ONT_ERR_OK;
		} else {
			RTMP_Log(RTMP_LOGERROR, "del channel ret is %d", resp);
			return ret;
		}
	}

	return ret;
}


int ont_device_add_channel(ont_device_t *dev, uint32_t channel, const char *title, uint32_t tsize, const char *desc, uint32_t dsize)
{
	t_message_addchannel chn;
	chn.chnid = channel;
	chn.title = (char *)title;
	chn.title_length = tsize;
	if (tsize < 0 || tsize > 64) {
		return ONT_ERR_BADPARAM;
	}
	chn.desc = (char *)desc;
	chn.desc_length = dsize;
	int ret = ont_device_msg_addchannel(dev, &chn);

	return ret;
}


int ont_device_check_receive(ont_device_t *dev, uint32_t tm_ms)
{
	int ret = 0;
	t_message_header msg_header;
	char *msg = NULL;

	ret = ont_device_msg_rcv(dev, &msg, &msg_header, tm_ms);
	if (ret == ONT_ERR_OK) {
		ret = ont_device_msg_callback(dev, msg, msg_header.msg_length, &msg_header);
	}
	if (ret == ONT_ERR_SOCKET_RCV_TIMEOUT) {
		/*suppose is ok*/
		ret = ONT_ERR_OK;
	}
	return ret;
}

int ont_device_reply_ont_cmd(ont_device_t *dev, int32_t result, const char *cmdid,
                             const char *resp, size_t size)
{
	char *resp_content = NULL;
	char  resp2[2];
	int   ret;
	size_t index = 0;
	if (cmdid) {
		resp_content = ont_platform_malloc(40 + size);
		if (!resp_content) {
			return ONT_ERR_NOMEM;
		}
		*(resp_content + index) = (char)result;
		index++;
		*(resp_content + index) = 1;
		index++;
		memcpy(resp_content + index, cmdid, 36);
		index += 36;
		if (size > 0) {
			ont_encodeInt16(resp_content + index, (uint16_t)size);
			index += 2;
			memcpy(resp_content + index, resp, size);
			index += size;
		}

		ret = ont_device_msg_snd_response(dev, MSG_TYPE_PLATFORM_CMD_RESP, resp_content, index, DEVICE_MSG_TIMEOUT);
	} else {
		resp2[0] = (char)result;
		resp2[1] = 0;
		ret = ont_device_msg_snd_response(dev, MSG_TYPE_PLATFORM_CMD_RESP, resp2, 2, DEVICE_MSG_TIMEOUT);
	}

	if (resp_content) {
		ont_platform_free(resp_content);
	}

	return ret;
}


int ont_device_reply_user_defined_cmd(ont_device_t *dev, int32_t result, const char *cmdid,
                                      const char *resp, size_t size)
{
	char *resp_content = NULL;
	char  resp2[2];
	int   ret;
	size_t index = 0;
	if (cmdid) {
		resp_content = ont_platform_malloc(40 + size);
		if (!resp_content) {
			return ONT_ERR_NOMEM;
		}
		*(resp_content + index) = (char)result;
		index++;
		*(resp_content + index) = 1;
		index++;
		memcpy(resp_content + index, cmdid, 36);
		index += 36;
		if (size > 0) {
			ont_encodeInt16(resp_content + index, (uint16_t)size);
			index += 2;
			memcpy(resp_content + index, resp, size);
			index += size;
		}

		ret = ont_device_msg_snd_response(dev, MSG_TYPE_USERDEFINED_CMD_RESP, resp_content, index, DEVICE_MSG_TIMEOUT);
	} else {
		resp2[0] = (char)result;
		resp2[1] = 0;
		ret = ont_device_msg_snd_response(dev, MSG_TYPE_USERDEFINED_CMD_RESP, resp2, 2, DEVICE_MSG_TIMEOUT);
	}

	if (resp_content) {
		ont_platform_free(resp_content);
	}

	return ret;
}

int ont_device_deal_api_defined_msg(ont_device_t *dev, char *msg, size_t msg_len)
{
	size_t i = 0;

	printf("\n");
	while (i < msg_len) {
		printf("%c", msg[i++]);
	}
	printf("\n");

	return ONT_ERR_OK;
}

int ont_device_deal_plat_resp_push_stream_msg(ont_device_t *dev, ont_plat_resp_dev_ps_t *resp)
{
	if (NULL == resp || NULL == dev) {
		return ONT_ERR_BADPARAM;
	}

	RTMP_Log(RTMP_LOGDEBUG, "information of pushing stream address: "
	         "\naddress: %s"
	         "\nlength of address: %d"
	         "\nttl of the address: %d minutes"
	         "\nprotocol: %s"
	         "\nchannel id: %d",
	         resp->push_url,
	         resp->url_len,
	         resp->url_ttl_min,
	         (resp->protype == 0) ? "RTMP" : "else-protocol",
	         resp->chan_id);

	(void)dev->device_cbs->live_start(dev, resp->chan_id, resp->protype, resp->url_ttl_min, resp->push_url);

	return ONT_ERR_OK;
}

int ont_device_data_upload(ont_device_t *dev, uint64_t dataid, const char *data, size_t len)
{
	char *upload_data = ont_platform_malloc(10 + len);
	ont_encodeInt64(upload_data, dataid);
	ont_encodeInt16(upload_data + 8, (uint16_t)len);
	memcpy(upload_data + 10, data, len);
	int ret = ont_device_msg_sendmsg(dev, MSG_TYPE_DEVICE_UPLOAD_DATA, upload_data, len + 10, 0, NULL, NULL, DEVICE_MSG_TIMEOUT);
	ont_platform_free(upload_data);
	return ret;
}

int ont_device_get_systime(ont_device_t *dev, struct ont_timeval *tm)
{
	int     ret = 0;
	char    resp[12];
	size_t  resp_len = 12;

	if (dev == NULL ||
	    tm  == NULL ) {
		return ONT_ERR_BADPARAM;
	}

	memset(resp, 0x00, 12);
	ret = ont_device_msg_sendmsg(dev, MSG_TYPE_GET_SYSTIME, NULL, 0, 1, resp, &resp_len, 5000);
	if (ret == ONT_ERR_OK) {
		tm->tv_sec = ont_decodeInt64(resp);
		tm->tv_usec = ont_decodeInt32(resp + 8);
	}
#if 0 /* to wipe off warning of compiler by using final return statement */
	else {
		return ret;
	}
#endif
	return ret;
}

int ont_device_keep_connect(ont_device_t *dev)
{
	int     ret = 0;

	if (NULL == dev) {
		return ONT_ERR_BADPARAM;
	}

	ret = ont_device_msg_sendmsg(dev, MSG_TYPE_KEEP_CONNECT, NULL, 0, 0, NULL, NULL, DEVICE_MSG_TIMEOUT);

	return ret;
}

int ont_device_req_push_stream(ont_device_t *dev, uint32_t chan_id, uint16_t idle_sec)
{
	int     ret = 0;
	int     index = 0;
	uint8_t result = 0;
	char    *data = NULL;
	size_t  data_len = 0;
	char    resp[533] = { 0 }; /* 目前设备为推流地址设定的存储空间为512字节，在平台回复中，除推流地址外共11个字节，预留10字节 */
	size_t  resp_len = 0;
	ont_plat_resp_dev_ps_t resp_s = { 0 };

	/* 推流空闲时间最大为600s */
	if (NULL == dev || 600 < idle_sec) {
		return ONT_ERR_BADPARAM;
	}

	/* 设备请求推流信息占6个字节 */
	data_len = 6;
	data = ont_platform_malloc(data_len);
	if (NULL == data) {
		return ONT_ERR_NOMEM;
	}
	memset(data, 0x00, data_len);

	ont_encodeInt32(data, chan_id);
	index += 4;
	ont_encodeInt16(data + index, idle_sec);

	resp_len = sizeof(resp);
	ret = ont_device_msg_sendmsg(dev, MSG_TYPE_REQ_PUSH_STREAM, data, data_len, 1, resp, &resp_len, DEVICE_MSG_TIMEOUT);
	if (ret == ONT_ERR_OK) {
		result = resp[0];
		if (result) {
			ret = result;
			RTMP_Log(RTMP_LOGERROR, "%s(%d), Failed to request to push stream, error=%d\n", __FUNCTION__, __LINE__, ret);
		} else {
			memset(&resp_s, 0x00, sizeof(ont_plat_resp_dev_ps_t));

			/* result */
			index = 0;
			memcpy(&resp_s.result, &resp[index], 1);

			/* channel id */
			index = 1;
			resp_s.chan_id = ont_decodeInt32(resp + index);

			/* protype of protocol */
			index += 4;
			memcpy(&resp_s.protype, &resp[index], 1);

			/* ttl of link address */
			index += 1;
			resp_s.url_ttl_min = ont_decodeInt16(resp + index);

			/* length of pushing stream address */
			index += 2;
			resp_s.url_len = ont_decodeInt16(resp + index);

			/* push stream address */
			index += 2;
			memcpy(resp_s.push_url, resp + index, resp_s.url_len);

			/* to deal the response of platform */
			(void)dev->device_cbs->plt_resp_dev_req_stream_msg(dev, &resp_s);
		}
	}

	if (data) {
		ont_platform_free(data);
	}

	return ret;
}
