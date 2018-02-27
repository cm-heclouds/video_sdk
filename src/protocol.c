#include <stdio.h>
#include <string.h>

#include "log.h"
#include "error.h"
#include "device.h"
#include "protocol.h"
#include "security.h"
#include "platform.h"
#include "ont_bytes.h"
#include "platform_cmd.h"

#define CHECK_ENCRYPTFLAG(flag)    ((flag)&0xf0)


/*timeout is 1 second*/
int ont_device_msg_snd_response(ont_device_t *dev, uint16_t msgtype, char *data, size_t length, uint32_t ms)
{
	int ret = ont_device_msg_sendmsg(dev, msgtype, data, length, 0, NULL, NULL, ms);
	return ret;
}

static int ont_device_msg_live_callback(ont_device_t *dev, char *msg, size_t length)
{
	uint32_t channel;
	uint8_t protype;
	uint16_t min;
	int32_t index = 0;
	uint16_t pushlen = 0;
	char    push_url[512];
	char    *buffer = msg;
	channel = ont_decodeInt32(buffer + index);
	index += 4;
	
	protype = buffer[index++];
	min = ont_decodeInt16(buffer + index);
	index += 2;
	pushlen = ont_decodeInt16(buffer+index);
	index += 2;

	memcpy(push_url, buffer + index, pushlen);
	push_url[pushlen] = '\0';
	return dev->device_cbs->live_start(dev, channel, protype, min, push_url);
}


static int ont_device_msg_rvod_callback(ont_device_t *dev, char *msg, size_t length)
{
	uint32_t channel;
	uint8_t protype;
	int32_t index = 0;
	uint16_t push_len = 0;
	uint16_t flag_len = 0;
	uint16_t min;
	ont_video_file_t file;
	char    push_url[512];
	char    vod_flag[33];
	char    *buffer = msg;
	channel = ont_decodeInt32(buffer + index);
	index += 4;
	protype = buffer[index++];

	min = ont_decodeInt16(buffer + index);
	index += 2;
	push_len = ont_decodeInt16(buffer + index);
	index += 2;

	memcpy(push_url, buffer + index, push_len);
	push_url[push_len] = '\0';
	index += push_len;
	memcpy(file.begin_time, buffer + index, 20);
	index += 20;
	memcpy(file.end_time, buffer + index, 20);
	index += 20;
	flag_len = buffer[index];
	index++;
	memcpy(vod_flag, buffer + index, flag_len);
	vod_flag[flag_len] = '\0';
	file.channel = channel;
	return dev->device_cbs->rvod_start(dev, channel, protype, &file, vod_flag, push_url, min);
}



static int ont_device_msg_userdefined_cmd_callback(ont_device_t *dev, char *msg, size_t length)
{
	ont_device_cmd_t  cmd;
	int32_t index = 0;
	char    *buffer = msg;
	cmd.need_resp = buffer[index++] & 0xff;
	if (cmd.need_resp)
	{
		memcpy(cmd.id, buffer + index, 36);
		index += 36;
	}
	cmd.size = ont_decodeInt16(buffer + index);
	index += 2;
	cmd.req = buffer + index;

	return dev->device_cbs->user_defined_cmd(dev, &cmd);
}

static int ont_device_msg_platform_cmd_callback(ont_device_t *dev, char *msg, size_t length)
{
	ont_device_cmd_t  cmd;
	int32_t index = 0;
	char    *buffer = msg;
	char    *cmd_req = NULL;
	cmd.need_resp = buffer[index++] & 0xff;
	if (cmd.need_resp)
	{
		memcpy(cmd.id, buffer + index, 36);
		index += 36;
	}
	cmd.size = ont_decodeInt16(buffer + index);
	index += 2;
	cmd_req = ont_platform_malloc(cmd.size+1);
	memcpy(cmd_req, buffer + index, cmd.size);
	cmd_req[cmd.size] = '\0';
	cmd.req = cmd_req;
	int ret = ont_videocmd_handle(dev, &cmd);
	ont_platform_free(cmd_req);
	return ret;
}



