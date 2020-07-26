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

void mqttMessagePublish(gisunlink_system_ctrl *gisunlink_system, const char *act,void *message) {
	gisunlink_uart_event *uart = (gisunlink_uart_event *)message;

	if(uart == NULL) {
		return;
	}

	if(uart->cmd == GISUNLINK_TASK_CONTROL && gisunlink_mqtt_isconnected()) {
		char *publish_topic = NULL;
		char *publish_data = NULL;
		uint8 *base64Str = (uint8 *)" ";
		uint8 behavior = (uint8)(*uart->data);  //第一个字节为行为
		uint8 publish_ack = (uint8)(*(uart->data + 1));//第二个字节为是否需要等待服务器确认
		uint32 requestID = getRequestID();
		if(uart->data_len > 2) {
			uint8 *data = uart->data + 2; //数据往后偏移2位
			uint16 data_len = uart->data_len - 2; //数据长度也减2
			uint16 enc_len = data_len/3;
			size_t base64_len = (data_len%3) ? ((enc_len + 1)*4) : (enc_len*4);
			//这里是不支持浮点运算的，所以加3个字节长度，做补偿.
			base64_len += 3;
			base64Str = (uint8 *)gisunlink_malloc(base64_len);
			if(base64Str) {
				int ret = 0;
				if((ret = mbedtls_base64_encode(base64Str, base64_len, &base64_len, data, data_len)) == 0) { 
					asprintf(&publish_data,PUBLISH_FORMAT,requestID,act,behavior,base64Str,getNowTimeBySec());
					asprintf(&publish_topic,"%s%s",STATUS_POST,gisunlink_system->deviceHWSn); 
					gisunlink_print(GISUNLINK_PRINT_ERROR,"%s ->data:%s",publish_topic,publish_data);
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
	asprintf(&msg,"unknown act:%s",message->act);
	mqttMessageRespond("transfer",message->behavior,message->id,false,msg);
	if(msg) {
		gisunlink_free(msg);
		msg = NULL;
	}
}

void mqttRecvMessageHandle(gisunlink_system_ctrl *gisunlink_system, gisunlink_mqtt_message *message) {
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

