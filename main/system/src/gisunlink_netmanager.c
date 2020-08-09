/*
* _COPYRIGHT_
*
* File Name: gisunlink_netmanager.c
* System Environment: JOHAN-PC
* Created Time:2019-01-23
* Author: johan
* E-mail: johaness@qq.com
* Description: 
* rssi <= 0 && rssi >= -50  信号最好 
* rssi < -50 && rssi >= -70 信号较好 
* rssi < -70 && rssi >= -80 信号一般 
* rssi <-80  && rssi >= -100 信号较差  
* rssi没有在0～（-100)之间，表示无信号
*
*/

#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_system.h"
#include "tcpip_adapter.h"
#include "esp_smartconfig.h"
#include "freertos/event_groups.h"
#include "internal/esp_wifi_internal.h"

#include "gisunlink_queue.h"
#include "gisunlink_atomic.h"
#include "gisunlink_config.h"
#include "gisunlink_message.h"
//#include "gisunlink_airlink.h"
#include "gisunlink_netmanager.h"

typedef struct _gisunlink_netmanager_ctrl {
	bool connected;
	bool lost_connect;
	uint8 net_state;
	bool enter_pairing;
	int connected_bit;
	int airkiss_done_bit;
	EventGroupHandle_t wifi_event_group;
} gisunlink_netmanager_ctrl;

static gisunlink_wifi_conf *wifi_conf;
static gisunlink_netmanager_ctrl *netmanager = NULL;

static void gisunlink_netmanager_event_message_free(void *msg_data) {
	if(msg_data) {
		gisunlink_message *message = (gisunlink_message *)msg_data;
		gisunlink_netmanager_event *event = NULL;
		if(message->data_len && message->data) {
			event = (gisunlink_netmanager_event *)message->data;
			if(event->param && event->param_free) {
				event->param_free(event->param);
				event->param = NULL;
			}
			gisunlink_free(event);
			event = NULL;
		}
		gisunlink_free(message);
		message = NULL;
	}
}

static void gisunlink_netmanager_package_message(gisunlink_message *message,gisunlink_netmanager_event *event) {
	message->type = ASYNCMSG;
	message->src_id = GISUNLINK_NETMANAGER_MODULE; 
	message->dst_id = GISUNLINK_MAIN_MODULE;
	message->message_id = GISUNLINK_NETMANAGER_STATE_MSG;
	message->data_len = sizeof(gisunlink_netmanager_event);
	message->data = (void *)event;
	message->malloc = NULL;
	message->free = gisunlink_netmanager_event_message_free; 
	message->callback = NULL;
}

static void gisunlink_netmanager_post_state_message(GISUNLINK_NETMANAGER_WORK_STATE id,void *data,void (*data_free)(void *param)) {
	if(netmanager) {
		gisunlink_message message = {0}; 
		gisunlink_netmanager_event *event = (gisunlink_netmanager_event *)gisunlink_malloc(sizeof(gisunlink_netmanager_event));
		event->id = netmanager->net_state = id;
		event->param = data;
		event->param_free = data_free;
		gisunlink_netmanager_package_message(&message,event);
		gisunlink_message_push(&message);
	}
}

static void gisunlink_netmanager_smartconfig_callback(smartconfig_status_t status, void *pdata) {
	switch (status) {
		case SC_STATUS_WAIT:
			gisunlink_print(GISUNLINK_PRINT_WARN,"SC_STATUS_WAIT");
			break;
		case SC_STATUS_FIND_CHANNEL:
			gisunlink_print(GISUNLINK_PRINT_WARN,"SC_STATUS_FIND_CHANNEL");
			break;
		case SC_STATUS_GETTING_SSID_PSWD:
			gisunlink_print(GISUNLINK_PRINT_WARN,"SC_STATUS_GETTING_SSID_PSWD");
			break;
		case SC_STATUS_LINK: 
			{
				wifi_config_t *wifi_config = pdata;
				if(wifi_conf == NULL) {
					wifi_conf = (gisunlink_wifi_conf *)gisunlink_malloc(sizeof(gisunlink_wifi_conf));
				}
				if(wifi_conf) {
					memcpy(wifi_conf->ssid,wifi_config->sta.ssid,SSIDMAXLEN);
					memcpy(wifi_conf->pswd,wifi_config->sta.password,PSWDMAXLEN);
					wifi_conf->ssid_len = strlen((const char *)wifi_conf->ssid);
					wifi_conf->pswd_len = strlen((const char *)wifi_conf->pswd);
				}
				gisunlink_print(GISUNLINK_PRINT_WARN,"SC_STATUS_LINK");
				ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config) );
				ESP_ERROR_CHECK(esp_wifi_connect());
			}
			break;
		case SC_STATUS_LINK_OVER:
			if(pdata != NULL) {
				uint8_t phone_ip[4] = { 0 };
				memcpy(phone_ip, (uint8_t* )pdata, 4);
				gisunlink_print(GISUNLINK_PRINT_WARN,"Phone ip: %d.%d.%d.%d\n", phone_ip[0], phone_ip[1], phone_ip[2], phone_ip[3]);
			}
			xEventGroupSetBits(netmanager->wifi_event_group, netmanager->airkiss_done_bit);
			gisunlink_print(GISUNLINK_PRINT_WARN,"SC_STATUS_LINK_OVER");
			break;
		default:
			break;
	}
}

