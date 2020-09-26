/*
* _COPYRIGHT_
*
* File Name: gisunlink_ota.c
* System Environment: JOHAN-PC
* Created Time:2019-03-05
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#include <string.h>

#include "esp_ota_ops.h"
#include "gisunlink_ota.h" 
#include "gisunlink_print.h" 
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "gisunLink_atomic.h"
#include "esp_system.h"

#define BUFFSIZE 1500

typedef enum esp_ota_firm_state {
	ESP_OTA_INIT = 0,
	ESP_OTA_PREPARE,
	ESP_OTA_START,
	ESP_OTA_RECVED,
	ESP_OTA_FINISH,
} esp_ota_firm_state_t;

typedef struct _gisunlink_ota_ctrl {
	uint8 *path;
	uint16_t path_len;
	void *thread_lock;
	uint32 download_process;
} gisunlink_ota_ctrl;

static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
	static uint32 process = 0;
	gisunlink_ota_ctrl *task_ctrl = (gisunlink_ota_ctrl *)evt->user_data;
	switch(evt->event_id) {
		case HTTP_EVENT_ERROR:
		case HTTP_EVENT_ON_CONNECTED:
		case HTTP_EVENT_HEADER_SENT:
		case HTTP_EVENT_ON_HEADER:
		case HTTP_EVENT_ON_FINISH:
		case HTTP_EVENT_DISCONNECTED:
			break;
		case HTTP_EVENT_ON_DATA:
			if(task_ctrl->download_process == 0) {
				process = 1;
			} else {
				//每10k输出一次
				if(task_ctrl->download_process >= (process * 10240)) {
					++process;
					gisunlink_print(GISUNLINK_PRINT_WARN,"ota file size:%d download progress:%d",task_ctrl->size,task_ctrl->download_process);
				}
			}
			task_ctrl->download_process += evt->data_len;
			break;
	}
	return ESP_OK;
}

static void gisunlink_ota_task(void *parameter) {
	uint32 file_size = 0;
	bool update_flags = false;
	gisunlink_ota_ctrl *task_ctrl = (gisunlink_ota_ctrl *)parameter;

	if(task_ctrl) {
		gisunlink_print(GISUNLINK_PRINT_ERROR, "ota file:%s", (const char *)task_ctrl->path);
		
		if(task_ctrl && task_ctrl->path && task_ctrl->path_len) {
			esp_http_client_config_t config = {
				.url = (const char *)task_ctrl->path,
				.event_handler = http_event_handler,
				.user_data = task_ctrl,
			};

			esp_err_t ret = esp_https_ota(&config);//调用这个自动完成更新
			if (ret == ESP_OK) {
				printf("------------------------success-------------------------------------\n");
				esp_restart();
			} else {
				printf("------------------------error:%x-------------------------------------\n",ret);
				gisunlink_print(GISUNLINK_PRINT_ERROR, "Firmware Upgrades Failed");
			}
		}
	}

	gisunlink_destroy_task(NULL);
}

void gisunlink_ota_runing(const char *url,unsigned int size) {

	if(otaCtrl == NULL) {
		return;
	}

	if(url && size) {
		if(otaCtrl->path) {
			gisunlink_free(otaCtrl->path);
			otaCtrl->path = NULL;
		}

		otaCtrl->path_len = strlen(url);
		otaCtrl->path = (uint8 *)gisunlink_malloc(otaCtrl->path_len);
		otaCtrl->size = size;
		memcpy(otaCtrl->path,url,otaCtrl->path_len);

		gisunlink_create_task_with_Priority(gisunlink_ota_task, "ota_task", otaCtrl, 8192, 5); 
	}

	return;
}

void gisunlink_ota_init(void) {
	if(otaCtrl == NULL) {
		otaCtrl = (gisunlink_ota_ctrl *)gisunlink_malloc(sizeof(gisunlink_ota_ctrl));
		otaCtrl->download_process = 0;
		memset(otaCtrl->ota_write_data, 0, BUFFSIZE + 1);
	}
}
