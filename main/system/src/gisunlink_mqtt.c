/*
* _COPYRIGHT_
*
* File Name: gisunlink_mqtt.c
* System Environment: JOHAN-PC
* Created Time:2019-04-07
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_http_client.h"
#include "lwip/inet.h"

#include "cJSON.h"
#include "mqtt_client.h"

#include "gisunlink.h"
#include "gisunlink_mqtt.h"
#include "gisunlink_config.h"
#include "gisunlink_queue.h"
#include "gisunlink_atomic.h"
#include "gisunlink_message.h"
#include "gisunlink_netmanager.h"

#define MQTT_INFO_SERV "http://power.fuxiangjf.com/device/mqtt_connect_info"

#define CONFIG_MQTT_PAYLOAD_BUFFER 1460
#define GISUNLINK_MAX_SEND_QUEUE 20
#define GISUNLINK_MAX_TRY_NUM 3
#define GISUNLINK_MAX_QUEUE_NUM 10
#define GISUNLINK_MAX_WAIT_QUEUE_NUM 64
#define GISUNLINK_FIRST_WAIT_TIME 3000
#define GISUNLINK_SECOND_WAIT_TIME 5000
#define GISUNLINK_RETRY_NUM		3	
#define GISUNLINK_GET_TIMEOUT	8000	

#define GISUNLINK_WAIT_TASK_DELAY 10

#define MAX_RESPOND_SIZE 1024

typedef struct _gisunlink_mqtt_ctrl {
	esp_mqtt_client_handle_t client;
	bool is_connecting;
	bool is_connected;
	uint32 httpcode;
	uint32 requestConut;
	uint32 errorString;
	char *broker;
	char *username;
	char *password;
	int port;
	char *clientID;
	char *FirmwareVersion; 
	char *DeviceHWSn;
	void *queue;
	void *wait_ack_queue;
	void *wait_ack_sem;
	void *connect_sem;
	void *message_sem;
	GISUNLINK_MQTT_CONNECT *connectCb;
	GISUNLINK_MQTT_MESSAGE *messageCb;
} gisunlink_mqtt_ctrl;

typedef enum {
    GISUNLINK_TOPIC_SUB,
    GISUNLINK_TOPIC_PUB,
} MQTT_TOPIC_TYPE;

typedef struct _gisunlink_mqtt_packet {
	MQTT_TOPIC_TYPE type;
	uint8 qos;
	char *topic;
	char *payload;
	uint16 payloadlen;
	uint8 ackstatus;
	uint8 retry;
	uint32 id;
	uint32 start_ticks;
	uint32 timeout_ticks;
} gisunlink_mqtt_packet;

gisunlink_mqtt_ctrl *gisunlink_mqtt = NULL;

static int gisunlink_matching_package(void *src, void *item) {
	uint32 *src_data = (uint32 *)src;
	gisunlink_mqtt_packet *data_item = (gisunlink_mqtt_packet *)item;
	if(*src_data == data_item->id) {
		return 0;
	}
	return 1;
}

static uint32 gisunlink_mqtt_get_resp_message_id(char *data,int data_len) {
	uint32 id = 0;

	if(data && data_len) {
		struct cJSON_Hooks js_hook = {gisunlink_malloc, &gisunlink_free};
		cJSON_InitHooks(&js_hook);
		cJSON *pJson = cJSON_Parse(data);
		cJSON *item = NULL;
		if(pJson) {
			if((item = cJSON_GetObjectItem(pJson, "req_id"))) {
				if(item->type == cJSON_Number) {
					id = item->valueint;
				}
			}
			cJSON_Delete(pJson);
			pJson = NULL;
		}
	}

	return id;
}

static void gisunlink_mqtt_message_arrived(gisunlink_mqtt_ctrl *mqtt,char *topic,int topic_len,char *data,int data_len) {
	const char *resp_act = "resp";
	const char *update_ver = "update_ver";
	const char *transfer = "transfer";
	const char *device_info = "device_info";

	if(mqtt == NULL) {
		return;
	}

	if(mqtt && mqtt->messageCb) {
		gisunlink_mqtt_message *message = NULL;
		if(data && data_len) {
			struct cJSON_Hooks js_hook = {gisunlink_malloc, &gisunlink_free};
			cJSON_InitHooks(&js_hook);
			cJSON *pJson = cJSON_Parse(data);
			cJSON *item = NULL;
			if(pJson) {
				message = (gisunlink_mqtt_message *)gisunlink_malloc(sizeof(gisunlink_mqtt_message));
				if(message) {
					if((item = cJSON_GetObjectItem(pJson, "id"))) {
						if(item->type == cJSON_Number) {
							message->id = item->valueint;
						}
					}
					if((item = cJSON_GetObjectItem(pJson, "act"))) {
						if(item->type == cJSON_String) {
							asprintf(&message->act, "%s",item->valuestring);
							message->act_len = strlen(message->act) - 1;
						}
					}
					if((item = cJSON_GetObjectItem(pJson, "behavior"))) {
						if(item->type == cJSON_Number) {
							message->behavior = item->valueint;
						}
					}

					if((item = cJSON_GetObjectItem(pJson, "data"))) {
						if(item->type == cJSON_String || item->type == cJSON_Raw) {
							asprintf(&message->data, "%s",item->valuestring);
						} else if(item->type == cJSON_Array || item->type == cJSON_Object) {
							message->data = cJSON_PrintUnformatted(item);
						}
						if(message->data) {
							message->data_len = strlen(message->data) - 1;
						} else {
							message->data_len = 0;
						}
					}
					///////////copy topic///////////
					if (topic && topic_len) {
						message->topic_len = topic_len;
						message->topic = (char *)gisunlink_malloc(message->topic_len);
						memcpy(message->topic,topic,message->topic_len);
					} 
				}
				cJSON_Delete(pJson);
				pJson = NULL;
			} 
		}

		if(message) {
			if(memcmp(message->act,resp_act,message->act_len) == 0) {
				uint32 id = gisunlink_mqtt_get_resp_message_id(message->data,message->data_len);
				if(id) {
					gisunlink_mqtt_packet *packet = gisunlink_queue_pop_cmp(mqtt->wait_ack_queue, (void *)&id, gisunlink_matching_package);
					if(packet) {
						gisunlink_free(packet->topic);
						gisunlink_free(packet->payload);
						gisunlink_free(packet);
					}
				}
			} else {
				if(memcmp(message->act,update_ver,message->act_len) == 0) {
					message->act_type = UPDATE_VER_ACT;
				} else if(memcmp(message->act,transfer,message->act_len) == 0) {
					message->act_type = TRANSFER_ACT;
				} else if(memcmp(message->act,device_info,message->act_len) == 0) {
					message->act_type = DEVICE_INFO_ACT;
				}
				mqtt->messageCb(message);
			}
			gisunlink_mqtt_free_message(message);
		}
	}
}

static bool gisunlink_mqtt_thread_subscribe(gisunlink_mqtt_ctrl *mqtt,gisunlink_mqtt_packet *packet) {
	int rc = 0;
	bool ret = true;
	if(mqtt && mqtt->client && mqtt->is_connected) {
		if((rc = esp_mqtt_client_subscribe(mqtt->client,packet->topic,packet->qos)) < 0) {
			gisunlink_print(GISUNLINK_PRINT_ERROR,"subscribe topic:%s failed:%d",packet->topic,rc);
			ret = false;
		}
	}
	return ret;
}

static bool gisunlink_mqtt_thread_publish(gisunlink_mqtt_ctrl *mqtt,gisunlink_mqtt_packet *packet) {
	int rc = 0;
	bool ret = true;
	if(mqtt && mqtt->client && mqtt->is_connected) {
		if((rc = esp_mqtt_client_publish(mqtt->client, packet->topic, packet->payload, strlen(packet->payload), packet->qos, 0)) < 0) {
			gisunlink_print(GISUNLINK_PRINT_ERROR,"publish topic:%s failed:%d",packet->topic,rc);
			ret = false;
		} 
	}
	return ret;
}

static void gisunlink_http_message(int status_code, const char *data, int data_len, void *param) {
	if(status_code == 200) {
		const char *jsonData = data;
		struct cJSON_Hooks js_hook = {gisunlink_malloc, &gisunlink_free};
		cJSON_InitHooks(&js_hook);
		cJSON *pJson = cJSON_Parse(jsonData);
		if(pJson) { 
			cJSON *item = NULL;
			if((item = cJSON_GetObjectItem(pJson, "code"))) {
				if(item->type == cJSON_True || item->type == cJSON_Number) {	
					gisunlink_mqtt->httpcode = item->valueint;
					cJSON *data_item = NULL;
					if((data_item = cJSON_GetObjectItem(pJson, "data"))) {
						if(data_item) {
							if((item = cJSON_GetObjectItem(data_item, "mqtt_host"))) {
								if(item->type == cJSON_String) {
									asprintf(&gisunlink_mqtt->broker, "%s",item->valuestring);
								}
							}
							if((item = cJSON_GetObjectItem(data_item, "mqtt_port"))) {
								if(item->type == cJSON_Number) {
									gisunlink_mqtt->port = item->valueint;
								}
							}
							if((item = cJSON_GetObjectItem(data_item, "username"))) {
								if(item->type == cJSON_String) {
									asprintf(&gisunlink_mqtt->username, "%s",item->valuestring);
								}
							}
							if((item = cJSON_GetObjectItem(data_item, "password"))) {
								if(item->type == cJSON_String) {
									asprintf(&gisunlink_mqtt->password, "%s",item->valuestring);
								}
							}
						}
					}
				}
			}
		} 
		cJSON_Delete(pJson);
		pJson = NULL;
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
				gisunlink_http_message(esp_http_client_get_status_code(evt->client),evt->data,evt->data_len,evt->user_data);
			}
			break;
	}
	return ESP_OK;
}

static bool gisunlink_mqtt_get_server(const char *host, const char *clientID, uint32 timeout_ms) {
	char *post_data = NULL;
	char *post_url = NULL;

	struct timeval tv;
	gettimeofday(&tv, NULL);

	asprintf(&post_url,"%s?%s&timeStamp=%ld&requestCount=%ld&errorString=%ld",host,gisunlink_mqtt->clientID,tv.tv_sec,++gisunlink_mqtt->requestConut,gisunlink_mqtt->errorString);

	esp_http_client_config_t config = {
		.url = post_url,
		.event_handler = _http_event_handler,
		.user_data = gisunlink_mqtt,
		.timeout_ms = timeout_ms,
	};

	int post_len = asprintf(&post_data,"{\"flag_number\":\"%s\",\"version\":\"%s\",\"device_sn\":\"%s\"}",clientID,gisunlink_mqtt->FirmwareVersion,gisunlink_mqtt->DeviceHWSn);
	esp_http_client_handle_t client = esp_http_client_init(&config);
	esp_http_client_set_method(client, HTTP_METHOD_POST);
	esp_http_client_set_header(client,"Content-Type", "application/json");
	esp_http_client_set_header(client,"User-Agent", "gisunLink_iot");
	esp_http_client_set_post_field(client, post_data,post_len); 

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

	if(err == ESP_OK) {
		if(esp_http_client_get_status_code(client) == 200 && esp_http_client_get_content_length(client)) {
			gisunlink_mqtt->requestConut = 0;
			gisunlink_mqtt->errorString = 0;
		} else {
			gisunlink_mqtt->errorString = esp_http_client_get_status_code(client);
			gisunlink_print(GISUNLINK_PRINT_WARN,"HTTP POST Status = %d, content_length = %d", esp_http_client_get_status_code(client), esp_http_client_get_content_length(client));
		}
	} else {
		gisunlink_mqtt->errorString = err;
		gisunlink_print(GISUNLINK_PRINT_ERROR,"HTTP POST request failed: %d", err);
	}

	esp_http_client_cleanup(client);

	if(gisunlink_mqtt->httpcode != 20000) {
		return false;
	}

	if(gisunlink_mqtt->broker && gisunlink_mqtt->port && gisunlink_mqtt->username && gisunlink_mqtt->password) {
		gisunlink_print(GISUNLINK_PRINT_WARN,"broker:%s port:%d username:%s password:%s",gisunlink_mqtt->broker,gisunlink_mqtt->port,gisunlink_mqtt->username,gisunlink_mqtt->password);
		return true;
	} else {
		return false;
	}

	return true;
}

static void gisunlink_mqtt_info_reset(gisunlink_mqtt_ctrl *mqtt) {
	if(mqtt) {
		mqtt->httpcode = 0;
		mqtt->port = 0;
		if(mqtt->broker) {
			gisunlink_free(mqtt->broker);
			mqtt->broker = NULL;
		}
		if(mqtt->username) {
			gisunlink_free(mqtt->username);
			mqtt->username = NULL;
		}
		if(mqtt->password) {
			gisunlink_free(mqtt->password);
			mqtt->password = NULL;
		}
	}
}

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event) {
	//	esp_mqtt_client_handle_t client = event->client;
	gisunlink_mqtt_ctrl *mqtt = event->user_context;
	switch (event->event_id) {
		case MQTT_EVENT_CONNECTED:
			gisunlink_mqtt->is_connected = true;
			mqtt->connectCb(MQTT_CONNECT_SUCCEED);
			break;
		case MQTT_EVENT_DISCONNECTED:
			gisunlink_mqtt->is_connected = false;
			mqtt->connectCb(MQTT_CONNECT_FAILED);
			gisunlink_put_sem(mqtt->message_sem);
			break;
		case MQTT_EVENT_SUBSCRIBED:
		case MQTT_EVENT_UNSUBSCRIBED:
		case MQTT_EVENT_PUBLISHED:
			//gisunlink_print(GISUNLINK_PRINT_WARN , "mqtt event_id = %d, msg_id = %d", event->event_id,event->msg_id);
			break;
		case MQTT_EVENT_DATA:
			//gisunlink_print(GISUNLINK_PRINT_WARN,"TOPIC=%.*s\r\n", event->topic_len, event->topic);
			gisunlink_print(GISUNLINK_PRINT_WARN,"DATA=%.*s\r\n", event->data_len, event->data);
			gisunlink_mqtt_message_arrived(mqtt,event->topic,event->topic_len,event->data,event->data_len);
			break;
		case MQTT_EVENT_ERROR:
			gisunlink_print(GISUNLINK_PRINT_ERROR, "MQTT_EVENT_ERROR");
			break;
	}
	return ESP_OK;
}

static void gisunlink_mqtt_connect_server(gisunlink_mqtt_ctrl *mqtt) {
	esp_mqtt_client_config_t mqtt_cfg = {
		.event_handle = mqtt_event_handler,
		.user_context = (void *)mqtt,
		.task_stack = 4096,
		.keepalive = 30,
		.buffer_size = 512,
	};

	if(mqtt == NULL) {
		return;
	}

	if(gisunlink_netmanager_is_enter_pairing()) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"netmanager is pairing exit MQTT");
		mqtt->is_connecting = false;
		return;
	}

	while(1) {
		gisunlink_mqtt_info_reset(mqtt);
		if(gisunlink_mqtt_get_server(MQTT_INFO_SERV,mqtt->clientID,GISUNLINK_GET_TIMEOUT)) {
			break;
		} 
		gisunlink_task_delay(10000 / portTICK_PERIOD_MS);
	}
	mqtt_cfg.host = mqtt->broker;
	mqtt_cfg.port = mqtt->port;
	mqtt_cfg.client_id = mqtt->clientID;
	mqtt_cfg.username = mqtt->username;
	mqtt_cfg.password = mqtt->password;

	if(mqtt->client == NULL) {
		mqtt->client = esp_mqtt_client_init(&mqtt_cfg);
		esp_mqtt_client_start(mqtt->client);
		gisunlink_print(GISUNLINK_PRINT_ERROR,"gisunlink_mqtt_connect_server");
	} 
	gisunlink_mqtt_info_reset(mqtt);
}

static void gisunlink_mqtt_thread(void *param) {
	gisunlink_mqtt_packet *packet = NULL;
	gisunlink_mqtt_ctrl *mqtt = (gisunlink_mqtt_ctrl *)param;
	while(1) {
		gisunlink_get_sem(mqtt->connect_sem);
		gisunlink_mqtt_connect_server(mqtt);
		while(1) {
			gisunlink_get_sem(mqtt->message_sem);
			if(mqtt->is_connected == false) {
				//进入了网络配对模式
				if(gisunlink_netmanager_is_enter_pairing()) {
					gisunlink_print(GISUNLINK_PRINT_ERROR,"netmanager is pairing exit MQ thread");
					esp_mqtt_client_destroy(mqtt->client);
					mqtt->client = NULL;
					mqtt->is_connecting = false;
					break;
				} else {
					if(mqtt->client) {
						gisunlink_mqtt_info_reset(mqtt);
						if(gisunlink_mqtt_get_server(MQTT_INFO_SERV,mqtt->clientID,GISUNLINK_GET_TIMEOUT)) {
							//	esp_mqtt_client_reset_username(mqtt->client,mqtt->username);
							//	esp_mqtt_client_reset_password(mqtt->client,mqtt->password);
							//	esp_mqtt_client_reset_host(mqtt->client,mqtt->broker);
							//	esp_mqtt_client_reset_port(mqtt->client,mqtt->port);
							gisunlink_mqtt_info_reset(mqtt);
						}
					}
				}
			}
			while(gisunlink_queue_not_empty(mqtt->queue)) {
				bool msg_err = true;
				bool clear_packet = true;
				if((packet = (gisunlink_mqtt_packet *)gisunlink_queue_pop(mqtt->queue))) {
					switch(packet->type) {
						case GISUNLINK_TOPIC_SUB:
							msg_err = gisunlink_mqtt_thread_subscribe(mqtt,packet);
							break;
						case GISUNLINK_TOPIC_PUB:
							msg_err = gisunlink_mqtt_thread_publish(mqtt,packet);
							break;
					}

					if(msg_err == false) {
						if(gisunlink_mqtt_isconnected() && packet->ackstatus == MQTT_PUBLISH_NEEDACK) {
							//如果发送错误，重新压回队列头						
							//gisunlink_print(GISUNLINK_PRINT_ERROR,"MQTT packet handle error repush stack");
							//gisunlink_queue_push_head(mqtt->queue,packet);
							if(gisunlink_queue_push(mqtt->wait_ack_queue, packet)) {
								clear_packet = false;
								gisunlink_put_sem(mqtt->wait_ack_sem);
								packet->start_ticks = gisunlink_get_tick_count();
								packet->timeout_ticks = GISUNLINK_FIRST_WAIT_TIME;
								packet->retry--;
							}
						}
					} 

					if(clear_packet == true) {
						gisunlink_free(packet->topic);
						gisunlink_free(packet->payload);
						gisunlink_free(packet);
					}
				}
			}
		}
		gisunlink_print(GISUNLINK_PRINT_WARN,"MQTT thread reset");
	}
	gisunlink_destroy_task(NULL);
}

static int gisunlink_matching_package_timeout(void *src, void *item) {
	uint32 *src_data = (uint32 *)src;
	gisunlink_mqtt_packet *packet = (gisunlink_mqtt_packet *)item;
	///aws_iot/port/timer.c
	if((*src_data - packet->start_ticks) >= packet->timeout_ticks/portTICK_PERIOD_MS) {
		return 0;
	} else {
		return 1;
	}
}

static void gisunlink_mqtt_wait_ack_thread(void *param) {
	gisunlink_mqtt_packet *packet = NULL;
	gisunlink_mqtt_ctrl *mqtt = (gisunlink_mqtt_ctrl *)param;
	while(1) {
		gisunlink_get_sem(mqtt->wait_ack_sem);
		while(gisunlink_queue_not_empty(mqtt->wait_ack_queue)) {
			uint16 wait_responds = gisunlink_queue_items(mqtt->wait_ack_queue);
			gisunlink_queue_reset(mqtt->wait_ack_queue);
			for(int i = 0; i < wait_responds; i++) {
				uint32 now_time = gisunlink_get_tick_count(); 
				gisunlink_queue_lock(mqtt->wait_ack_queue);
				packet = gisunlink_queue_get_next(mqtt->wait_ack_queue, (void *)&now_time, gisunlink_matching_package_timeout);
				if(packet && packet->retry) {
					gisunlink_print(GISUNLINK_PRINT_ERROR,"MQTT_PUBLISH_NEEDACK publish_id:%d retry:%d",packet->id,packet->retry);
					packet->start_ticks = gisunlink_get_tick_count();
					packet->timeout_ticks = GISUNLINK_SECOND_WAIT_TIME;
					gisunlink_mqtt_thread_publish(mqtt,packet);
					packet->retry--;
				}
				gisunlink_queue_unlock(mqtt->wait_ack_queue);
				if(packet && packet->retry == 0) {  //超时了 则从等待队列弹出
					gisunlink_print(GISUNLINK_PRINT_ERROR,"free MQTT_PUBLISH_NEEDACK publish_id:%d retry:%d",packet->id,packet->retry);
					packet = gisunlink_queue_pop_item(mqtt->wait_ack_queue,packet);
					if(packet) {
						gisunlink_free(packet->topic);
						gisunlink_free(packet->payload);
						gisunlink_free(packet);
					}
				}
			}
			gisunlink_task_delay(GISUNLINK_WAIT_TASK_DELAY / portTICK_PERIOD_MS);
		}
	}
	gisunlink_destroy_task(NULL);
}

void gisunlink_mqtt_init(char *DeviceHWSn_addr,char *FirmwareVersion) {
	if(gisunlink_mqtt) {
		gisunlink_mqtt->DeviceHWSn = DeviceHWSn_addr;
		gisunlink_mqtt->FirmwareVersion = FirmwareVersion;
		return;
	}

	if((gisunlink_mqtt = (gisunlink_mqtt_ctrl *)gisunlink_malloc(sizeof(gisunlink_mqtt_ctrl)))) {
		gisunlink_mqtt->is_connecting = false;
		gisunlink_mqtt->is_connected = false;
		gisunlink_mqtt->DeviceHWSn = DeviceHWSn_addr; //引用设备串号地址
		gisunlink_mqtt->FirmwareVersion = FirmwareVersion;
		gisunlink_mqtt->queue = gisunlink_queue_init(GISUNLINK_MAX_QUEUE_NUM);
		gisunlink_mqtt->wait_ack_queue = gisunlink_queue_init(GISUNLINK_MAX_WAIT_QUEUE_NUM);
		gisunlink_mqtt->wait_ack_sem = gisunlink_create_sem(GISUNLINK_MAX_WAIT_QUEUE_NUM,0);
		gisunlink_mqtt->connect_sem = gisunlink_create_sem(GISUNLINK_MAX_QUEUE_NUM,0);
		gisunlink_mqtt->message_sem = gisunlink_create_sem(GISUNLINK_MAX_QUEUE_NUM,0);
		gisunlink_mqtt->clientID = gisunlink_get_mac_with_string("gsl_");
		gisunlink_mqtt->requestConut = 0;	
		gisunlink_create_task_with_Priority(gisunlink_mqtt_thread, "mqtt_opt", gisunlink_mqtt, 2048,8); //3072 // 2048 // 2650
		gisunlink_create_task_with_Priority(gisunlink_mqtt_wait_ack_thread, "mqtt_wait", gisunlink_mqtt,1536,7); //3072 // 2048 // 2650
	}
}

void gisunlink_mqtt_connect(GISUNLINK_MQTT_CONNECT *connectCb,GISUNLINK_MQTT_MESSAGE *messageCb) {
	if(gisunlink_mqtt && messageCb && gisunlink_mqtt->is_connecting == false) {
		gisunlink_mqtt->is_connecting = true;
		gisunlink_mqtt->messageCb = messageCb;
		gisunlink_mqtt->connectCb = connectCb;
		gisunlink_put_sem(gisunlink_mqtt->connect_sem);
	} 
}

bool gisunlink_mqtt_isconnected(void) {
	if(gisunlink_mqtt && gisunlink_mqtt->client && gisunlink_mqtt->is_connected == true) {
		return true;
	}
	return false;
} 

bool gisunlink_mqtt_subscribe(const char *topic,uint8 qos) {
	if(gisunlink_mqtt && gisunlink_mqtt->is_connected) {
		gisunlink_mqtt_packet *packet = (gisunlink_mqtt_packet *)gisunlink_malloc(sizeof(gisunlink_mqtt_packet));
		packet->type = GISUNLINK_TOPIC_SUB;
		packet->ackstatus = MQTT_PUBLISH_NOACK;
		packet->qos = qos;
		packet->retry = GISUNLINK_RETRY_NUM;
		asprintf(&packet->topic,"%s",topic);
		if(gisunlink_queue_push(gisunlink_mqtt->queue, packet)) {
			gisunlink_put_sem(gisunlink_mqtt->message_sem);
		} else {
			gisunlink_free(packet->topic);
			gisunlink_free(packet);
			return false;
		}	
		return true;
	}
	return false;
} 

bool gisunlink_mqtt_publish(char *topic,const char *payload,uint8 qos, uint32 publish_id, bool ackstatus) {
	if(gisunlink_mqtt && gisunlink_mqtt->is_connected) {
		gisunlink_mqtt_packet *packet = (gisunlink_mqtt_packet *)gisunlink_malloc(sizeof(gisunlink_mqtt_packet));
		packet->type = GISUNLINK_TOPIC_PUB;
		packet->ackstatus = ackstatus;
		packet->qos = qos;
		packet->id = publish_id;
		packet->retry = GISUNLINK_RETRY_NUM;
		asprintf(&packet->topic,"%s",topic);
		asprintf(&packet->payload,"%s",payload);
		if(gisunlink_queue_push(gisunlink_mqtt->queue, packet)) {
			gisunlink_put_sem(gisunlink_mqtt->message_sem);
		} else {
			gisunlink_free(packet->topic);
			gisunlink_free(packet->payload);
			gisunlink_free(packet);
			return false;
		}	
		return true;
	}
	return false;
}

char *gisunlink_mqtt_clientid(void) {
	if(gisunlink_mqtt) {
		return gisunlink_mqtt->clientID;
	}
	return NULL;
}

void gisunlink_mqtt_free_message(gisunlink_mqtt_message *message) {
	if(message) {
		if(message->topic) {
			gisunlink_free(message->topic);
			message->topic = NULL;
		}
		if(message->act) {
			gisunlink_free(message->act);
			message->act = NULL;
		}
		if(message->data) {
			gisunlink_free(message->data);
			message->data = NULL;
		}

		gisunlink_free(message);
		message = NULL;
	}
	return;
}
