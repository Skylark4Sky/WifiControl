/*
* _COPYRIGHT_
*
* File Name: gisunlink_authorization.c
* System Environment: JOHAN-PC
* Created Time:2020-08-07
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#include "cJSON.h"
#include "string.h"
#include "gisunlink.h"
#include "esp_aes.h"
#include "esp_http_client.h"
#include "gisunlink_atomic.h"
#include "gisunlink_config.h"
#include "gisunlink_authorization.h"

#ifndef AES_BLOCK_SIZE
#define AES_BLOCK_SIZE 16
#endif


#define AUTHORIZATION_SERVICE "http://go.sky6.cn:8888/api/device"

//{0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x30,0x31,0x32,0x33,0x34,0x35,0x36}
const char aes_key_code[] = "78hrey23y28ogs89";
const char aes_iv_code[] = "1234567890123456";

typedef struct _gisunlink_authorization {
	bool taskStartFlags;		//线程执行标志
	void *userData;				//用户私有数据
} gisunlink_authorization_ctrl;

static gisunlink_authorization_ctrl *authorization_ctrl = NULL;

static void chkAuthorizationMessae(int status_code, const char *data, int data_len, void *param) {
	gisunlink_authorization_ctrl *authorization = (gisunlink_authorization_ctrl *)param; 
	gisunlink_system_ctrl *gisunlink_system = (gisunlink_system_ctrl *)authorization->userData;
	if(status_code == 200) {
		struct cJSON_Hooks js_hook = {gisunlink_malloc, &gisunlink_free};
		cJSON_InitHooks(&js_hook);
		cJSON *root = cJSON_Parse(data);
		if(root && root->type == cJSON_Object) {
			char *jsonString = cJSON_Print(root);
			gisunlink_print(GISUNLINK_PRINT_WARN,"%s",jsonString);
			gisunlink_free(jsonString);

			cJSON *status =  cJSON_GetObjectItem(root, "status");
			if(status && (status->type == cJSON_True || status->type == cJSON_False)) {
				bool chkOk = status->valueint;
				gisunlink_system->authorization = chkOk;
			}
		}
		cJSON_Delete(root);
		root = NULL;
	}
}

static esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
	switch(evt->event_id) {
		case HTTP_EVENT_ERROR:
		case HTTP_EVENT_ON_CONNECTED:
		case HTTP_EVENT_HEADER_SENT:
		case HTTP_EVENT_ON_HEADER:
		case HTTP_EVENT_ON_FINISH:
		case HTTP_EVENT_DISCONNECTED:
			break;
		case HTTP_EVENT_ON_DATA:
			if (!esp_http_client_is_chunked_response(evt->client)) {
				chkAuthorizationMessae(esp_http_client_get_status_code(evt->client),evt->data,evt->data_len,evt->user_data);
			}
			break;
	}
	return ESP_OK;
}

static char *createEncryptString(const char *key, const char *iv, uint8 *data, int data_len) {
	int ret;
	esp_aes_t ctx;
	uint8_t iv_tmp[AES_BLOCK_SIZE];
	uint8 encryptLen = gisunlink_bytes_align(data_len,AES_BLOCK_SIZE);
	uint8 *encryptBuf = NULL;
	uint8 hexTable[AES_BLOCK_SIZE] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};

	ret = esp_aes_set_encrypt_key(&ctx, key, 128);
	if (ret) {
		return NULL;
	}

	encryptBuf = (uint8 *)gisunlink_malloc(encryptLen);
	memcpy(encryptBuf,data,data_len);
	memcpy(iv_tmp, iv, AES_BLOCK_SIZE);

	ret = esp_aes_encrypt_cbc(&ctx, encryptBuf, encryptLen, encryptBuf, encryptLen, iv_tmp);
	if (ret) {
		if(encryptBuf) {
			gisunlink_free(encryptBuf);
			encryptBuf = NULL;
		}
		return NULL;
	}

	char *toHexStringBuf = (char *)gisunlink_malloc((encryptLen * 2) + 1);

	for(int i = 0; i < encryptLen; i++) {
		uint8 data = encryptBuf[i];
		uint8 offset = i * 2;
		toHexStringBuf[offset] = hexTable[(data & 0xF0) >> 4]; 
		toHexStringBuf[offset + 1] = hexTable[(data & 0x0F)]; 
	}

	if(encryptBuf) {
		gisunlink_free(encryptBuf);
		encryptBuf = NULL;
	}

	return toHexStringBuf;
}

static bool connectToAuthorizationService(const char *host,gisunlink_system_ctrl *gisunlink_system) {
	char *post_data = NULL;
	char *post_url = NULL;
	char *clientID = gisunlink_get_mac_with_string(NULL);
	char *AESEncryptData = createEncryptString(aes_key_code,aes_iv_code,(uint8 *)clientID,strlen(clientID));

	asprintf(&post_url,"%s?clientID=%s&version=%s",host,clientID,WIFI_FIRMWARE_VERSION);

	esp_http_client_config_t config = {
		.url = post_url,
		.event_handler = _http_event_handler,
		.user_data = authorization_ctrl,	
		.timeout_ms = 15000, /*十五秒*/ 
	};
	
	int post_len = 0;

	post_len = asprintf(&post_data,"{ \"deviceSN\":\"%s\",\"token\":\"%s\"}",gisunlink_system->deviceHWSn,AESEncryptData); 
	
	esp_http_client_handle_t client = esp_http_client_init(&config);
	esp_http_client_set_method(client, HTTP_METHOD_POST);
	esp_http_client_set_header(client,"Content-Type", "application/json");
	esp_http_client_set_header(client,"User-Agent", "gisunLink_iot");
	esp_http_client_set_post_field(client, post_data, post_len); 

	esp_err_t err = esp_http_client_perform(client);

	if(post_url) {
		gisunlink_print(GISUNLINK_PRINT_WARN,"URL:%s",post_url);
		gisunlink_free(post_url);
		post_url = NULL;
	}	

	if(post_data) {
		gisunlink_print(GISUNLINK_PRINT_WARN,"POST:%s",post_data);
		gisunlink_free(post_data);
		post_data = NULL;
	}	

	if(clientID) {
		gisunlink_free(clientID);
		clientID = NULL;
	}

	if(AESEncryptData) {
		gisunlink_free(AESEncryptData);
		AESEncryptData = NULL;
	}

	if(err == ESP_OK) {
		gisunlink_print(GISUNLINK_PRINT_WARN,"HTTP POST Status = %d, content_length = %d", esp_http_client_get_status_code(client), esp_http_client_get_content_length(client));
	} 

	esp_http_client_cleanup(client);
	client = NULL;

	return true;
}

static void gisunlink_authorization_task(void *param) {
	gisunlink_authorization_ctrl *authorization = (gisunlink_authorization_ctrl *)param;
	if(authorization) {
		connectToAuthorizationService(AUTHORIZATION_SERVICE,authorization->userData);
		authorization_ctrl->taskStartFlags = false;
	}
	gisunlink_destroy_task(NULL);
}

void gisunlink_authorization_init(gisunlink_system_ctrl *gisunlink_system) {
	if(!authorization_ctrl && gisunlink_system) {
		authorization_ctrl = (gisunlink_authorization_ctrl *)gisunlink_malloc(sizeof(gisunlink_authorization_ctrl));
		if(authorization_ctrl) {
			authorization_ctrl->userData = gisunlink_system;
			authorization_ctrl->taskStartFlags = false;
		}
	}
}

void gisunlink_authorization_start_task(void) {
	if(authorization_ctrl && !authorization_ctrl->taskStartFlags) {
		authorization_ctrl->taskStartFlags = true;
		gisunlink_create_task(gisunlink_authorization_task, "authorization_opt", authorization_ctrl, 3072);
	}
}

