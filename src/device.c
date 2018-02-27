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
    if (ret != 0)
    {
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
    if (ret)
    {
        ont_platform_udp_close(sock);
        return ONT_ERR_SOCKET_OP_FAIL;
    }

    memset(buf, 0, BUFSIZE);

    ret = ont_platform_udp_recv(sock, buf, BUFSIZE, &bytes_read);
    if (ret)
    {
        ont_platform_udp_close(sock);
        return ONT_ERR_SOCKET_OP_FAIL;
    }

    for (i = 0; i < 32; i++)
    {
        token[i] = buf[i+4];
    }
    dev->port = ont_decodeInt16(buf+36);
	i = 0;
	while(1)
    {
        dev->acc_ip[i] = buf[i+ 38];
		if (!buf[i + 38])
		{
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
	if (*dev || !cbs)
	{
		return ONT_ERR_BADPARAM;
	}
	_dev = ont_platform_malloc(sizeof(ont_device_t));
	if (!_dev)
	{
		return ONT_ERR_NOMEM;
	}
	memset(_dev, 0x00, sizeof(ont_device_t));

	_dev->seq_snd = ont_platform_time();
	_dev->rcv_buf = ont_platform_malloc(ONT_PROTOCOL_DEFAULT_SIZE);
	if (!_dev->rcv_buf)
	{
		ont_platform_free(_dev);
		return ONT_ERR_NOMEM;
	}
	_dev->buf_length = ONT_PROTOCOL_DEFAULT_SIZE;
	_dev->rcv_length = 0;
	_dev->device_cbs = ont_platform_malloc(sizeof(ont_device_callback_t));
	memcpy(_dev->device_cbs, cbs, sizeof(ont_device_callback_t));
	*dev = _dev;
	return ONT_ERR_OK;
}


int ont_device_set_platformcmd_handle(ont_device_t *dev, ont_cmd_callbacks_t *cb)
{
	if (!dev || !cb)
	{
		return ONT_ERR_BADPARAM;
	}
	dev->ont_cmd_cbs = ont_platform_malloc(sizeof(ont_cmd_callbacks_t));
	memcpy(dev->ont_cmd_cbs, cb, sizeof(ont_cmd_callbacks_t));
	return ONT_ERR_OK;
}

int ont_device_connect(ont_device_t *dev)
{
	int ret = 0;
	if (!dev)
	{
		return ONT_ERR_BADPARAM;
	}
	if (ont_platform_tcp_create(&dev->fd))
	{
		return ONT_ERR_INTERNAL;
	}
	do
	{
		ret = ont_platform_tcp_connect(dev->fd, dev->acc_ip, dev->port);
	} while (ret == ONT_ERR_SOCKET_INPROGRESS);
	if (ret == ONT_ERR_SOCKET_ISCONN)
	{
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
	reg.regcode = (char*)reg_code;
	reg.deviceid_size = (uint8_t)strlen(id);
	reg.deviceid = (char*)id;
	return ont_device_msg_register(dev, &reg);
}


int ont_device_verify(ont_device_t *dev, const char *token)
{
	char resp;
	size_t resp_length = 1;
	int ret = ont_device_msg_sendmsg(dev, MSG_TYPE_CONNECT_VERIFY, token, 32, 1, &resp, &resp_length, DEVICE_MSG_TIMEOUT);
	if (ret == ONT_ERR_OK && resp == 0)
	{
		return ONT_ERR_OK;
	}
	return ret;
}


int ont_device_auth(ont_device_t *dev, const char *auth_code, char *auth_code_new)
{
	char resp[258];
	size_t resp_length=258;
	char result;
	char auth_length;
	char data[64];
	char code_length = (char)strlen(auth_code);
	if (!auth_code || !auth_code_new)
	{
		return ONT_ERR_BADPARAM;
	}
	ont_encodeInt64(data, dev->device_id);
	*(data + 8) = code_length;
	memcpy(data+9, auth_code, code_length);

	int ret = ont_device_msg_sendmsg(dev, MSG_TYPE_AUTH, data, 9+ code_length, 1, resp, &resp_length, DEVICE_MSG_TIMEOUT);
	if (ret == 0)
	{
		result = resp[0];
		if (result == 0)
		{
			dev->status = ONTDEV_STATUS_AUTHENTICATED;
			return ONT_ERR_OK;
		}
		if (result == ONT_PF_CODE_DEVICE_AUTH_CODE_UPDATE)
		{
			auth_length = resp[1];
			memcpy(auth_code_new, &resp[2], auth_length);
			memcpy(dev->auth_code, auth_code_new, auth_length);
			dev->status = ONTDEV_STATUS_AUTHENTICATED;
			return ONT_ERR_OK;
		}
		else
		{
			return ret;
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
	if (dev->rcv_buf)
	{
		ont_platform_free(dev->rcv_buf);
	}

	ont_platform_free(dev);
}

int ont_device_keepalive(ont_device_t *dev)
{
	int ret = ont_device_msg_sendmsg(dev, MSG_TYPE_KEEPALIVE, NULL, 0, 0, NULL,NULL, 100);
	return ret;
}

int ont_device_request_rsa_publickey(ont_device_t *dev)
{
	char resp[130];
	size_t resp_length=sizeof(resp);
	uint16_t rsa_len;
	int ret = ont_device_msg_sendmsg(dev, MSG_TYPE_REQ_PUBKEY, NULL, 0, 1, resp, &resp_length, DEVICE_MSG_TIMEOUT);
	if (ret == ONT_ERR_OK)
	{
		rsa_len = ont_decodeInt16(resp);
		dev->encrypt_flag = 1;
		ont_security_rsa_generate(&dev->encrypt_ctx, &resp[2], rsa_len, NULL, 0);
	}
	return ret;
}


int ont_device_replay_rsa_publickey(ont_device_t *dev)
{
	char key[130];
	ont_security_rsa_generate(&dev->decrypt_ctx, NULL, 0, NULL, 0);
	ont_encodeInt16(key, 128);
	ont_security_rsa_get_pubkey(dev->decrypt_ctx, key + 2, 128);
	int ret = ont_device_msg_sendmsg(dev, MSG_TYPE_PLATFORM_REQ_PUBKEY_RESPONSE, key, sizeof(key), 0, NULL, NULL, DEVICE_MSG_TIMEOUT);
	return ret;
}

int ont_device_get_channels(ont_device_t *dev, uint32_t *channels, size_t *size)
{
	char resp[200];
	size_t resp_length=200;
	int32_t cnt, index;

	int ret = ont_device_msg_sendmsg(dev, MSG_TYPE_QUERY_CHANNELLIST, NULL, 0, 1, resp, &resp_length, DEVICE_MSG_TIMEOUT);
	if (ret == ONT_ERR_OK)
	{
		index = 0;
		cnt = resp[0];
		for (; index < cnt; index++)
		{
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
	size_t resp_length=1;
	ont_encodeInt32(channle_data, channel);
	int ret = ont_device_msg_sendmsg(dev, MSG_TYPE_DEL_CHANNEL, channle_data, 4, 1, &resp, &resp_length, DEVICE_MSG_TIMEOUT);
	if (ret == ONT_ERR_OK)
	{
		if (resp == 0)
		{
			return ONT_ERR_OK;
		}
		else
		{
			ONT_LOG1(ONTLL_ERROR, "del channel ret is %d", resp);
			return ret;
		}
	}

	return ret;
}


int ont_device_add_channel(ont_device_t *dev, uint32_t channel, const char*title, uint32_t tsize, const char *desc, uint32_t dsize)
{
	t_message_addchannel chn;
	chn.chnid = channel;
	chn.title = (char*)title;
	chn.title_length = tsize;
	if (tsize < 0 || tsize>64)
	{
		return ONT_ERR_BADPARAM;
	}
	chn.desc = (char*)desc;
	chn.desc_length = dsize;
	int ret = ont_device_msg_addchannel(dev, &chn);

	return ret;
}


int ont_device_check_receive(ont_device_t *dev, uint32_t tm_ms)
{
	int ret = 0;
	t_message_header msg_header;
	char *msg=NULL;

	ret = ont_device_msg_rcv(dev, &msg, &msg_header, tm_ms);
	if (ret == ONT_ERR_OK)
	{
		ret = ont_device_msg_callback(dev, msg, msg_header.msg_length, &msg_header);
	}
	if (ret == ONT_ERR_SOCKET_RCV_TIMEOUT)
	{
		/*suppose is ok*/
		ret= ONT_ERR_OK;
	}
	return ret;
}

int ont_device_reply_ont_cmd(ont_device_t *dev, int32_t result, const char *cmdid,
	const char *resp, size_t size)
{
	char *resp_content=NULL;
	char  resp2[2];
	int   ret;
	size_t index = 0;
	if (cmdid)
	{
		resp_content = ont_platform_malloc(40 + size);
		if (!resp_content)
		{
			return ONT_ERR_NOMEM;
		}
		*(resp_content+ index)=(char)result;
		index++;
		*(resp_content + index) = 1;
		index++;
		memcpy(resp_content+index, cmdid, 36);
		index += 36;
		if (size > 0)
		{
			ont_encodeInt16(resp_content + index, (uint16_t)size);
			index += 2;
			memcpy(resp_content + index, resp, size);
			index += size;
		}

		ret = ont_device_msg_snd_response(dev, MSG_TYPE_PLATFORM_CMD_RESP, resp_content, index, DEVICE_MSG_TIMEOUT);
	}
	else
	{
		resp2[0] = (char)result;
		resp2[1] = 0;
		ret = ont_device_msg_snd_response(dev, MSG_TYPE_PLATFORM_CMD_RESP, resp2, 2, DEVICE_MSG_TIMEOUT);
	}

	if (resp_content)
	{
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
	if (cmdid)
	{
		resp_content = ont_platform_malloc(40 + size);
		if (!resp_content)
		{
			return ONT_ERR_NOMEM;
		}
		*(resp_content + index) = (char)result;
		index++;
		*(resp_content + index) = 1;
		index++;
		memcpy(resp_content + index, cmdid, 36);
		index += 36;
		if (size > 0)
		{
			ont_encodeInt16(resp_content + index, (uint16_t)size);
			index += 2;
			memcpy(resp_content + index, resp, size);
			index += size;
		}

		ret = ont_device_msg_snd_response(dev, MSG_TYPE_USERDEFINED_CMD_RESP, resp_content, index, DEVICE_MSG_TIMEOUT);
	}
	else
	{
		resp2[0] = (char)result;
		resp2[1] = 0;
		ret = ont_device_msg_snd_response(dev, MSG_TYPE_USERDEFINED_CMD_RESP, resp2, 2, DEVICE_MSG_TIMEOUT);
	}

	if (resp_content)
	{
		ont_platform_free(resp_content);
	}

	return ret;
}


int ont_device_data_upload(ont_device_t *dev, uint64_t dataid, const char*data, size_t len)
{
	char *upload_data = ont_platform_malloc(10+len);
	ont_encodeInt64(upload_data, dataid);
	ont_encodeInt16(upload_data+8, (uint16_t)len);
	memcpy(upload_data + 10, data, len);
	int ret = ont_device_msg_sendmsg(dev, MSG_TYPE_DEVICE_UPLOAD_DATA, upload_data, len+10, 0, NULL,NULL, DEVICE_MSG_TIMEOUT);
	ont_platform_free(upload_data);
	return ret;
}
