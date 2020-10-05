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

#include "gisunlink.h"

#include "esp_ota_ops.h"
#include "gisunlink_ota.h" 
#include "gisunlink_print.h" 
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "gisunLink_atomic.h"
#include "esp_system.h"

#define BUFFSIZE 1500

typedef struct _gisunlink_ota {
	uint8 *path;
	uint32_t size;
	uint32 download_process;
} gisunlink_ota;

typedef struct _gisunlink_module_ctrl {
	void *thread_lock;
	bool start;
} gisunlink_ota_ctrl;

gisunlink_ota_ctrl *otaTask = NULL;

static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
	static uint32 process = 0;
	gisunlink_ota *task = (gisunlink_ota *)evt->user_data;
	switch(evt->event_id) {
		case HTTP_EVENT_ERROR:
		case HTTP_EVENT_ON_CONNECTED:
		case HTTP_EVENT_HEADER_SENT:
		case HTTP_EVENT_ON_HEADER:
		case HTTP_EVENT_ON_FINISH:
		case HTTP_EVENT_DISCONNECTED:
			break;
		case HTTP_EVENT_ON_DATA:
			if(task->download_process == 0) {
				process = 1;
			} else {
				//每10k输出一次
				if(task->download_process >= (process * 10240)) {
					++process;
					gisunlink_print(GISUNLINK_PRINT_WARN,"ota file size:%d download progress:%d",task->size,task->download_process);
				}
			}
			task->download_process += evt->data_len;
			break;
	}
	return ESP_OK;
}

static void gisunlink_ota_task_exec(void *parameter) {
	gisunlink_ota *task = (gisunlink_ota *)parameter;

	if(task) {
		gisunlink_print(GISUNLINK_PRINT_ERROR, "ota file:%s", (const char *)task->path);
		if(task && task->path) {
			esp_http_client_config_t config = {
				.url = (const char *)task->path,
				.event_handler = http_event_handler,
				.user_data = task,
			};

			esp_err_t ret = esp_https_ota(&config);//调用这个自动完成更新


			if (ret == ESP_OK && (task->size == task->download_process)) {
				printf("------------------------success-------------------------------------\n");
				esp_restart();
			} else {
				printf("------------------------error:%x-------------------------------------\n",ret);
				gisunlink_print(GISUNLINK_PRINT_ERROR, "Firmware Upgrades Failed");
			}

			if(task->path) {
				gisunlink_free(task->path);
				task->path = NULL;
			}

			if(task) {
				gisunlink_free(task);
				task = NULL;
			}
		}
	}

	gisunlink_destroy_task(NULL);
}

void gisunlink_ota_task(const char *url,unsigned int size) {

	if(otaTask == NULL) {
		return;
	}

	if(url && size) {
		gisunlink_get_lock(otaTask->thread_lock);
		if (otaTask->start == false) {
			otaTask->start = true;

			gisunlink_ota *ota = (gisunlink_ota *)gisunlink_malloc(sizeof(gisunlink_ota));
			uint16_t path_len = strlen(url) + 1;

			if(ota && path_len) {
				ota->path = (uint8_t *)gisunlink_malloc(path_len);
				memcpy(ota->path,url,path_len);
				ota->size = size;
				ota->download_process = 0;
				gisunlink_create_task_with_Priority(gisunlink_ota_task_exec, "ota_task", ota, 8192, 5); 
			} else {
				gisunlink_free(ota);
				ota = NULL;
			}
		}
		gisunlink_free_lock(otaTask->thread_lock);
	}

	return;
}

void gisunlink_ota_init(void) {
	if(otaTask == NULL) {
		otaTask = (gisunlink_ota_ctrl *)gisunlink_malloc(sizeof(gisunlink_ota_ctrl));
		otaTask->start = false;
		otaTask->thread_lock = gisunlink_create_lock();
	}
}
