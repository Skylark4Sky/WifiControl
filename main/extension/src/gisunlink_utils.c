/*
* _COPYRIGHT_
*
* File Name: gisunlink_utils.c
* System Environment: JOHAN-PC
* Created Time:2020-07-21
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#include <stdio.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "esp_libc.h"
#include "esp_system.h"
#include "esp_log.h"

#include "gisunlink_utils.h"
#include "gisunlink_atomic.h"
#include "gisunlink_netmanager.h"
#include "gisunlink_peripheral.h"

static struct timeval systime; 

uint32 getNowTimeBySec(void) {
	memset(&systime,0x0,sizeof(struct timeval));
	gettimeofday(&systime,NULL);
	return systime.tv_sec;
}

uint32 getNowTimeByUSec(void) {
	memset(&systime,0x0,sizeof(struct timeval));
	gettimeofday(&systime,NULL);
	return systime.tv_usec;// 1000;
}

uint32 getRequestID(void) {
	return ((getNowTimeBySec()%100000)*10000) + getNowTimeByUSec();
}

char getApRssi(void) {
	static int rssi = 0xFF;
	static uint32 send_wait = 0;
	int ap_rssi = gisunlink_netmanager_get_ap_rssi();
	if((send_wait%5) == 0) {
		gisunlink_peripheral_message uart_message = {
			.cmd = GISUNLINK_NETWORK_RSSI,
			.data = (uint8 *)&ap_rssi,
			.data_len = 1,
			.respond = UART_NO_RESPOND,
			.respondCb = NULL,
		};
		if(rssi != ap_rssi) {
			rssi = ap_rssi;
			gisunlink_peripheral_send_message(&uart_message);
		}
	} 
	send_wait++;
	return ap_rssi;
}

uint32 getHeapSize(void) {
	return esp_get_free_heap_size();
}

void freeUartRespondMessage(gisunlink_respond_message *respond) {
	if(respond) {
		if(respond->data) {
			gisunlink_free(respond->data);
			respond->data = NULL;
		}
		gisunlink_free(respond);
		respond = NULL;
	}
}

bool getDeviceHWSnOrFirmwareVersion(uint8 cmd,char *buffter) {
	bool getHWInfo = false;

	if(buffter == NULL) {
		return getHWInfo;
	}

	gisunlink_peripheral_message uart_message = {
		.cmd = cmd,
		.respond = UART_RESPOND,
		.data = NULL,
		.data_len = 0,
		.respondCb = NULL,
	};

	gisunlink_respond_message *respond = NULL;
	while(1) {
		gisunlink_print(GISUNLINK_PRINT_WARN,"waiting the device sn");
		if((respond = gisunlink_peripheral_send_message(&uart_message))) {
			if(respond->reason == GISUNLINK_SEND_SUCCEED) {
				if(respond->data_len == STM32_UNIQUE_ID_SIZE) {
					snprintf(buffter,DEVICEINFOSIZE,"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
							respond->data[0],respond->data[1],respond->data[2],respond->data[3],respond->data[4],respond->data[5],
							respond->data[6],respond->data[7],respond->data[8],respond->data[9],respond->data[10],respond->data[11]);
					gisunlink_print(GISUNLINK_PRINT_ERROR,"device sn:%s",buffter);
					getHWInfo = true;
				}
				freeUartRespondMessage(respond);
				break;
			} else {
				gisunlink_print(GISUNLINK_PRINT_ERROR,"send wait_hw_sn packet_id:%d timeout",respond->id);
				gisunlink_task_delay(100 / portTICK_PERIOD_MS);
			}
			freeUartRespondMessage(respond);
		}
	}
	return getHWInfo;
}