static void gisunlink_netmanager_smartconfig_task(void * parm) {
	EventBits_t uxBits;
//	if(true == gisunlink_airlink_is_run()) {
//		gisunlink_airlink_deinit();
//	}
	ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_AIRKISS));
	ESP_ERROR_CHECK(esp_smartconfig_start(gisunlink_netmanager_smartconfig_callback));
	while (1) {
		uxBits = xEventGroupWaitBits(netmanager->wifi_event_group, netmanager->connected_bit | netmanager->airkiss_done_bit, true, false, portMAX_DELAY); 
		if(uxBits & netmanager->airkiss_done_bit) {
			esp_smartconfig_stop();
			gisunlink_netmanager_post_state_message(GISUNLINK_NETMANAGER_EXI_CONFIG,NULL,NULL);
			netmanager->enter_pairing = false;
			break;
		}
	}
	gisunlink_destroy_task(NULL);
}

static esp_err_t esp_net_event_handler(void *ctx, system_event_t *event) {
	//gisunlink_print(GISUNLINK_PRINT_ERROR,"WIFI EventID:%d",(uint8)event->event_id);
	system_event_info_t *info = &event->event_info;
	switch((uint8)event->event_id) {
		case SYSTEM_EVENT_WIFI_READY:
			break;
		case SYSTEM_EVENT_SCAN_DONE:
			break;
		case SYSTEM_EVENT_STA_START:
			if(netmanager->enter_pairing) {
				gisunlink_netmanager_post_state_message(GISUNLINK_NETMANAGER_ENT_CONFIG,NULL,NULL);
				netmanager->connected = false;
				netmanager->lost_connect = false;
				gisunlink_create_task_with_Priority(gisunlink_netmanager_smartconfig_task,"smartconfig_task",NULL,4096,3);
			} else {
				gisunlink_netmanager_post_state_message(GISUNLINK_NETMANAGER_START,NULL,NULL);
				esp_wifi_connect();
			}
			break;
		case SYSTEM_EVENT_STA_STOP:
			break;
		case SYSTEM_EVENT_STA_CONNECTED:
			if(netmanager->enter_pairing) {
				uint8 PowerOn = WIFI_CONFIGURED_MODE;
				if(wifi_conf) {
					gisunlink_config_set(WIFI_CONFIG,wifi_conf);
					gisunlink_config_set(POWERON_CONFIG,&PowerOn);
					gisunlink_netmanager_post_state_message(GISUNLINK_NETMANAGER_SAVE_CONFIG,NULL,NULL);
					gisunlink_free(wifi_conf);
					wifi_conf = NULL;
				}
			}
			break;
		case SYSTEM_EVENT_STA_DISCONNECTED:
			//如果是已经连接上断开的，报断开事件
			if(netmanager->connected == true) {
				netmanager->connected = false;
				netmanager->lost_connect = true;
				gisunlink_netmanager_post_state_message(GISUNLINK_NETMANAGER_DISCONNECTED,NULL,NULL);
			} else {
				if(netmanager->lost_connect) {
					if(netmanager->net_state != GISUNLINK_NETMANAGER_RECONNECTING) {
						gisunlink_netmanager_post_state_message(GISUNLINK_NETMANAGER_RECONNECTING,NULL,NULL);
					}
				} else {
					if(netmanager->net_state != GISUNLINK_NETMANAGER_CONNECTING) {
						gisunlink_netmanager_post_state_message(GISUNLINK_NETMANAGER_CONNECTING,NULL,NULL);
					}
				}
			}

			if(info->disconnected.reason == WIFI_REASON_BASIC_RATE_NOT_SUPPORT) {
				gisunlink_print(GISUNLINK_PRINT_ERROR,"Switch to 802.11 bgn mode");
				/*Switch to 802.11 bgn mode */
				esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
			}
			esp_wifi_connect();
			xEventGroupClearBits(netmanager->wifi_event_group, netmanager->connected_bit);
			break;
		case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:
			break;
		case SYSTEM_EVENT_STA_GOT_IP:
			xEventGroupSetBits(netmanager->wifi_event_group,netmanager->connected_bit);
			netmanager->connected = true;
			netmanager->lost_connect = false;
			gisunlink_netmanager_post_state_message(GISUNLINK_NETMANAGER_CONNECTED,NULL,NULL);
			break;
		case SYSTEM_EVENT_STA_LOST_IP:
			if(netmanager->enter_pairing == false) {
				gisunlink_netmanager_post_state_message(GISUNLINK_NETMANAGER_DISCONNECTED,NULL,NULL);
			}
			esp_wifi_connect();
			break;
	}
	return ESP_OK;
}