int ont_device_msg_callback(ont_device_t *dev, char *msg, size_t length, t_message_header *header)
{
	ont_device_callback_t   *cbs = dev->device_cbs;
	int ret = 0;
	int r_byte;
	switch (header->msg_type)
	{
	    case MSG_TYPE_PLATFORM_REQ_PUBKEY:
		{
			ont_device_replay_rsa_publickey(dev);
			break;
		}
		case MSG_TYPE_KEEPALIVE_RESP:
		{
			cbs->keepalive_resp(dev);
			break;
		}
		case MSG_TYPE_LIVE_START:
		{
			ret = ont_device_msg_live_callback(dev, msg, length);
			break;
		}
		case MSG_TYPE_RVOD_START:
		{
			ret = ont_device_msg_rvod_callback(dev, msg, length);
			break;
		}
		case MSG_TYPE_PLATFORM_CMD:
		{
			ret = ont_device_msg_platform_cmd_callback(dev, msg, length);
			break;
		}
		case MSG_TYPE_USERDEFINED_CMD:
		{
			ret = ont_device_msg_userdefined_cmd_callback(dev, msg, length);
			break;
		}
#if 0
        case MSG_TYPE_DEVICE_REGISTER_RESP:
        {

            char *pResp=msg;
            pResp++;
			int auth_length = *pResp;
			pResp++;
			memcpy(dev->auth_code, pResp, auth_length);
			pResp += auth_length;
			dev->device_id=ont_decodeInt64(pResp);
            printf("reg result is %d, id %lld\n", msg[0], dev->device_id);
            break;
        }
        case MSG_TYPE_AUTH_RESP:
        {
            //printf("auth result is %d\n", msg[0]);
            dev->status = ONTDEV_STATUS_AUTHENTICATED;
            break;
        }
#endif


		default:
		{
			/*not recognize the message*/
			break;
		}
	}
	if (header->msg_type==MSG_TYPE_LIVE_START||
		header->msg_type == MSG_TYPE_RVOD_START)
	{
		r_byte = (char)ret;
		ont_device_msg_sendmsg(dev, header->msg_type | 0x8000, (const char*)&r_byte, 1, 0, NULL, NULL,  DEVICE_MSG_TIMEOUT);
	}
	return ONT_ERR_OK;
}

int ont_device_msg_rcv(ont_device_t *dev, char **msg_out, t_message_header *header, uint32_t ms)
{
	int index, dec_index;
	int ret = 0;
	uint32_t bytes_read, begin_ts, end_ts;
    uint32_t rcv_offset;
	char      *dec_buffer=NULL;
	char      *temp;
	size_t    decrypt_size;
	char      *buffer = dev->rcv_buf;
	begin_ts = ont_platform_time();
	rcv_offset = dev->rcv_length;
    while(rcv_offset < ONT_PROTOCOL_COMM_PLAIN_HEADER)
    {	
	    ret = ont_platform_tcp_recv(dev->fd, buffer + rcv_offset, ONT_PROTOCOL_COMM_PLAIN_HEADER - rcv_offset, &bytes_read);
		if (ret < 0)
		{
			goto __end;
		}
		rcv_offset += bytes_read;
		end_ts = ont_platform_time();
		if (rcv_offset < ONT_PROTOCOL_COMM_PLAIN_HEADER)
		{
			if (ms==0 || end_ts * 1000 - begin_ts * 1000 > ms)
			{
				ret = ONT_ERR_SOCKET_RCV_TIMEOUT;
				goto __end;
			}
		}
	};
	index = 0;
	header->flag = buffer[index++];
	header->msg_length = ont_decodeInt24(buffer+ index);
	index += 3;

	/*check if need realloc the memory*/
	if (header->msg_length > dev->buf_length)
	{
		temp= ont_platform_malloc(header->msg_length);
		dev->buf_length = header->msg_length;
		memcpy(temp, dev->rcv_buf, ONT_PROTOCOL_COMM_PLAIN_HEADER);
		ont_platform_free(dev->rcv_buf);
		dev->rcv_buf = temp;
		buffer = dev->rcv_buf;
	}

	if (header->msg_length > ONT_PROTOCOL_COMM_PLAIN_HEADER)
	{
		do
		{
			ret = ont_platform_tcp_recv(dev->fd, buffer + rcv_offset, header->msg_length - rcv_offset, &bytes_read);
			if (ret < 0)
			{
				goto __end;
			}
			rcv_offset += bytes_read;
			end_ts = ont_platform_time();
			if (rcv_offset < (uint32_t)header->msg_length)
			{
				if (end_ts * 1000 - begin_ts * 1000 > ms)
				{
					ret = ONT_ERR_SOCKET_RCV_TIMEOUT;
					goto __end;
				}
			}
		} while (rcv_offset < (uint32_t)header->msg_length);
	}

	/*check if need decrypt*/
	if (CHECK_ENCRYPTFLAG(header->flag))
	{
		dec_buffer = ont_platform_malloc(header->msg_length);
		if (!dec_buffer)
		{
			ret = ONT_ERR_NOMEM;
			goto __end;
		}
		decrypt_size = header->msg_length;
		if (ont_security_rsa_decrypt(dev->decrypt_ctx, (unsigned char*)buffer + ONT_PROTOCOL_COMM_PLAIN_HEADER, header->msg_length - ONT_PROTOCOL_COMM_PLAIN_HEADER, (unsigned char*)dec_buffer, &decrypt_size))
		{
            ret = ONT_ERR_DEC_FAIL;
			dev->rcv_length = 0;
			goto __end;
		}
		/*reset the message length*/
		header->msg_length = ONT_PROTOCOL_COMM_PLAIN_HEADER + decrypt_size;
		dec_index = 0;
		header->msg_type = ont_decodeInt16(dec_buffer + dec_index);
		dec_index += 2;
		header->msg_id = ont_decodeInt32(dec_buffer + dec_index);
		/*memory copy*/
		memcpy(buffer+ ONT_PROTOCOL_COMM_PLAIN_HEADER, dec_buffer, decrypt_size);
	}
	else
	{
		header->msg_type = ont_decodeInt16(buffer + index);
		index += 2;
		header->msg_id = ont_decodeInt32(buffer + index);
	}
	/*success handle one message*/
	dev->rcv_length = 0; 
	/*if request */
	if(! (header->msg_type&0x8000))
	{
	    dev->seq_rcv = header->msg_id;
	}
	*msg_out = buffer + ONT_PROTOCOL_COMM_HEADER_LENGTH;
	if (dec_buffer)
	{
		ont_platform_free(dec_buffer);
	}
	return ret;
__end:
	dev->rcv_length = rcv_offset;
	if (dec_buffer)
	{
		ont_platform_free(dec_buffer);
	}
	return ret;
}

