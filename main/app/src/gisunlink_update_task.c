/*
* _COPYRIGHT_
*
* File Name: gisunlink_update_task.c
* System Environment: JOHAN-PC
* Created Time:2020-07-20
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#include "freertos/FreeRTOS.h"

#include <stdio.h>
#include "esp_libc.h"
#include "esp_system.h"
#include "esp_log.h"

#include "gisunlink_mqtt.h"
#include "gisunlink_utils.h"
#include "gisunlink_atomic.h"
#include "gisunlink_peripheral.h"
#include "gisunlink_update_task.h"
#include "gisunlink_updatefirmware.h"

#define FIRMWARE_UPDATE_FORMAT "{\"id\":%lu,\"act\":\"%s\",\"data\":{\"success\":%s,\"msg\":\"%s\"}}"

uint8 firmwareQuery(gisunlink_firmware_update *firmware) {
	uint8 ret = GISUNLINK_TRANSFER_FAILED;
	if(firmware == NULL) {
		return ret;
	}

	uint16 data_len = sizeof(firmware->download->ver) + sizeof(firmware->download->size) + firmware->download->md5_len;
	uint8 *uart_data = (uint8 *)gisunlink_malloc(data_len);
	if(uart_data) {
		uint8 *data_offset = uart_data;
		*(data_offset++) = (firmware->download->ver & 0xFF);
		*(data_offset++) = ((firmware->download->ver >> 8) & 0xFF);
		*(data_offset++) = ((firmware->download->ver >> 16) & 0xFF);
		*(data_offset++) = ((firmware->download->ver >> 24) & 0xFF);

		*(data_offset++) = (firmware->download->size & 0xFF);
		*(data_offset++) = ((firmware->download->size >> 8) & 0xFF);
		*(data_offset++) = ((firmware->download->size >> 16) & 0xFF);
		*(data_offset++) = ((firmware->download->size >> 24) & 0xFF);
		memcpy(data_offset,firmware->download->md5,firmware->download->md5_len);
		gisunlink_peripheral_message uart_message = {
			.cmd = GISUNLINK_DEV_FW_INFO,
			.respond = UART_RESPOND,
			.data = uart_data,
			.data_len = data_len,
			.respondCb = NULL,
		};

		uint8 retry = FIRMWARE_SEND_RETRY;
		gisunlink_respond_message *respond = NULL;

		while(retry--) {
			gisunlink_print(GISUNLINK_PRINT_ERROR,"send firmware_ver:%d md5:%s size:%d",firmware->download->ver,firmware->download->md5,firmware->download->size);
			if((respond = gisunlink_peripheral_send_message(&uart_message))) {
				if(respond->reason == GISUNLINK_SEND_SUCCEED) {
					if(respond->data_len) {
						uint8 status = respond->data[0];
						if(status == GISUNLINK_DEVICE_BUSY) {
							gisunlink_print(GISUNLINK_PRINT_WARN,"status 0x%x device busy!",status);
							ret = GISUNLINK_DEVICE_BUSY;
						} else if(status == GISUNLINK_NEED_UPGRADE) {
							gisunlink_print(GISUNLINK_PRINT_WARN,"status 0x%x device need upgrade!",status);
							ret = GISUNLINK_NEED_UPGRADE;
						} else if(status == GISUNLINK_NO_NEED_UPGRADE) {
							gisunlink_print(GISUNLINK_PRINT_WARN,"status 0x%x device no need upgrade!",status);
							ret = GISUNLINK_NO_NEED_UPGRADE;
						}
					} else {
						gisunlink_print(GISUNLINK_PRINT_ERROR,"unknown error!");
					}
					break;
				} else {
					if(!retry) {
						gisunlink_print(GISUNLINK_PRINT_ERROR,"send firmware_query packet_id:%d timeout",respond->id);
					}
					gisunlink_task_delay(50 / portTICK_PERIOD_MS);
				}
				freeUartRespondMessage(respond);
			}
		}
		gisunlink_free(uart_data);
		uart_data = NULL;
	}

	return ret;
} 

bool firmwareTransfer(uint16 offset,uint8 *data,uint16 len) {
	bool send_succeed = false;

	if(data == NULL || len == 0) {
		return send_succeed;
	}

	uint16 data_len = sizeof(uint16) + sizeof(uint16) + len;
	uint8 *uart_data = (uint8 *)gisunlink_malloc(data_len);

	if(uart_data) {
		uint8 *data_offset = uart_data;
		*(data_offset++) = (offset & 0xFF);
		*(data_offset++) = ((offset >> 8) & 0xFF);

		*(data_offset++) = (len & 0xFF);
		*(data_offset++) = ((len >> 8) & 0xFF);

		memcpy(data_offset,data,len);

		gisunlink_peripheral_message uart_message = {
			.cmd = GISUNLINK_DEV_FW_TRANS,
			.respond = UART_RESPOND,
			.data = uart_data,
			.data_len = data_len,
			.respondCb = NULL,
		};

		uint8 retry = FIRMWARE_SEND_RETRY;
		gisunlink_respond_message *respond = NULL;
		while(retry--) {
			gisunlink_print(GISUNLINK_PRINT_ERROR,"send firmware_transfer offset:%d len:%d",offset - 1,len);
			if((respond = gisunlink_peripheral_send_message(&uart_message))) {
				if(respond->reason == GISUNLINK_SEND_SUCCEED) {
					if(respond->data_len) {
						uint16_t respond_offset = respond->data[0] + (respond->data[1] << 8);
						if(respond_offset == offset) {
							send_succeed = true;
						}
					}
					freeUartRespondMessage(respond);
					break;
				} else {
					if(!retry) {
						gisunlink_print(GISUNLINK_PRINT_ERROR,"send firmware_transfer packet_id:%d timeout, offset:%d len:%d",respond->id,offset,len);
					}
					gisunlink_task_delay(50 / portTICK_PERIOD_MS);
				}
				freeUartRespondMessage(respond);
			}
		}
		gisunlink_free(uart_data);
		uart_data = NULL;
	}
	return send_succeed;
} 

uint8 firmwareChk(void) {
	uint8 ret = GISUNLINK_TRANSFER_FAILED;
	gisunlink_peripheral_message uart_message = {
		.cmd = GISUNLINK_DEV_FW_READY,
		.respond = UART_RESPOND,
		.data = NULL,
		.data_len = 0,
		.respondCb = NULL,
	};

	uint8 retry = FIRMWARE_SEND_RETRY;
	gisunlink_respond_message *respond = NULL;
	while(retry--) {
		gisunlink_print(GISUNLINK_PRINT_WARN,"waiting the device check the firmware");
		if((respond = gisunlink_peripheral_send_message(&uart_message))) {
			if(respond->reason == GISUNLINK_SEND_SUCCEED) {
				ret = GISUNLINK_FIRMWARE_CHK_NO_OK;
				if(respond->data_len) {
					if(respond->data[0] == GISUNLINK_FIRMWARE_CHK_OK) {
						ret = GISUNLINK_FIRMWARE_CHK_OK;
					}
				}
				freeUartRespondMessage(respond);
				break;
			} else {
				if(!retry) {
					gisunlink_print(GISUNLINK_PRINT_ERROR,"send firmware_chk packet_id:%d timeout",respond->id);
				}
				gisunlink_task_delay(100 / portTICK_PERIOD_MS);
			}
			freeUartRespondMessage(respond);
		}
	}
	return ret;
}

void firmwareState(bool state,const char *msg) {
	char *stateBuf = NULL;
	uint32 requestID = getRequestID();
	char *result_str = state ? "true" : "false";
	asprintf(&stateBuf,FIRMWARE_UPDATE_FORMAT,requestID,"status",result_str,msg);
	gisunlink_mqtt_publish(FIRMWARE_UPDATE_STATE,stateBuf,0,requestID,MQTT_PUBLISH_NOACK);

	if(stateBuf) {
		gisunlink_free(stateBuf);
		stateBuf = NULL;
	}
}