static void gisunlink_netmanager_init_sta_wifi(void) {
	//初始化WIFI相关系统接口
	tcpip_adapter_init();
	netmanager->wifi_event_group = xEventGroupCreate();
	ESP_ERROR_CHECK(esp_event_loop_init(esp_net_event_handler, NULL));
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
}

static uint8 gisunlink_netmanager_message_callback(gisunlink_message *message) {
	if(message) {
		if(message->message_id == GISUNLINK_NETMANAGER_PAIRING_MSG) {
			gisunlink_print(GISUNLINK_PRINT_ERROR,"GISUNLINK_NETMANAGER_PAIRING");
			ESP_ERROR_CHECK(esp_wifi_disconnect());
			ESP_ERROR_CHECK(esp_wifi_stop());
			ESP_ERROR_CHECK(esp_wifi_start());
		}
	}
	return true;
}

void gisunlink_netmanager_init(void) {
	uint8 poweron = WIFI_UNCONFIGURED_MODE;
	gisunlink_wifi_conf sys_wifi_conf;

	if(netmanager == NULL) {
		if((netmanager = (gisunlink_netmanager_ctrl *)gisunlink_malloc(sizeof(gisunlink_netmanager_ctrl)))) {
			gisunlink_message_register(GISUNLINK_NETMANAGER_MODULE,gisunlink_netmanager_message_callback);
			gisunlink_netmanager_post_state_message(GISUNLINK_NETMANAGER_IDLE,NULL,NULL);
			netmanager->enter_pairing = false;
			netmanager->connected = false;
			netmanager->lost_connect = false;
			netmanager->connected_bit = BIT0;
			netmanager->airkiss_done_bit = BIT1;
			gisunlink_netmanager_init_sta_wifi();
			gisunlink_config_get(POWERON_CONFIG,&poweron);

			//设备设置过网络
			if(WIFI_CONFIGURED_MODE == poweron) {
				netmanager->enter_pairing = false;
				wifi_config_t wifi_config;
				gisunlink_config_get(WIFI_CONFIG,&sys_wifi_conf);

				memset(&wifi_config,0x00,sizeof(wifi_config_t));
				memcpy(wifi_config.sta.ssid, sys_wifi_conf.ssid, sys_wifi_conf.ssid_len);
				memcpy(wifi_config.sta.password, sys_wifi_conf.pswd, sys_wifi_conf.pswd_len);
				ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
				gisunlink_print(GISUNLINK_PRINT_INFO,"statr connect Wifi SSID:%s PSWD:%s",wifi_config.sta.ssid, wifi_config.sta.password);
			} else {
				//不直接进入配网模式
				//netmanager->enter_pairing = true;
				wifi_config_t wifi_config = {
					.sta = {
						.ssid = "ylpower",
						.password = "yununion2020!" 
					},
				};
				ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
				gisunlink_print(GISUNLINK_PRINT_INFO,"statr connect Wifi SSID:%s PSWD:%s",wifi_config.sta.ssid, wifi_config.sta.password);
			}
			ESP_ERROR_CHECK(esp_wifi_start());
		}
	}
	return;
}

uint8 gisunlink_netmanager_get_state(void) {
	if(netmanager == NULL) {
		return GISUNLINK_NETMANAGER_UNKNOWN;
	}
	return netmanager->net_state;
}

bool gisunlink_netmanager_is_enter_pairing(void) {
	if(netmanager) {
		return netmanager->enter_pairing;
	}
	return false;
}

void gisunlink_netmanager_enter_pairing(uint8 module_id) {
	if(netmanager) {
		netmanager->enter_pairing = true;
		gisunlink_message message = {0}; 
		message.type = ASYNCMSG;
		message.src_id = module_id;
		message.dst_id = GISUNLINK_NETMANAGER_MODULE; 
		message.message_id = GISUNLINK_NETMANAGER_PAIRING_MSG;
		gisunlink_message_push(&message);
	}
}

int8 gisunlink_netmanager_get_ap_rssi(void) {
	int8 rssi = 0;
	if(netmanager) {
		rssi = esp_wifi_get_ap_rssi();
	}
	return rssi;
}

bool gisunlink_netmanager_is_connected_ap(void) {
	if(netmanager) {
		if(netmanager->enter_pairing != true && netmanager->connected) {
			return true;
		}
	} 
	return false;
}