int ont_device_msg_sendmsg(ont_device_t *dev, uint16_t type, const char *data, size_t length, uint8_t wait_resp,
	char *resp, size_t *resp_length, uint32_t ms)
{
	int index = 0;
	int ret = 0;
	size_t msgsize, encrypted_size;
	uint32_t bytes_snd, snd_offset, begin_ts, end_ts;
	t_message_header rcv_header;
	char    *msg_encrypt = NULL;
	char    *msg;
	char    *msgrcv=NULL;
	char     flag;

	if (wait_resp && (!resp || !resp_length))
	{
		return ONT_ERR_BADPARAM;
	}

	/*flag*/
	if (dev->encrypt_flag)
	{
		flag = MSG_FLAG_VERSION | 0x10;
		encrypted_size = 128 * ((length + 6 + 116) / 117); //encrpted size
		msgsize = encrypted_size + ONT_PROTOCOL_COMM_PLAIN_HEADER;
	}
	else
	{
		flag = MSG_FLAG_VERSION;
		msgsize = length + ONT_PROTOCOL_COMM_HEADER_LENGTH;
	}

	msg = ont_platform_malloc(msgsize+3);

	msg[index++] = flag;
	ont_encodeInt24(msg + index, msgsize);
	index += 3;

	/*msgtype*/
    ont_encodeInt16(msg+index, type);
	index += 2;

	/*msg sequence id */
	if (type & 0x8000) /*if response message*/
	{
		ont_encodeInt32(msg + index, dev->seq_rcv);
	}
	else
	{
		ont_encodeInt32(msg + index, dev->seq_snd);
	}
	index += 4;

	memcpy(msg + index, data, length);
	/*if need encrpted*/
	if (dev->encrypt_flag)
	{

		msg_encrypt = ont_platform_malloc(encrypted_size);
		ret = ont_security_rsa_encrypt(dev->encrypt_ctx, (unsigned char*)msg+ONT_PROTOCOL_COMM_PLAIN_HEADER, 6+length, (unsigned char*)msg_encrypt, &encrypted_size);
		if (ret)
		{
			goto __end;
		}
		memcpy(msg + ONT_PROTOCOL_COMM_PLAIN_HEADER, msg_encrypt, encrypted_size);
	}
	begin_ts = ont_platform_time();
	snd_offset = 0;
	do
	{
		ret = ont_platform_tcp_send(dev->fd, msg+ snd_offset, msgsize- snd_offset, &bytes_snd);
		if (ret < 0)
		{
			goto __end;
		}
		snd_offset += bytes_snd;
		end_ts = ont_platform_time();
		if (bytes_snd<msgsize&&end_ts * 1000 - begin_ts * 1000 > ms)
		{
			ret = ONT_ERR_SOCKET_SND_TIMEOUT;
			goto __end;
		}
	} while (snd_offset<msgsize);
	
	if (!wait_resp)
	{
		goto __end;
	}

	do
	{
        ret=ont_device_msg_rcv(dev, &msgrcv, &rcv_header, ms);
		if (ret < 0)
		{
			goto __end;
		}
		/*if it's response of the message*/
		if (rcv_header.msg_id == dev->seq_snd)
		{
			if (*resp_length >= (size_t)rcv_header.msg_length - ONT_PROTOCOL_COMM_HEADER_LENGTH)
			{
				memcpy(resp, msgrcv, rcv_header.msg_length - ONT_PROTOCOL_COMM_HEADER_LENGTH);
				*resp_length = (size_t)rcv_header.msg_length - ONT_PROTOCOL_COMM_HEADER_LENGTH;
			}
			else
			{
				ret = ONT_ERR_NOMEM;
				goto __end;
			}
			break;
		}
		else
		{
			/*other message, invoke the callback*/
			dev->seq_rcv = rcv_header.msg_id;
			if (!msgrcv)
			{
				ONT_LOG0(ONTLL_ERROR, "error");
				break;/*error*/
			}
			ont_device_msg_callback(dev, msgrcv, rcv_header.msg_length, &rcv_header);
		}

	} while (1);


__end:
	if (!(type & 0x8000))
	{
		dev->seq_snd++;
	}
	if (msg_encrypt)
	{
		ont_platform_free(msg_encrypt);
	}
	ont_platform_free(msg);
	return ret;
}

