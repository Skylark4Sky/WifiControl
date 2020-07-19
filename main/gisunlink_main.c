/*
* _COPYRIGHT_
*
* File Name: gisunlink_main.c
* System Environment: JOHAN-PC
* Created Time:2019-01-22
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#include "freertos/FreeRTOS.h"
#include "mbedtls/base64.h"

#include <stdio.h>
#include "esp_libc.h"
#include "esp_system.h"
#include "esp_log.h"

#include "gisunlink.h"
#include "gisunlink_mqtt.h"
#include "gisunlink_atomic.h"
#include "gisunlink_config.h"
#include "gisunlink_system.h"
#include "gisunlink_message.h"
#include "gisunlink_peripheral.h"
#include "gisunlink_updatefirmware.h"

#define FIRMWARE_SEND_RETRY 5
#define STM32_UNIQUE_ID_SIZE 12

static gisunlink_system_ctrl *gisunlink_system = NULL;

static gisunlink_firmware_update_hook update_hook = {
	.update = false,
	.version = 0,
	.file_size = 0,
	.send_size = 0,
};

//等待获取设备号
static bool waitHWSn = false;

static bool update_retry = false;
static uint16_t update_retry_tick = 0;

static char DeviceHWSn[25] = {0};

#define RESPOND_FORMAT "{\"id\":%lu,\"act\":\"%s\",\"behavior\":%d,\"data\":{\"req_id\":%ld,\"success\":%s,\"msg\":\"%s\"}}"
#define PUBLISH_FORMAT "{\"id\":%lu,\"act\":\"%s\",\"behavior\":%d,\"data\":\"%s\",\"ctime\":%ld}"

static void gisunlink_mqtt_message_respond(const char *act,uint8 behavior,uint32 req_id,bool result,const char *msg) {
	char *resp_buf = NULL;
	uint32 sec = gisunlink_system->getSec();
	uint32 usec = gisunlink_system->getUsec();
	uint32 id = ((sec%100000)*10000) + usec;
	char *result_str = result ? "true" : "false";
	asprintf(&resp_buf,RESPOND_FORMAT,id,act,behavior,req_id,result_str,msg);
	gisunlink_mqtt_publish("/point_switch_resp",resp_buf,0,id,MQTT_PUBLISH_NOACK);

	gisunlink_print(GISUNLINK_PRINT_WARN,"%s id:%d msg:%s",act,req_id,msg);
	if(resp_buf) {
		gisunlink_free(resp_buf);
		resp_buf = NULL;
	}
}

static void gisunlink_mqtt_task_send_respond(gisunlink_respond_message *message) {
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
	gisunlink_mqtt_message_respond("transfer",message->behavior,message->heading,send_status,msg);
}

static void gisunlink_mqtt_messageCb(gisunlink_mqtt_message *message) {
	const char *update_ver_act = "update_ver";
	const char *transfer_act = "transfer";
	if(message && message->data && message->data_len) {
		if(memcmp(message->act,transfer_act,message->act_len) == 0) {
			if(update_hook.update == true) {
				char *msg = NULL;
				asprintf(&msg,"system upgrade! ver:%ld file_size:%ld progress_size: %ld",update_hook.version,update_hook.file_size,update_hook.send_size);
				gisunlink_mqtt_message_respond("transfer",message->behavior,message->id,false,msg);
				if(msg) {
					gisunlink_free(msg);
					msg = NULL;
				}
				return;
			}

			//长度对齐
			uint16 data_len = message->data_len + 4 - ((message->data_len%4));
			uint8 *uart_buf = (uint8 *)gisunlink_malloc(data_len + 1); 

			if(uart_buf) {
				int ret = 0;
				size_t dec_len = 0;
				uint8 *dec_buf = uart_buf + 1;
				if((ret = mbedtls_base64_decode(dec_buf, data_len, &dec_len, (const uint8 *)message->data, data_len)) == 0) {
					gisunlink_print(GISUNLINK_PRINT_WARN,"message->data_len:%d :%s",message->data_len%4,message->data);
					uart_buf[0] = message->behavior;
					gisunlink_peripheral_message uart_message = {
						.behavior = message->behavior,
						.heading = message->id,
						.cmd = GISUNLINK_TASK_CONTROL,
						.data = uart_buf,
						.data_len = dec_len + 1,
						.respond = UART_RESPOND,
						.respondCb = gisunlink_mqtt_task_send_respond,
					};
					gisunlink_peripheral_send_message(&uart_message);
				} else {
					gisunlink_print(GISUNLINK_PRINT_ERROR,"mbedtls_base64_decode failed %d",ret);
				}
				gisunlink_free(uart_buf); uart_buf = NULL;
			}
		}
		if(memcmp(message->act,update_ver_act,message->act_len) == 0) {
			gisunlink_updatefirmware_download_new_firmware(message->data,message->data_len);
		}
	}
}

static void gisunlink_mqtt_message_publish(const char *act,void *message) {
	gisunlink_uart_event *uart = (gisunlink_uart_event *)message;

	if(uart == NULL) {
		return;
	}

	if(uart->cmd == GISUNLINK_TASK_CONTROL && gisunlink_mqtt_isconnected()) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"gisunlink_mqtt_message_publish cmd:0x%02x data_len:%d",uart->cmd,uart->data_len);
		char *publish_topic = NULL;
		char *publish_data = NULL;
		uint8 *base64Str = (uint8 *)" ";
		uint8 behavior = (uint8)(*uart->data);  //第一个字节为行为
		uint8 publish_ack = (uint8)(*(uart->data + 1));//第二个字节为是否需要等待服务器确认
		uint32 sec = gisunlink_system->getSec();
		uint32 usec = gisunlink_system->getUsec();
		uint32 id = ((sec%100000)*10000) + usec;
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
					asprintf(&publish_data,PUBLISH_FORMAT,id,act,behavior,base64Str,sec);
//					asprintf(&publish_topic,"/power_run/%s",gisunlink_mqtt_clientid());
					asprintf(&publish_topic,"/power_run/%s",DeviceHWSn); //采用设备sn号做客户端ID
					gisunlink_print(GISUNLINK_PRINT_ERROR,"%s ->data:%s",publish_topic,publish_data);
					if(publish_ack == MQTT_PUBLISH_NEEDACK) {
						gisunlink_mqtt_publish(publish_topic,publish_data,2,id,publish_ack);
					} else {
						gisunlink_mqtt_publish(publish_topic,publish_data,0,id,publish_ack);
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

static void gisunlink_mqtt_connectCb(MQTT_CONNECT_STATUS status) {
	if(status == MQTT_CONNECT_SUCCEED) {

		char task_topic[64] = {0}; 
		char prv_upgrade_topic[64] = {0}; 
		char upgrade_topic[32] = {0};
		gisunlink_system_set_state(gisunlink_system,GISUNLINK_NETMANAGER_CONNECTED_SER);
		gisunlink_print(GISUNLINK_PRINT_INFO,"MQTT connect succeed");
		sprintf(task_topic, "%s/%s","/point_switch",DeviceHWSn);
		sprintf(prv_upgrade_topic, "%s/%s","/point_common",DeviceHWSn);
		sprintf(upgrade_topic, "%s","/point_common");
		gisunlink_mqtt_subscribe(task_topic,0);
		gisunlink_mqtt_subscribe(upgrade_topic,0);
		gisunlink_mqtt_subscribe(prv_upgrade_topic,0);
	} else {
		gisunlink_system_set_state(gisunlink_system,GISUNLINK_NETMANAGER_DISCONNECTED_SER);
		gisunlink_print(GISUNLINK_PRINT_ERROR,"MQTT connect failed");
	} 
}

static uint8 gisunlink_message_callback(void *message_data) {
	if(message_data && gisunlink_system) {
		gisunlink_message *message = (gisunlink_message *)message_data;
		switch(message->src_id) {
			case GISUNLINK_NETMANAGER_MODULE:
				if(message->data && message->data_len) {
					gisunlink_system->routeHandle(message->data,gisunlink_system);
				}
				break;
			case GISUNLINK_PERIPHERAL_MODULE:
				if(message->data && message->data_len) {
					gisunlink_mqtt_message_publish("transfer",message->data);
					gisunlink_system->uartHandle(message->data,gisunlink_system);
				}
		}
	}
	return true;
}

static void gisunlink_free_uart_respond_message(gisunlink_respond_message *respond) {
	if(respond) {
		if(respond->data) {
			gisunlink_free(respond->data);
			respond->data = NULL;
		}
		gisunlink_free(respond);
		respond = NULL;
	}
}

uint8 gisunlink_firmware_query(gisunlink_firmware_update *firmware) {
	uint8 ret = GISUNLINK_DEVICE_TIMEOUT;
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
						if(status == GISUNLINK_NEED_UPGRADE) {
							gisunlink_print(GISUNLINK_PRINT_WARN,"status %d device need upgrade!",status);
							ret = GISUNLINK_NEED_UPGRADE;
						} else {
							gisunlink_print(GISUNLINK_PRINT_WARN,"status %d device no need upgrade!",status);
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
				gisunlink_free_uart_respond_message(respond);
			}
		}
		gisunlink_free(uart_data);
		uart_data = NULL;
	}

	if(ret == GISUNLINK_DEVICE_TIMEOUT) {
		update_retry = true;
	} else {
		update_retry = false;
	}

	update_retry_tick = 0;

	gisunlink_print(GISUNLINK_PRINT_WARN,"firmware_query update_retry:%s",update_retry ? "true" : "false");

	return ret;
} 

bool gisunlink_firmware_transfer(uint16 offset,uint8 *data,uint16 len) {
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
					gisunlink_free_uart_respond_message(respond);
					break;
				} else {
					if(!retry) {
						gisunlink_print(GISUNLINK_PRINT_ERROR,"send firmware_transfer packet_id:%d timeout, offset:%d len:%d",respond->id,offset,len);
					}
					gisunlink_task_delay(50 / portTICK_PERIOD_MS);
				}
				gisunlink_free_uart_respond_message(respond);
			}
		}
		gisunlink_free(uart_data);
		uart_data = NULL;
	}
	return send_succeed;
} 

uint8 gisunlink_firmware_chk(void) {
	uint8 ret = GISUNLINK_DEVICE_TIMEOUT;
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
				gisunlink_free_uart_respond_message(respond);
				break;
			} else {
				if(!retry) {
					gisunlink_print(GISUNLINK_PRINT_ERROR,"send firmware_chk packet_id:%d timeout",respond->id);
				}
				gisunlink_task_delay(100 / portTICK_PERIOD_MS);
			}
			gisunlink_free_uart_respond_message(respond);
		}
	}
	return ret;
}

static bool gisunlink_wait_hw_sn(void) {
	bool getHWSn = false;
	gisunlink_peripheral_message uart_message = {
		.cmd = GISUNLINK_HW_SN,
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
					snprintf(DeviceHWSn,sizeof(DeviceHWSn),"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
							respond->data[0],respond->data[1],respond->data[2],respond->data[3],respond->data[4],respond->data[5],
							respond->data[6],respond->data[7],respond->data[8],respond->data[9],respond->data[10],respond->data[11]);
					gisunlink_print(GISUNLINK_PRINT_ERROR,"device sn:%s",DeviceHWSn);
					getHWSn = true;
				}
				gisunlink_free_uart_respond_message(respond);
				break;
			} else {
				gisunlink_print(GISUNLINK_PRINT_ERROR,"send wait_hw_sn packet_id:%d timeout",respond->id);
				gisunlink_task_delay(100 / portTICK_PERIOD_MS);
			}
			gisunlink_free_uart_respond_message(respond);
		}
	}
	return getHWSn;
}

void app_main(void) {
	gisunlink_system = gisunlink_system_init(DeviceHWSn,gisunlink_message_callback);
	update_hook.query = gisunlink_firmware_query; 
	update_hook.transfer = gisunlink_firmware_transfer; 
	update_hook.check = gisunlink_firmware_chk; 
	gisunlink_updatefirmware_register_hook(&update_hook);

	while(1) {

		//参试获取设备串号
		if(waitHWSn == false) {
			waitHWSn = gisunlink_wait_hw_sn();
		}

		//如果已经连接上了ap
		if(gisunlink_system->isConnectAp()) {
			//检查系统时间有没有更新
			if(gisunlink_system_time_ok(gisunlink_system)) {
				//拿到设备串号执行相关业务调用
				if(waitHWSn) {
					//链接MQTT服务器
					gisunlink_mqtt_connect(gisunlink_mqtt_connectCb,gisunlink_mqtt_messageCb);

					//触发了升级重试
					if(update_retry) {
						if(++update_retry_tick >= 60) {
							update_retry_tick = 0;
							//触发一个升级信号
							gisunlink_updatefirmware_start_signal();
						}
					}

				} else{ //参试获取设备串号 
					waitHWSn = gisunlink_wait_hw_sn();
				}
				gisunlink_print(GISUNLINK_PRINT_INFO,"heap_size:%d rssi:%d time:%d - %d (us)",gisunlink_system->heapSize(),(signed char)gisunlink_system->apRssi(),gisunlink_system->getSec(),gisunlink_system->getUsec());
			} else {
				gisunlink_print(GISUNLINK_PRINT_WARN,"heap_size:%d rssi:%d",gisunlink_system->heapSize(),(signed char)gisunlink_system->apRssi());
			} 
		} else {
			gisunlink_print(GISUNLINK_PRINT_ERROR,"heap_size:%d rssi:%d",gisunlink_system->heapSize(),(signed char)gisunlink_system->apRssi());
		}
		gisunlink_task_delay(1000 / portTICK_PERIOD_MS);
	}
}
