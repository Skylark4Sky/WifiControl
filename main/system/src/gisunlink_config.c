/*
* _COPYRIGHT_
*
* File Name: gisunlink_config.c
* System Environment: JOHAN-PC
* Created Time:2019-01-22
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#include <stdio.h>
#include <string.h>

#include "nvs.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_system.h"

#include "gisunlink.h"
#include "gisunlink_config.h"
#include "gisunlink_mountDisk.h"

//开机模式
#define GISUNLINK_POWERON_NVS_NAME "POWER"
#define GISUNLINK_POWERON_MODE_NVS "POWER_MODE"
//wifi配置
#define GISUNLINK_WIFI_NVS_NAME "WIFI"
#define GISUNLINK_WIFI_SSID_NVS "WIFI_SSID"
#define GISUNLINK_WIFI_PSWD_NVS "WIFI_PSWD"
//授权配置
#define GISUNLINK_AUTHORIZATION_NVS_NAME "AUTHORIZE"
#define GISUNLINK_AUTHORIZATION_MODE_NAME "AUTHORIZE_MODE"
//令牌配置
#define GISUNLINK_TOKEN_NVS_NAME "TOKEN"
#define GISUNLINK_TOKEN_DATA_NVS "TOKEN_DATA"
//设备号
#define GISUNLINK_SN_CODE_NVS_NAME "SN_CODE"
#define GISUNLINK_SN_CODE_DATA_NVS "SN_CODE_DATA"
//版本配置
#define GISUNLINK_DOWNLOAD_NVS_NAME "DOWNLOAD"
#define GISUNLINK_FIRMWARE_NVS_NAME "FIRMWARE"

#define GISUNLINK_PATH_DATA_NVS "PATH_DATA"
#define GISUNLINK_MD5_DATA_NVS "MD5_DATA"
#define GISUNLINK_SIZE_DATA_NVS "SIZE_DATA"
#define GISUNLINK_VER_DATA_NVS "VER_DATA"
#define GISUNLINK_OVER_DATA_NVS "OVER_DATA"

#define GISUNLINK_TRANSFER_OVER_DATA_NVS "TRANSFER_DATA"

#define READ_ACTION 0
#define WRITE_ACTION 1

static bool read_mac = false;
static uint8 mac_addr[MACMAXLEN];

static esp_err_t gisunlink_config_nvs_flash_init(void) {
     esp_err_t err = nvs_flash_init();
     if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
         ESP_ERROR_CHECK(nvs_flash_erase());
         err = nvs_flash_init();
     }
     ESP_ERROR_CHECK(err);
     return err;
 }

void gisunlink_config_init(void) {
	esp_err_t err = gisunlink_config_nvs_flash_init();
    if(err != ESP_OK) {
        gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_flash_init:%d",err);
    }

	if(false == gisunlink_mount_disk()) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"gisunlink_mountDisk failed!");
	} 
}

static bool gisunlink_config_get_wifi(gisunlink_wifi_conf *conf) {
	esp_err_t err = ESP_OK;
	nvs_handle NvsHandle;
	if(conf == NULL) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"conf is invalid memory address!!!");
		return false;
	}

	memset(conf,0x0,sizeof(gisunlink_wifi_conf));

	if((err = nvs_open(GISUNLINK_WIFI_NVS_NAME, NVS_READWRITE, &NvsHandle)) != ESP_OK) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_open %s :%d",GISUNLINK_WIFI_NVS_NAME,err);
		nvs_close(NvsHandle);
		return false;
	}	

	conf->ssid_len = sizeof(conf->ssid);
	conf->pswd_len = sizeof(conf->pswd);

	if((err = nvs_get_blob(NvsHandle, GISUNLINK_WIFI_SSID_NVS, conf->ssid,(size_t *)&conf->ssid_len)) != ESP_OK) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_get %s :%d",GISUNLINK_WIFI_SSID_NVS,err);
	}

	if((err = nvs_get_blob(NvsHandle, GISUNLINK_WIFI_PSWD_NVS, conf->pswd,(size_t *)&conf->pswd_len)) != ESP_OK) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_get %s :%d",GISUNLINK_WIFI_PSWD_NVS,err);
	}
	nvs_close(NvsHandle);	
	return true;
}

static bool gisunlink_config_set_wifi(gisunlink_wifi_conf *conf) {
	esp_err_t err = ESP_OK;
	nvs_handle NvsHandle;

	if(conf == NULL) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"conf is invalid memory address!!!");
		return false;
	}

	if((err = nvs_open(GISUNLINK_WIFI_NVS_NAME, NVS_READWRITE, &NvsHandle)) != ESP_OK) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_open %s :%d",GISUNLINK_WIFI_NVS_NAME,err);
		nvs_close(NvsHandle);
		return false;
	}	

	if((err = nvs_set_blob(NvsHandle, GISUNLINK_WIFI_SSID_NVS, conf->ssid,conf->ssid_len)) != ESP_OK) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_set %s :%d",GISUNLINK_WIFI_SSID_NVS,err);
		nvs_close(NvsHandle);
		return false;
	}

	if((err = nvs_set_blob(NvsHandle, GISUNLINK_WIFI_PSWD_NVS, conf->pswd,conf->pswd_len)) != ESP_OK) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_set %s :%d",GISUNLINK_WIFI_PSWD_NVS,err);
		nvs_close(NvsHandle);
		return false;
	}

	if((err = nvs_commit(NvsHandle)) != ESP_OK) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"set_wifi nvs_commit:%d",err);
		nvs_close(NvsHandle);
		return false;
	}

	gisunlink_print(GISUNLINK_PRINT_INFO,"save SSID:%s PSWD:%s",conf->ssid,conf->pswd);
	nvs_close(NvsHandle);
	return true;
}

static bool gisunlink_config_get_general(uint8 conf_id, uint8 *conf,int8 *nvs_name, int8 *nvs_mode) {
	esp_err_t err = ESP_OK;
	nvs_handle NvsHandle;

	if(conf == NULL) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"conf is invalid memory address!!!");
		return false;
	}

	if((err = nvs_open(nvs_name, NVS_READWRITE, &NvsHandle)) != ESP_OK) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_open %s :%d",nvs_name,err);
		nvs_close(NvsHandle);
		return false;
	}	

	if((err = nvs_get_u8(NvsHandle, nvs_mode, conf)) != ESP_OK) {
		switch (err) {
			case ESP_ERR_NVS_NOT_FOUND:
				{
					if(POWERON_CONFIG == conf_id) {
						//第一次开机没有配置，则默认为WIFI配置模式
						*conf = WIFI_UNCONFIGURED_MODE;
					}

					if(AUTHORIZATION_CONFIG == conf_id) {
						//第一次开机没有配置，则默认为未知状态
						*conf = DEVICE_AUTHORIZE_UNKNOWN;
					}
					break;
				}
			default :
				gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_get %s :%d",nvs_mode,err);
		}
	}

	nvs_close(NvsHandle);	
	return true;
}

static bool gisunlink_config_set_general(uint8 conf_id, uint8 *conf,int8 *nvs_name, int8 *nvs_mode) {
	esp_err_t err = ESP_OK;
	nvs_handle NvsHandle;

	if(conf == NULL) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"conf is invalid memory address!!!");
		return false;
	}

	if((err = nvs_open(nvs_name, NVS_READWRITE, &NvsHandle)) != ESP_OK) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_open %s :%d",nvs_name,err);
		nvs_close(NvsHandle);
		return false;
	}	

     if((err  = nvs_set_u8(NvsHandle, nvs_mode, *conf)) != ESP_OK) {
         nvs_close(NvsHandle);
		 gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_set %s :%d",nvs_mode,err);
         return false;
     }

	if((err = nvs_commit(NvsHandle)) != ESP_OK) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"set_wifi_poweron nvs_commit:%d",err);
		nvs_close(NvsHandle);
		return false;
	}

	gisunlink_print(GISUNLINK_PRINT_INFO,"save power on mode to:%d",*conf);

	nvs_close(NvsHandle);	
	return true;
}

static bool gisunlink_config_get_system(void *conf) {
	return true;
}


static bool gisunlink_config_set_system(void *conf) {

	return true;
}

static bool gisunlink_config_get_token(gisunlink_token_conf *token) {
	esp_err_t err = ESP_OK;
	nvs_handle NvsHandle;
	if(token == NULL) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"conf is invalid memory address!!!");
		return false;
	}

	memset(token,0x0,sizeof(gisunlink_token_conf));

	if((err = nvs_open(GISUNLINK_TOKEN_NVS_NAME, NVS_READWRITE, &NvsHandle)) != ESP_OK) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_open %s :%d",GISUNLINK_TOKEN_NVS_NAME,err);
		nvs_close(NvsHandle);
		return false;
	}	

	token->length = sizeof(token->string);

	if((err = nvs_get_blob(NvsHandle, GISUNLINK_TOKEN_DATA_NVS, token->string,(size_t *)&token->length)) != ESP_OK) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_get %s :%d",GISUNLINK_TOKEN_DATA_NVS,err);
	}

	nvs_close(NvsHandle);	
	return true;
}

static bool gisunlink_config_set_token(gisunlink_token_conf *token) {
	esp_err_t err = ESP_OK;
	nvs_handle NvsHandle;

	if(token == NULL) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"conf is invalid memory address!!!");
		return false;
	}

	if((err = nvs_open(GISUNLINK_TOKEN_NVS_NAME, NVS_READWRITE, &NvsHandle)) != ESP_OK) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_open %s :%d",GISUNLINK_TOKEN_NVS_NAME,err);
		nvs_close(NvsHandle);
		return false;
	}	

	if((err = nvs_set_blob(NvsHandle, GISUNLINK_TOKEN_DATA_NVS, token->string,token->length)) != ESP_OK) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_set %s :%d",GISUNLINK_TOKEN_DATA_NVS,err);
		nvs_close(NvsHandle);
		return false;
	}

	if((err = nvs_commit(NvsHandle)) != ESP_OK) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"set_wifi nvs_commit:%d",err);
		nvs_close(NvsHandle);
		return false;
	}

	gisunlink_print(GISUNLINK_PRINT_INFO,"save TOKEN:%s",token->string);
	nvs_close(NvsHandle);
	return true;
}

static bool gisunlink_config_get_sn_code(gisunlink_sn_code_conf *sn_code) {
	esp_err_t err = ESP_OK;
	char *sn_code_str = "gisunlink_iot";
	nvs_handle NvsHandle;
	if(sn_code == NULL) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"conf is invalid memory address!!!");
		return false;
	}

	memset(sn_code,0x0,sizeof(gisunlink_sn_code_conf));

	if((err = nvs_open(GISUNLINK_SN_CODE_NVS_NAME, NVS_READWRITE, &NvsHandle)) != ESP_OK) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_open %s :%d",GISUNLINK_SN_CODE_NVS_NAME,err);
		nvs_close(NvsHandle);
		return false;
	}	

	sn_code->length = sizeof(sn_code->string);

	if((err = nvs_get_blob(NvsHandle, GISUNLINK_SN_CODE_DATA_NVS, sn_code->string,(size_t *)&sn_code->length)) != ESP_OK) {
		switch (err) {
			case ESP_ERR_NVS_NOT_FOUND:
				sn_code->length = strlen(sn_code_str);
				memcpy(sn_code->string,sn_code_str,sn_code->length);
				break;
			default:
				gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_get %s :%d",GISUNLINK_SN_CODE_DATA_NVS,err);
		}
	}

	nvs_close(NvsHandle);	
	return true;
}

static bool gisunlink_config_set_sn_code(gisunlink_sn_code_conf *sn_code) {
	esp_err_t err = ESP_OK;
	nvs_handle NvsHandle;

	if(sn_code == NULL) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"conf is invalid memory address!!!");
		return false;
	}

	if((err = nvs_open(GISUNLINK_SN_CODE_NVS_NAME, NVS_READWRITE, &NvsHandle)) != ESP_OK) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_open %s :%d",GISUNLINK_SN_CODE_NVS_NAME,err);
		nvs_close(NvsHandle);
		return false;
	}	

	if((err = nvs_set_blob(NvsHandle, GISUNLINK_SN_CODE_DATA_NVS, sn_code->string,sn_code->length)) != ESP_OK) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_set %s :%d",GISUNLINK_SN_CODE_DATA_NVS,err);
		nvs_close(NvsHandle);
		return false;
	}

	if((err = nvs_commit(NvsHandle)) != ESP_OK) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"set_wifi nvs_commit:%d",err);
		nvs_close(NvsHandle);
		return false;
	}

	gisunlink_print(GISUNLINK_PRINT_INFO,"save sn_cpde:%s",sn_code->string);
	nvs_close(NvsHandle);
	return true;
}

static bool gisunlink_config_get_mac(uint8 *conf) {
	if(conf == NULL) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"conf is invalid memory address!!!");
		return false;
	}

	if(read_mac == false) {
		if(esp_read_mac(mac_addr, ESP_MAC_WIFI_STA) != ESP_OK) {
			gisunlink_print(GISUNLINK_PRINT_ERROR,"read mac failed!!!");
			return false;
		}
		read_mac = true;
		memcpy(conf,mac_addr,MACMAXLEN);
	} else {
		memcpy(conf,mac_addr,MACMAXLEN);
	}

	return true;
}

static bool gisunlink_config_download_path(gisunlink_firmware_download *download,nvs_handle NvsHandle, uint8 action) {
	esp_err_t err = ESP_OK;
	switch(action) {
		case READ_ACTION: 
			{
				if((err = nvs_get_blob(NvsHandle, GISUNLINK_PATH_DATA_NVS, download->path,(size_t *)&download->path_len)) != ESP_OK) {
					switch (err) {
						case ESP_ERR_NVS_NOT_FOUND:
							download->path_len = 0; 
							break;
						default:
							gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_get %s :%d",GISUNLINK_PATH_DATA_NVS,err);
					}
				}
			}
			break;
		case WRITE_ACTION:
			{
				if((err = nvs_set_blob(NvsHandle, GISUNLINK_PATH_DATA_NVS,download->path,download->path_len)) != ESP_OK) {
					gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_set %s :%d",GISUNLINK_PATH_DATA_NVS,err);
					return false;
				}
			}
			break;
	}
	return true;
}

static bool gisunlink_config_download_md5(gisunlink_firmware_download *download,nvs_handle NvsHandle, uint8 action) {
	esp_err_t err = ESP_OK;
	switch(action) {
		case READ_ACTION:
			{
				if((err = nvs_get_blob(NvsHandle, GISUNLINK_MD5_DATA_NVS, download->md5,(size_t *)&download->md5_len)) != ESP_OK) {
					switch (err) {
						case ESP_ERR_NVS_NOT_FOUND:
							download->md5_len = 0; 
							break;
						default:
							gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_get %s :%d",GISUNLINK_MD5_DATA_NVS,err);
					}
				}
			}
			break;
		case WRITE_ACTION:
			{
				if((err = nvs_set_blob(NvsHandle, GISUNLINK_MD5_DATA_NVS,download->md5,download->md5_len)) != ESP_OK) {
					gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_set %s :%d",GISUNLINK_MD5_DATA_NVS,err);
					return false;
				}
			}
			break;
	}
	return true;
}

static bool gisunlink_config_download_size(gisunlink_firmware_download *download,nvs_handle NvsHandle, uint8 action) {
	esp_err_t err = ESP_OK;
	switch(action) {
		case READ_ACTION:
			{
				if((err = nvs_get_u32(NvsHandle, GISUNLINK_SIZE_DATA_NVS, (uint32_t*)&download->size)) != ESP_OK) {
					switch (err) {
						case ESP_ERR_NVS_NOT_FOUND:
							download->size = 0; 
							break;
						default:
							gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_get %s :%d",GISUNLINK_SIZE_DATA_NVS,err);
					}
				}
			}
			break;
		case WRITE_ACTION:
			{
				if((err = nvs_set_u32(NvsHandle, GISUNLINK_SIZE_DATA_NVS,download->size)) != ESP_OK) {
					gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_set %s :%d",GISUNLINK_SIZE_DATA_NVS,err);
					return false;
				}
			}
			break;
	}
	return true;
}

static bool gisunlink_config_download_ver(gisunlink_firmware_download *download,nvs_handle NvsHandle, uint8 action) {
	esp_err_t err = ESP_OK;
	switch(action) {
		case READ_ACTION:
			{
				if((err = nvs_get_u32(NvsHandle, GISUNLINK_VER_DATA_NVS, (uint32_t*)&download->ver)) != ESP_OK) {
					switch (err) {
						case ESP_ERR_NVS_NOT_FOUND:
							download->ver = 0; 
							break;
						default:
							gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_get %s :%d",GISUNLINK_VER_DATA_NVS,err);
					}
				}
			}
			break;
		case WRITE_ACTION:
			{
				if((err = nvs_set_u32(NvsHandle, GISUNLINK_VER_DATA_NVS,download->ver)) != ESP_OK) {
					gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_set %s :%d",GISUNLINK_VER_DATA_NVS,err);
					return false;
				}
			}
			break;
	}
	return true;
}

static bool gisunlink_config_download_over(gisunlink_firmware_download *download,nvs_handle NvsHandle, uint8 action) {
	esp_err_t err = ESP_OK;
	switch(action) {
		case READ_ACTION:
			{
				if((err = nvs_get_u8(NvsHandle, GISUNLINK_OVER_DATA_NVS, (uint8_t*)&download->download_over)) != ESP_OK) {
					switch (err) {
						case ESP_ERR_NVS_NOT_FOUND:
							download->download_over = false; 
							break;
						default:
							gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_get %s :%d",GISUNLINK_OVER_DATA_NVS,err);
					}
				}
			}
			break;
		case WRITE_ACTION:
			{
				if((err = nvs_set_u8(NvsHandle, GISUNLINK_OVER_DATA_NVS, download->download_over)) != ESP_OK) {
					gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_set %s :%d download_over = %d",GISUNLINK_OVER_DATA_NVS,err,download->download_over);
					return false;
				}
			}
			break;
	}
	return true;
}

static bool gisunlink_config_get_download_task(gisunlink_firmware_download *download) {
	esp_err_t err = ESP_OK;
	nvs_handle NvsHandle;
	if(download == NULL) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"conf is invalid memory address!!!");
		return false;
	}

	if((err = nvs_open(GISUNLINK_DOWNLOAD_NVS_NAME, NVS_READWRITE, &NvsHandle)) != ESP_OK) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_open %s :%d",GISUNLINK_DOWNLOAD_NVS_NAME,err);
		nvs_close(NvsHandle);
		return false;
	}	

	memset(download->path,0x0,download->path_len);
	memset(download->md5,0x0,download->md5_len);

	gisunlink_config_download_path(download,NvsHandle,READ_ACTION);
	gisunlink_config_download_md5(download,NvsHandle,READ_ACTION);
	gisunlink_config_download_size(download,NvsHandle,READ_ACTION);
	gisunlink_config_download_ver(download,NvsHandle,READ_ACTION);
	gisunlink_config_download_over(download,NvsHandle,READ_ACTION);

	nvs_close(NvsHandle);	
	return true;
}

static bool gisunlink_config_set_download_task(gisunlink_firmware_download *download) {
	esp_err_t err = ESP_OK;
	nvs_handle NvsHandle;

	if(download == NULL) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"conf is invalid memory address!!!");
		return false;
	}

	if((err = nvs_open(GISUNLINK_DOWNLOAD_NVS_NAME, NVS_READWRITE, &NvsHandle)) != ESP_OK) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_open %s :%d",GISUNLINK_DOWNLOAD_NVS_NAME,err);
		nvs_close(NvsHandle);
		return false;
	}	

	if (gisunlink_config_download_path(download,NvsHandle,WRITE_ACTION) == false) {
		nvs_close(NvsHandle);
		return false;
	}

	if(gisunlink_config_download_md5(download,NvsHandle,WRITE_ACTION) == false) { 
		nvs_close(NvsHandle);
		return false;
	}

	if(gisunlink_config_download_size(download,NvsHandle,WRITE_ACTION) == false) {
		nvs_close(NvsHandle);
		return false;
	}

	if(gisunlink_config_download_ver(download,NvsHandle,WRITE_ACTION) == false) {
		nvs_close(NvsHandle);
		return false;
	}

	if(gisunlink_config_download_over(download,NvsHandle,WRITE_ACTION) == false) { 
		nvs_close(NvsHandle);
		return false;
	}

	if((err = nvs_commit(NvsHandle)) != ESP_OK) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"set_wifi nvs_commit:%d",err);
		nvs_close(NvsHandle);
		return false;
	}

	gisunlink_print(GISUNLINK_PRINT_INFO,"save download path(%d):%s size:%d md5(%d):%s ver:%d",download->path_len,download->path,download->size,download->md5_len,download->md5,download->ver);
	nvs_close(NvsHandle);
	return true;
}

static bool gisunlink_config_get_firmware(gisunlink_firmware_update *update) {
	esp_err_t err = ESP_OK;
	nvs_handle NvsHandle;
	if(update == NULL) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"conf is invalid memory address!!!");
		return false;
	}

	if((err = nvs_open(GISUNLINK_FIRMWARE_NVS_NAME, NVS_READWRITE, &NvsHandle)) != ESP_OK) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_open %s :%d",GISUNLINK_FIRMWARE_NVS_NAME,err);
		nvs_close(NvsHandle);
		return false;
	}	

	memset(update->download->path,0x0,update->download->path_len);
	memset(update->download->md5,0x0,update->download->md5_len);

	gisunlink_config_download_path(update->download,NvsHandle,READ_ACTION);
	gisunlink_config_download_md5(update->download,NvsHandle,READ_ACTION);
	gisunlink_config_download_size(update->download,NvsHandle,READ_ACTION);
	gisunlink_config_download_ver(update->download,NvsHandle,READ_ACTION);
	gisunlink_config_download_over(update->download,NvsHandle,READ_ACTION);

	if((err = nvs_get_u8(NvsHandle, GISUNLINK_TRANSFER_OVER_DATA_NVS, (uint8_t*)&update->transfer_over)) != ESP_OK) {
		switch (err) {
			case ESP_ERR_NVS_NOT_FOUND:
				update->transfer_over = false; 
				break;
			default:
				gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_get %s :%d",GISUNLINK_TRANSFER_OVER_DATA_NVS,err);
		}
	}

	nvs_close(NvsHandle);	
	return true;
}

static bool gisunlink_config_set_firmware(gisunlink_firmware_update *update) {
	esp_err_t err = ESP_OK;
	nvs_handle NvsHandle;

	if(update == NULL) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"conf is invalid memory address!!!");
		return false;
	}

	if((err = nvs_open(GISUNLINK_FIRMWARE_NVS_NAME, NVS_READWRITE, &NvsHandle)) != ESP_OK) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_open %s :%d",GISUNLINK_FIRMWARE_NVS_NAME,err);
		nvs_close(NvsHandle);
		return false;
	}	

	if (gisunlink_config_download_path(update->download,NvsHandle,WRITE_ACTION) == false) {
		nvs_close(NvsHandle);
		return false;
	}

	if(gisunlink_config_download_md5(update->download,NvsHandle,WRITE_ACTION) == false) { 
		nvs_close(NvsHandle);
		return false;
	}

	if(gisunlink_config_download_size(update->download,NvsHandle,WRITE_ACTION) == false) {
		nvs_close(NvsHandle);
		return false;
	}

	if(gisunlink_config_download_ver(update->download,NvsHandle,WRITE_ACTION) == false) {
		nvs_close(NvsHandle);
		return false;
	}

	if(gisunlink_config_download_over(update->download,NvsHandle,WRITE_ACTION) == false) { 
		nvs_close(NvsHandle);
		return false;
	}

	if((err  = nvs_set_u8(NvsHandle, GISUNLINK_TRANSFER_OVER_DATA_NVS, update->transfer_over)) != ESP_OK) {
		nvs_close(NvsHandle);
		gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_set %s :%d",GISUNLINK_TRANSFER_OVER_DATA_NVS,err);
		return false;
	}


	if((err = nvs_commit(NvsHandle)) != ESP_OK) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"set_wifi nvs_commit:%d",err);
		nvs_close(NvsHandle);
		return false;
	}

	gisunlink_print(GISUNLINK_PRINT_INFO,"save firmware md5:%s ver:%d transfer_over:%d",update->download->md5,update->download->ver,update->transfer_over);
	nvs_close(NvsHandle);
	return true;
}

bool gisunlink_config_get(uint8 conf_type,void *conf) {
	bool ret = false;
	switch(conf_type) {
		case POWERON_CONFIG:
			ret = gisunlink_config_get_general(conf_type,(uint8 *)conf,GISUNLINK_POWERON_NVS_NAME,GISUNLINK_POWERON_MODE_NVS);
			break;
		case MAC_CONFIG:
			ret = gisunlink_config_get_mac((uint8 *)conf);
			break;
		case WIFI_CONFIG:
			ret = gisunlink_config_get_wifi((gisunlink_wifi_conf *)conf);
			break;
		case SYSTEM_CONFIG:
			ret = gisunlink_config_get_system(conf);
			break;
		case AUTHORIZATION_CONFIG:
			ret = gisunlink_config_get_general(conf_type,(uint8 *)conf,GISUNLINK_AUTHORIZATION_NVS_NAME,GISUNLINK_AUTHORIZATION_MODE_NAME);
			break;
		case TOKEN_CONFIG:
			ret = gisunlink_config_get_token((gisunlink_token_conf *)conf); 
			break;
		case SN_CODE_CONFIG:
			ret = gisunlink_config_get_sn_code((gisunlink_sn_code_conf *)conf); 
			break;
		case DOWNLOAD:
			ret = gisunlink_config_get_download_task((gisunlink_firmware_download *)conf); 
			break;
		case FIRMWARE:
			ret = gisunlink_config_get_firmware((gisunlink_firmware_update *)conf); 
			break;
	}
	return ret;
}

bool gisunlink_config_set(uint8 conf_type,void *conf) {
	bool ret = false;
	switch(conf_type) {
		case POWERON_CONFIG:
			ret = gisunlink_config_set_general(conf_type,(uint8 *)conf,GISUNLINK_POWERON_NVS_NAME,GISUNLINK_POWERON_MODE_NVS);
			break;
		case WIFI_CONFIG:
			ret = gisunlink_config_set_wifi((gisunlink_wifi_conf *)conf);
			break;
		case SYSTEM_CONFIG:
			ret = gisunlink_config_set_system(conf);
			break;
		case AUTHORIZATION_CONFIG:
			ret = gisunlink_config_set_general(conf_type,(uint8 *)conf,GISUNLINK_AUTHORIZATION_NVS_NAME,GISUNLINK_AUTHORIZATION_MODE_NAME);
			break;
		case TOKEN_CONFIG:
			ret = gisunlink_config_set_token((gisunlink_token_conf *)conf); 
			break;
		case SN_CODE_CONFIG:
			ret = gisunlink_config_set_sn_code((gisunlink_sn_code_conf *)conf); 
			break;
		case DOWNLOAD:
			ret = gisunlink_config_set_download_task((gisunlink_firmware_download *)conf); 
			break;
		case FIRMWARE:
			ret = gisunlink_config_set_firmware((gisunlink_firmware_update *)conf); 
			break;
	}
	return ret;
}

char *gisunlink_get_mac_with_string(const char *prefix) {

	char *mac_string = NULL;
	gisunlink_config_get(MAC_CONFIG,&mac_addr);

	if(prefix == NULL) {
		asprintf(&mac_string, "%02x%02x%02x%02x%02x%02x",mac_addr[0],mac_addr[1],mac_addr[2],mac_addr[3],mac_addr[4],mac_addr[5]);
	} else {
		asprintf(&mac_string, "%s%02x%02x%02x%02x%02x%02x",prefix,mac_addr[0],mac_addr[1],mac_addr[2],mac_addr[3],mac_addr[4],mac_addr[5]);
	}
	return mac_string;
}

char *gisunlink_get_firmware_version(void) {
	uint32_t version = 0;
	char *version_string = NULL;

	esp_err_t err = ESP_OK;
	nvs_handle NvsHandle;

	if((err = nvs_open(GISUNLINK_FIRMWARE_NVS_NAME, NVS_READWRITE, &NvsHandle)) != ESP_OK) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"nvs_open %s :%d",GISUNLINK_FIRMWARE_NVS_NAME,err);
		nvs_close(NvsHandle);
		asprintf(&version_string, "%s","unknown");
		return version_string;
	}	

	if((err = nvs_get_u32(NvsHandle, GISUNLINK_VER_DATA_NVS, &version)) != ESP_OK) {
		asprintf(&version_string, "%s","unknown");
	} else {
		asprintf(&version_string, "%d",version);
	}

	return version_string;
}