int ont_device_msg_register(ont_device_t *dev, t_message_register *reg)
{
	if (!reg)
	{
		return ONT_ERR_BADPARAM;
	}
	/*response max size*/
	int ret = 0;
	uint8_t result;
	uint16_t auth_length;
	char resp[268];
	size_t resp_len = 268;
	char *pResp;
	char *data = ont_platform_malloc(sizeof(t_message_register) + reg->regcode_size + reg->deviceid_size);
	char *pData = data;
	ont_encodeInt64(pData, reg->pid);
	pData += 8;
	*pData++ = reg->regcode_size;
	memcpy(pData, reg->regcode, reg->regcode_size);
	pData += reg->regcode_size;
	
	*pData++ = reg->deviceid_size;
	memcpy(pData, reg->deviceid, reg->deviceid_size);
	pData += reg->deviceid_size;
	ret = ont_device_msg_sendmsg(dev, MSG_TYPE_DEVICE_REGISTER, data, pData - data , 1, resp, &resp_len, DEVICE_MSG_TIMEOUT);
	if (ret == ONT_ERR_OK)
	{
		pResp = resp;
		result = *pResp;
		if (!result)
		{
			/*success*/
			pResp++;
			auth_length = *pResp;
			pResp++;
			memcpy(dev->auth_code, pResp, auth_length);
			pResp += auth_length;
			dev->device_id=ont_decodeInt64(pResp);
		}
		else
		{
			ret = result;
			ONT_LOG1(ONTLL_ERROR, "register ret is %d", result);
		}
	}

	if (data)
	{
		ont_platform_free(data);
	}
	return ret;
}


int ont_device_msg_addchannel(ont_device_t *dev, t_message_addchannel *chn)
{
	if (!chn)
	{
		return ONT_ERR_BADPARAM;
	}
	/*response max size*/
	int ret = 0;
	uint8_t result;
	char resp[1];
	size_t resp_len = 1;
	char *data = ont_platform_malloc(sizeof(t_message_addchannel) + chn->title_length + chn->desc_length);
	char *pData = data;
	ont_encodeInt32(pData, chn->chnid);
	pData += 4;
	*pData++ = chn->title_length;
	memcpy(pData, chn->title, chn->title_length);
	pData += chn->title_length;

	*pData++ = chn->desc_length;
	memcpy(pData, chn->desc, chn->desc_length);
	pData += chn->desc_length;
	ret = ont_device_msg_sendmsg(dev, MSG_TYPE_ADD_CHANNEL, data, pData - data, 1, resp, &resp_len, 2000);
	if (ret == ONT_ERR_OK)
	{
		result = resp[0];
		if (result)
		{
			ONT_LOG1(ONTLL_ERROR, "add channel ret is %d", result);
		}
	}

	if (data)
	{
		ont_platform_free(data);
	}
	return ret;
}
