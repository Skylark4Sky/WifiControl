/*
* _COPYRIGHT_
*
* File Name: gisunlink_mqtt_task.c
* System Environment: JOHAN-PC
* Created Time:2020-07-20
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#include <math.h>
#include "esp_log.h"
#include "esp_libc.h"

#include "esp_system.h"
#include "mbedtls/base64.h"

#include "gisunlink_uart.h"
#include "gisunlink_mqtt.h"
#include "gisunlink_utils.h"
#include "gisunlink_atomic.h"
#include "gisunlink_mqtt_task.h"
#include "gisunlink_peripheral.h"
#include "gisunlink_updatefirmware.h"

#define SIGNAL_BIT_SIZE (1)
#define TOTAL_BIT_SIZE (1)
#define COM_ID_BIT_SIZE(total) total  
#define COM_DATA_SIZE (31)
#define PACKET_CONTENT(total) (COM_DATA_SIZE * (total))
#define MIN_PACKET_SIZE_WITH_TOTAL(total) (SIGNAL_BIT_SIZE + TOTAL_BIT_SIZE + COM_ID_BIT_SIZE(total) + PACKET_CONTENT(total)) 
#define CALC_BASE64_LEN(dataLen) ((dataLen % 3) ? (((dataLen/3) + 1) *4) : ((dataLen/3) *4)) 

#define PUBLISH_FORMAT "{\"id\":%lu,\"act\":\"%s\",\"behavior\":%d,\"data\":\"%s\",\"ctime\":%ld}"
#define RESPOND_FORMAT "{\"id\":%lu,\"act\":\"%s\",\"behavior\":%d,\"data\":{\"req_id\":%ld,\"success\":%s,\"msg\":\"%s\"}}"
#define DEVICE_INFO_FORMAT "{\"id\":%lu,\"act\":\"%s\",\"info\":{\"version\":%s,\"device_sn\":%s\",\"wifi_version\":\"%s\"}}"

static void mattTransferTaskRespond(gisunlink_respond_message *message) {
	char *msg = NULL;
	bool send_status = false;
	switch(message->reason) {
		case GISUNLINK_SEM_ERROR:
			msg = "system error";
			break;
		case GISUNLINK_SEND_SUCCEED:
			msg = "send data succeed";
			send_status = true;
			break;
		case GISUNLINK_SEND_TIMEOUT:
			msg = "send data timeout";
			break;
		case GISUNLINK_SEND_BUSY:
			msg = "system busy";
			break;
		default:
			msg = "unknown error";
	}
	mqttMessageRespond("transfer",message->behavior,message->heading,send_status,msg);
}

void packetEncode(gisunlink_system_ctrl *gisunlink_system, const char *act, uint8 behavior, uint8 publish_ack, uint8 *comData, uint16 len) {
	char *publish_topic = NULL;
	char *publish_data = NULL;
	uint8 *base64Str = (uint8 *)" ";
	size_t base64_len = CALC_BASE64_LEN(len) + 3; 
	base64Str = (uint8 *)gisunlink_malloc(base64_len);
	if(base64Str) {
		int ret = 0;
		if((ret = mbedtls_base64_encode(base64Str, base64_len, &base64_len, comData, len)) == 0) { 
			uint32 requestID = getRequestID();
			asprintf(&publish_data,PUBLISH_FORMAT,requestID,act,behavior,base64Str,getNowTimeBySec());
			asprintf(&publish_topic,"%s%s",STATUS_POST,gisunlink_system->deviceHWSn); 
			gisunlink_print(GISUNLINK_PRINT_ERROR,"%s ->data:%s - %d",publish_topic,publish_data,strlen(publish_data));
			if(publish_ack == MQTT_PUBLISH_NEEDACK) {
				gisunlink_mqtt_publish(publish_topic,publish_data,2,requestID,publish_ack);
			} else {
				gisunlink_mqtt_publish(publish_topic,publish_data,0,requestID,publish_ack);
			}
		} else {
			gisunlink_print(GISUNLINK_PRINT_ERROR,"mbedtls_base64_encode failed %d",ret);
		}
		gisunlink_free(base64Str);
		base64Str = NULL;
	}
	if(publish_topic) {
		gisunlink_free(publish_topic);
		publish_topic = NULL;
	}

	if(publish_data) {
		gisunlink_free(publish_data);
		publish_data = NULL;
	}
}

void transferPacketEncode(gisunlink_transfer_packect *transferInfo, gisunlink_system_ctrl *gisunlink_system) {
	uint16 start = 0;
	uint16 offset = 0;
	if(transferInfo) {
		uint8 comID = 0;
		for(uint8 i = 0; i < transferInfo->shardingNum; i++) {
			start  = i * transferInfo->shardingSize;
			offset = ((i == (transferInfo->shardingNum - 1)) ? (transferInfo->dataLen - (i * transferInfo->shardingSize)) :  transferInfo->shardingSize);
			uint16  maxSize = start + offset;
			if(maxSize <= transferInfo->dataLen) {
				uint8 *buffer = NULL;
				uint8 total = (offset / COM_DATA_SIZE); 
				uint16 packetSize = MIN_PACKET_SIZE_WITH_TOTAL(total);
				gisunlink_print(GISUNLINK_PRINT_ERROR,"total %d packetSize %d dataSize %d start %d offset %d",total,packetSize,maxSize,start,offset);

				if((buffer = (uint8 *)gisunlink_malloc(packetSize)) != NULL) {
					buffer[0] = transferInfo->signal;
					buffer[1] = total;
					uint8 k = 1;
					for(k = 1; k <= total; k++){
						buffer[1 + k] = transferInfo->comList[comID++];
					}

					memcpy(buffer + (1 + k), transferInfo->comData + start, offset);

					packetEncode(gisunlink_system,transferInfo->act ,transferInfo->behavior, transferInfo->ack, buffer,offset + 2 + total);

					gisunlink_free(buffer);
					buffer = NULL;
				}
			}
		}
	}
} 

void mqttMessagePublish(gisunlink_system_ctrl *gisunlink_system, char *act,void *message) {
	gisunlink_uart_event *uart = (gisunlink_uart_event *)message;

	if(uart == NULL) {
		return;
	}

	if(uart->cmd == GISUNLINK_TASK_CONTROL && gisunlink_mqtt_isconnected()) {
		uint8 *transferData = uart->data;
		gisunlink_transfer_packect transferInfo;
		//模组和下位沟通指令
		transferInfo.behavior = *transferData++;
		transferInfo.ack = *transferData++;
		transferInfo.act = act;
		//数据段 开始
		transferInfo.signal = *transferData++;								//信号
		transferInfo.total = *transferData++;								//端口数量
		transferInfo.comList = transferData;								//端口ID列表
		transferInfo.comData = transferData + transferInfo.total;			//端口数据
		transferInfo.dataLen = uart->data_len - 4 - transferInfo.total;		//端口数据长度
		transferInfo.shardingSize = (SPLIT_PACKET_SIZE * COM_DATA_SIZE);	//分包大小

		if(transferInfo.dataLen <= transferInfo.shardingSize) {
			transferInfo.shardingNum = 1; 
		} else {
			transferInfo.shardingNum = ceil((transferInfo.dataLen + (transferInfo.shardingSize - 1))/transferInfo.shardingSize);
		}
		transferPacketEncode(&transferInfo,gisunlink_system);
	}
} 

void mqttMessageRespond(const char *act,uint8 behavior,uint32 req_id,bool result,const char *msg) {
	char *resp_buf = NULL;
	uint32 requestID = getRequestID();
	char *result_str = result ? "true" : "false";
	asprintf(&resp_buf,RESPOND_FORMAT,requestID,act,behavior,req_id,result_str,msg);
	gisunlink_mqtt_publish(TRANSFER_RESPOND,resp_buf,0,requestID,MQTT_PUBLISH_NOACK);
	gisunlink_print(GISUNLINK_PRINT_WARN,"%s id:%d msg:%s",act,req_id,msg);
	if(resp_buf) {
		gisunlink_free(resp_buf);
		resp_buf = NULL;
	}
}

static void transferHandle(gisunlink_mqtt_message *message) {
	uint16 data_len = message->data_len + 4 - ((message->data_len%4));
	uint8 *uart_buf = (uint8 *)gisunlink_malloc(data_len + 1); 

	if(uart_buf) {
		int ret = 0;
		size_t dec_len = 0;
		uint8 *dec_buf = uart_buf + 1;
		if((ret = mbedtls_base64_decode(dec_buf, data_len, &dec_len, (const uint8 *)message->data, data_len)) == 0) {
			uart_buf[0] = message->behavior;
			gisunlink_peripheral_message uart_message = {
				.behavior = message->behavior,
				.heading = message->id,
				.cmd = GISUNLINK_TASK_CONTROL,
				.data = uart_buf,
				.data_len = dec_len + 1,
				.respond = UART_RESPOND,
				.respondCb = mattTransferTaskRespond,
			};
			gisunlink_peripheral_send_message(&uart_message);
		} else {
			gisunlink_print(GISUNLINK_PRINT_ERROR,"mbedtls_base64_decode failed %d",ret);
		}
		gisunlink_free(uart_buf); uart_buf = NULL;
	}
}

static void deviceInfoHandle(gisunlink_system_ctrl *gisunlink_system,gisunlink_mqtt_message *message) {
	char *resp_buf = NULL;
	uint32 requestID = getRequestID();
	asprintf(&resp_buf,DEVICE_INFO_FORMAT,requestID,message->act,gisunlink_system->deviceFWVersion,gisunlink_system->deviceHWSn,WIFI_FIRMWARE_VERSION);
	gisunlink_mqtt_publish(GETDEVICEINFO,resp_buf,0,requestID,MQTT_PUBLISH_NOACK);

	if(resp_buf) {
		gisunlink_free(resp_buf);
		resp_buf = NULL;
	}
}

static void respondUNknownAct(gisunlink_mqtt_message *message) {
	char *msg = NULL;
	asprintf(&msg,"unknown act:%s token:false",message->act);
	mqttMessageRespond("transfer",message->behavior,message->id,false,msg);
	if(msg) {
		gisunlink_free(msg);
		msg = NULL;
	}
}

void mqttRecvMessageHandle(gisunlink_system_ctrl *gisunlink_system, gisunlink_mqtt_message *message) {
	if(!gisunlink_system->authorization) {
		respondUNknownAct(message);
		return;
	}
	switch(message->act_type) {
		case TRANSFER_ACT:
			transferHandle(message);
			break;
		case UPDATE_VER_ACT:
			gisunlink_updatefirmware_download_new_firmware(message->data,message->data_len);
			break;
		case DEVICE_INFO_ACT:
			deviceInfoHandle(gisunlink_system,message);
			break;
		default:
			respondUNknownAct(message);
			break;
	} 
}

