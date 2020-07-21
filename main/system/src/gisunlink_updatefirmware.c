/*
* _COPYRIGHT_
*
* File Name: gisunlink_updatefirmware.c
* System Environment: JOHAN-PC
* Created Time:2019-04-18
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>

#include "rom/md5_hash.h"
#include "mbedtls/base64.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_http_client.h"

#include "fcntl.h"
#include "unistd.h"
#include "cJSON.h"

#include "gisunlink_config.h"
#include "gisunlink_atomic.h" 
#include "gisunlink_updatefirmware.h"

#define DOWNLOAD_NUM 5
#define MD5_MAX_LEN (33)
#define MAX_RESPOND_SIZE 1024
#define GISUNLINK_MAX_QUEUE_NUM 10

#define SEND_FIRMWARE_SIZE 256

#define UPD_FILE_PATH "/spiffs/download.bin"
#define FW_FILE_PATH  "/spiffs/firmware.bin"

typedef struct _gisunlink_updatefirmware_ctrl {
	FILE *file;
	uint32 download_process;
	bool start_download;
	uint32 download_runtime;
	void *thread_sem;
	gisunlink_firmware_download *download;
	gisunlink_firmware_update_hook *hook;
} gisunlink_updatefirmware_ctrl;

static gisunlink_updatefirmware_ctrl *updateCtrl = NULL;

static void gisunlink_firmware_download_free(gisunlink_firmware_download *download) {
	if(download) {
		if(download->path) {
			gisunlink_free(download->path);
			download->path = NULL;
		}

		if(download->md5) {
			gisunlink_free(download->md5);
			download->md5 = NULL;
		}

		gisunlink_free(download);
		download = NULL;
	}
}

static gisunlink_firmware_download *gisunlink_updatefirmware_json_proc(const char *jsonData, uint16 json_len) {
	cJSON *pJson = NULL; 
	gisunlink_firmware_download *download = NULL;
	struct cJSON_Hooks js_hook = {gisunlink_malloc, &gisunlink_free};
	if(jsonData && json_len) {
		cJSON_InitHooks(&js_hook);
		download = (gisunlink_firmware_download *)gisunlink_malloc(sizeof(gisunlink_firmware_download));
		download->download_over = false;
		if((pJson = cJSON_Parse(jsonData)) && download) { 
			cJSON *item = NULL;
			if((item = cJSON_GetObjectItem(pJson, "url")) && (item->type == cJSON_String)) {
				asprintf((char **)&download->path, "%s",item->valuestring);
				download->path_len = strlen(item->valuestring);
			} else {
				goto error_exit;
			}

			if((item = cJSON_GetObjectItem(pJson, "md5")) && (item->type == cJSON_String)) {
				asprintf((char **)&download->md5, "%s",item->valuestring);
				download->md5_len = strlen(item->valuestring);
			} else {
				goto error_exit;
			}
			if((item = cJSON_GetObjectItem(pJson, "size")) && (item->type == cJSON_Number)) {
				download->size = item->valueint;
			} else {
				goto error_exit;
			}
			if((item = cJSON_GetObjectItem(pJson, "ver")) && (item->type == cJSON_Number)) {
				download->ver = item->valueint;
			} else {
				goto error_exit;
			}
			goto normal_exit;
		} 
	}
error_exit:
	if(download) {
		gisunlink_firmware_download_free(download);
		download = NULL;
	}
normal_exit:
	if(pJson) {
		cJSON_Delete(pJson);
		pJson = NULL;
	}
	return download;
}

static gisunlink_firmware_download *gisunlink_get_download_task(void) {
	gisunlink_firmware_download *download = NULL;
	if((download = (gisunlink_firmware_download *)gisunlink_malloc(sizeof(gisunlink_firmware_download)))) {
		download->path_len = 256; download->md5_len = 64;
		download->path = (uint8 *)gisunlink_malloc(download->path_len);
		download->md5 = (uint8 *)gisunlink_malloc(download->md5_len);
		if(download->path && download->md5) {
			gisunlink_config_get(DOWNLOAD,download);
			if(download->path_len == 0 || download->ver == 0 || download->md5_len == 0) {
				gisunlink_firmware_download_free(download);
				download = NULL;
			}
		} else {
			gisunlink_firmware_download_free(download);
			download = NULL;
		}
	}
	return download;
}

void gisunlink_updatefirmware_download_new_firmware(const char *jsonData, uint16 json_len) {
	gisunlink_firmware_download *download = gisunlink_updatefirmware_json_proc(jsonData,json_len);

	if(download) {
		bool isDownload = false;
		//检查固件大小
		if(download->size <= 0) {
			gisunlink_firmware_download_free(download);
			download = NULL;
			return;
		} 
		gisunlink_firmware_download *last_download_task = gisunlink_get_download_task(); 
		// 之前有保存过固件信息
		if(last_download_task != NULL) {
			//比较md5值
			if(strncmp((const char *)last_download_task->md5,(const char *)download->md5,last_download_task->md5_len) != 0) { //相等则不处理了
				//不考虑版本号小的情况
				if(download->ver >= last_download_task->ver) {
					isDownload = true;
				} 
			}
			gisunlink_firmware_download_free(last_download_task);
			last_download_task = NULL;
		} else { //没有直接下载
			isDownload = true;
		}

		if(isDownload) {
			//先把需要更新的固件写入系统 防止再下载的时候漏掉多接收的新版本号,固件缓存后，需再次读出md5值 进行对比
			gisunlink_config_set(DOWNLOAD,download);
			gisunlink_print(GISUNLINK_PRINT_ERROR,"has new version come %s:%d",download->path,download->size);
			if(updateCtrl && updateCtrl->start_download == false) {
				updateCtrl->start_download = true;
				updateCtrl->download = download;
				gisunlink_put_sem(updateCtrl->thread_sem);
				return;
			} 
		}
		gisunlink_firmware_download_free(download);
	} 
} 

static void gisunlink_updatefirmware_close_file(FILE *file) {
	if(file) {
		fclose(file);
		file = NULL;
	}
}

static void gisunlink_updatefirmware_remove_file(const char *path) {
	struct stat st;
	if(stat(path, &st) == 0) {
		unlink(path);
	}
}

static int gisunlink_updatefirmware_rename_file(const char *path,const char *newpath) {
	int ret = -1;
	struct stat st;
	if(stat(path, &st) == 0) {
      ret = rename(path, newpath);
	}
	return ret;
}

static FILE *gisunlink_updatefirmware_open_file(const char *path,const char *mode) {
	FILE *file = fopen(path, mode);
	if(file == NULL) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"Failed to open file for writing %s",path);
		return NULL;
	}
	return file;
}

static esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
	static uint32 process = 0;
	gisunlink_updatefirmware_ctrl *ufw_ctrl = (gisunlink_updatefirmware_ctrl *)evt->user_data;
	switch(evt->event_id) {
		case HTTP_EVENT_ERROR:
		case HTTP_EVENT_ON_CONNECTED:
		case HTTP_EVENT_HEADER_SENT:
		case HTTP_EVENT_ON_HEADER:
		case HTTP_EVENT_ON_FINISH:
		case HTTP_EVENT_DISCONNECTED:
			break;
		case HTTP_EVENT_ON_DATA:
			if(ufw_ctrl->download_process == 0) {
				process = 1;
			} else {
				//每10k输出一次
				if(ufw_ctrl->download_process >= (process * 10240)) {
					++process;
					gisunlink_print(GISUNLINK_PRINT_WARN,"file size:%d download progress:%d",ufw_ctrl->download->size,ufw_ctrl->download_process);
				}
			}
			if(ufw_ctrl && ufw_ctrl->download_process < ufw_ctrl->download->size) {
				if(ufw_ctrl->file) {
					fwrite(evt->data,1,evt->data_len,ufw_ctrl->file);
				} 
			} else {
				gisunlink_print(GISUNLINK_PRINT_ERROR,"overflow file_size %d -->%d",ufw_ctrl->download->size,ufw_ctrl->download_process);
			}
			ufw_ctrl->download_process += evt->data_len;
			break;
	}
	return ESP_OK;
}

static int gisunlink_updatefirmware_http(gisunlink_updatefirmware_ctrl *ufw_ctrl) {
	uint32 download_size = 0;
	uint8 download_num = DOWNLOAD_NUM;
	esp_http_client_config_t config = {
		.url = (const char *)ufw_ctrl->download->path,
		.event_handler = _http_event_handler,
		.user_data = ufw_ctrl,
	};

	esp_http_client_handle_t client = esp_http_client_init(&config);
	esp_http_client_set_header(client,"User-Agent", "gisunLink_iot");
	if(ufw_ctrl->download && ufw_ctrl->download->path && ufw_ctrl->download->path_len) {
		while(download_num--) {
			gisunlink_updatefirmware_remove_file(UPD_FILE_PATH);
			ufw_ctrl->download_process = 0;
			ufw_ctrl->file = gisunlink_updatefirmware_open_file(UPD_FILE_PATH,"w"); //重新打开新文件
			esp_err_t err = esp_http_client_perform(client);
			gisunlink_updatefirmware_close_file(ufw_ctrl->file);//关闭文件
			if(err == ESP_OK) {
				if(esp_http_client_get_status_code(client) == 200 && esp_http_client_get_content_length(client)) {
					download_size = esp_http_client_get_content_length(client);
					if(download_size == ufw_ctrl->download->size && ufw_ctrl->download->size == ufw_ctrl->download_process) {
						gisunlink_print(GISUNLINK_PRINT_WARN,"%s download finish file_size:%d download_size:%d download_process:%d",ufw_ctrl->download->md5,ufw_ctrl->download->size,download_size,ufw_ctrl->download_process);
						break;
					} else {
						gisunlink_print(GISUNLINK_PRINT_ERROR,"download error, now and try again %d:%d:%d",ufw_ctrl->download->size,download_size,download_num);
					}
				} 
			} else {
				gisunlink_print(GISUNLINK_PRINT_ERROR,"download error, now and try again %d failed: %d",ufw_ctrl->download->size,download_size,download_num,err);
			}
		}
	}
	esp_http_client_cleanup(client);
	return download_size;
}

static bool gisunlink_updatefirmware_check_md5(const char *file_path,const char *md5,uint8 md5_len) {
	int16 read_size;
	bool md5_ok = false;
	struct MD5Context md5_ctx;
	uint8 data_buf[SEND_FIRMWARE_SIZE] = {0};
	char md5_value[MD5_MAX_LEN] = {0};
	unsigned char digest[MD5_MAX_LEN];

	FILE *file = gisunlink_updatefirmware_open_file(file_path,"r");

	if(file) {
		MD5Init(&md5_ctx);
		while((read_size = fread(data_buf,1,SEND_FIRMWARE_SIZE,file)) > 0) {
			MD5Update(&md5_ctx, (uint8 const *)data_buf, read_size);
		}

		MD5Final(digest, &md5_ctx);
		for (int i = 0; i < 16; ++i) {
			sprintf(&md5_value[i * 2], "%02x", (unsigned int)digest[i]);
		}

		if(strncmp(md5_value,md5,md5_len) != 0) {
			gisunlink_print(GISUNLINK_PRINT_ERROR,"md5 compare voer is failed md5:%s <--> %s",md5_value,md5);
			md5_ok = false;
		} else {
			md5_ok = true;
		} 
		gisunlink_updatefirmware_close_file(file);
	}

	return md5_ok; 
}

static void gisunlink_updatefirmware_save_new_firmware(gisunlink_firmware_download *download) {
	gisunlink_firmware_update *firmware = (gisunlink_firmware_update *)gisunlink_malloc(sizeof(gisunlink_firmware_update));
	if(firmware) {
		firmware->download = download;
		firmware->transfer_over = false;
		gisunlink_config_set(FIRMWARE,firmware);
		gisunlink_free(firmware);
		firmware = NULL;
	}
}

static void gisunlink_updatefirmware_download(gisunlink_updatefirmware_ctrl *ufw_ctrl) {
	if(ufw_ctrl != NULL) {
		while(ufw_ctrl->start_download) {
			bool chk_md5_ok = false;
			int download_size = 0;
			if((download_size = gisunlink_updatefirmware_http(ufw_ctrl))) {
				if(download_size == ufw_ctrl->download->size) { 
					chk_md5_ok = gisunlink_updatefirmware_check_md5(UPD_FILE_PATH,(const char *)ufw_ctrl->download->md5,ufw_ctrl->download->md5_len);
				} else {
					gisunlink_print(GISUNLINK_PRINT_ERROR,"download failed file_size:%d download_size:%d",ufw_ctrl->download->size,download_size);
				}
			}
			//检查是否有新的下载任务进来
			gisunlink_firmware_download *new_download_task = gisunlink_get_download_task();
			if(strncmp((const char *)ufw_ctrl->download->md5,(const char *)new_download_task->md5,new_download_task->md5_len) == 0) {
				if(chk_md5_ok) {
					gisunlink_updatefirmware_remove_file(FW_FILE_PATH);
					int ret = gisunlink_updatefirmware_rename_file(UPD_FILE_PATH,FW_FILE_PATH);
					if(ret == 0) {
						gisunlink_print(GISUNLINK_PRINT_WARN,"md5 company ok");
						gisunlink_updatefirmware_save_new_firmware(ufw_ctrl->download);
					} else {
						gisunlink_print(GISUNLINK_PRINT_WARN,"rename failed:%d",ret);
					}
				}
				ufw_ctrl->download->download_over = true;
				gisunlink_config_set(DOWNLOAD,ufw_ctrl->download);
				gisunlink_firmware_download_free(ufw_ctrl->download);
				ufw_ctrl->download = NULL;
				gisunlink_firmware_download_free(new_download_task);
				new_download_task = NULL;
				ufw_ctrl->start_download = false;
			} else {
				gisunlink_firmware_download_free(ufw_ctrl->download);
				ufw_ctrl->download = NULL;
				ufw_ctrl->download = new_download_task;
				gisunlink_print(GISUNLINK_PRINT_ERROR,"break current download task, because has a new version come in");
			}
		}
	}
}

static bool gisunlink_updatefirmware_download_check(gisunlink_firmware_download *download) {
	bool download_over = true;
	if(download) {
		if(download->download_over != true) {
			download_over = false;
		}
	} 	
	//没有保存过下载任务返回下载完成
	return download_over;
}

static uint32 gisunlink_updatefirmware_send_firmware(const char *file_path,gisunlink_firmware_update_hook *update_hook) {
	int16 read_size;
	uint8 data_buf[SEND_FIRMWARE_SIZE] = {0};

	if(update_hook == NULL|| update_hook->transfer == NULL) {
		return 0;
	}

	FILE *file = gisunlink_updatefirmware_open_file(file_path,"r");
	if(file) {
		uint16 offset = 0;
		while((read_size = fread(data_buf,1,SEND_FIRMWARE_SIZE,file)) > 0) {
			if(update_hook->transfer(++offset,data_buf,read_size)) {
				update_hook->send_size += read_size;
			} else {
				break;
			}
		}
		gisunlink_updatefirmware_close_file(file);
	}
	return update_hook->send_size;
}

static void gisunlink_updatefirmware_update(gisunlink_firmware_update_hook *update_hook) {
	if(update_hook == NULL || update_hook->query == NULL || update_hook->transfer == NULL || update_hook->check == NULL) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"update_hook is empty!!!");
		return;
	}

	gisunlink_firmware_update *firmware = (gisunlink_firmware_update *)gisunlink_malloc(sizeof(gisunlink_firmware_update));

	if(firmware) {
		if((firmware->download = (gisunlink_firmware_download *)gisunlink_malloc(sizeof(gisunlink_firmware_download)))) {
			firmware->download->path_len = 256; firmware->download->md5_len = 64;
			firmware->download->path = (uint8 *)gisunlink_malloc(firmware->download->path_len);
			firmware->download->md5 = (uint8 *)gisunlink_malloc(firmware->download->md5_len);
		} else {
			goto error_exit;
		}
	}

	gisunlink_config_get(FIRMWARE,firmware);

	if(firmware->transfer_over == false && firmware->download->size > 0 && firmware->download->ver > 0) {
		gisunlink_print(GISUNLINK_PRINT_WARN,"firmware:%s md5:%s size:%d",firmware->download->path,firmware->download->md5,firmware->download->size);
		bool transfer_over = false;
		bool clean_version = false;
		update_hook->update_retry = false;
		update_hook->update_retry_tick = 0;
		switch(update_hook->query(firmware)) {
			case GISUNLINK_NEED_UPGRADE:
				{
					update_hook->update = true;
					update_hook->version = firmware->download->ver;
					update_hook->file_size = firmware->download->size;
					if(gisunlink_updatefirmware_send_firmware(FW_FILE_PATH,update_hook) == firmware->download->size) {
						transfer_over = true;
					}
				}
				break;
			case GISUNLINK_NO_NEED_UPGRADE:
				clean_version = true;
				gisunlink_print(GISUNLINK_PRINT_ERROR,"device no need to update firmware");
				break;
			case GISUNLINK_DEVICE_TIMEOUT: 
				{
					update_hook->update_retry = true;
					gisunlink_print(GISUNLINK_PRINT_ERROR,"device is off-line");
					break;
				}
		}

		if(transfer_over) {
			gisunlink_print(GISUNLINK_PRINT_WARN,"firmware transfer finish");
			switch(update_hook->check()) {
				case GISUNLINK_FIRMWARE_CHK_OK:
					clean_version = true;
					break;
				case GISUNLINK_DEVICE_TIMEOUT:
					gisunlink_print(GISUNLINK_PRINT_ERROR,"device is off-line");
					break;
				case GISUNLINK_FIRMWARE_CHK_NO_OK:
				default:
					clean_version = false;
					gisunlink_print(GISUNLINK_PRINT_ERROR,"device check data error!");
					break;
			}
		} else {
			gisunlink_print(GISUNLINK_PRINT_ERROR,"firmware transfer unfinished");
		}

		if(clean_version) {
			gisunlink_updatefirmware_remove_file(FW_FILE_PATH);
			firmware->transfer_over = true;
			gisunlink_config_set(FIRMWARE,firmware);
			if(transfer_over) {
				gisunlink_print(GISUNLINK_PRINT_WARN,"device succeed receive data!");
			}
		} 
	} else {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"no think to do");
	}

	gisunlink_firmware_download_free(firmware->download);
	firmware->download = NULL;
error_exit:
	gisunlink_free(firmware);
	firmware = NULL;
	update_hook->update = false;
	update_hook->file_size = 0;
	update_hook->send_size = 0;
	update_hook->version = 0;
}

static void gisunlink_updatefirmware_task(void *param) {
	gisunlink_updatefirmware_ctrl *ufw_ctrl = (gisunlink_updatefirmware_ctrl *)param;
	while(1) {
		gisunlink_get_sem(ufw_ctrl->thread_sem);
		while(1) {
			gisunlink_updatefirmware_download(ufw_ctrl);
			//取下载配置
			gisunlink_firmware_download *last_download_task = gisunlink_get_download_task();
			if(gisunlink_updatefirmware_download_check(last_download_task)) {
				gisunlink_firmware_download_free(last_download_task);
				last_download_task = NULL;
				break;
			} else {
				//从新下载	
				gisunlink_firmware_download_free(ufw_ctrl->download);
				ufw_ctrl->download = NULL;
				ufw_ctrl->download = last_download_task;
				ufw_ctrl->start_download = true;
				gisunlink_print(GISUNLINK_PRINT_ERROR,"the file:%s download unfinished now go continue download it",last_download_task->md5);
			}
		}
		//检测固件升级
		gisunlink_updatefirmware_update(ufw_ctrl->hook);
	}
	gisunlink_destroy_task(NULL);
}

void gisunlink_updatefirmware_init(void) {
	if(updateCtrl) {
		return;
	}

	if((updateCtrl = (gisunlink_updatefirmware_ctrl *)gisunlink_malloc(sizeof(gisunlink_updatefirmware_ctrl)))) {
		updateCtrl->start_download = false;
		updateCtrl->download_runtime = 0;
		updateCtrl->thread_sem = gisunlink_create_sem(GISUNLINK_MAX_QUEUE_NUM,0);
		gisunlink_create_task_with_Priority(gisunlink_updatefirmware_task, "fw_task", updateCtrl, 2048,8); //3072 // 2048 // 2650
	}
}

void gisunlink_updatefirmware_start_signal(void) {
	if(updateCtrl && updateCtrl->start_download == false) {
		gisunlink_put_sem(updateCtrl->thread_sem);
	}
}

void gisunlink_updatefirmware_register_hook(gisunlink_firmware_update_hook *hook) {
	if(updateCtrl) {
		updateCtrl->hook = hook;
	}
}
