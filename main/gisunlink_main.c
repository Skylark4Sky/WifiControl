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

#include <stdio.h>
#include "esp_libc.h"
#include "esp_system.h"
#include "esp_log.h"

#include "gisunlink.h"
#include "gisunlink_mqtt.h"
#include "gisunlink_mqtt_task.h"
#include "gisunlink_utils.h"
#include "gisunlink_atomic.h"
#include "gisunlink_config.h"
#include "gisunlink_system.h"
#include "gisunlink_message.h"
#include "gisunlink_peripheral.h"
#include "gisunlink_updatefirmware.h"
#include "gisunlink_update_task.h"

static gisunlink_system_ctrl *gisunlink_system = NULL;
static gisunlink_firmware_update_hook update_hook = {
	.update = false,
	.version = 0,
	.file_size = 0,
	.send_size = 0,
};

static uint8 gisunlink_message_callback(gisunlink_system_ctrl *gisunlink_system, void *message_data) {
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
					mqttMessagePublish(gisunlink_system,"transfer",message->data);
					gisunlink_system->uartHandle(message->data,gisunlink_system);
				}
		}
	}
	return true;
}

static void gisunlink_mqtt_connectCb(MQTT_CONNECT_STATUS status) {
	if(status == MQTT_CONNECT_SUCCEED) {
		char task_topic[64] = {0}; 
		char prv_upgrade_topic[64] = {0}; 
		char upgrade_topic[32] = {0};
		gisunlink_system_set_state(gisunlink_system,GISUNLINK_NETMANAGER_CONNECTED_SER);
		gisunlink_print(GISUNLINK_PRINT_INFO,"MQ connect succeed");
		sprintf(task_topic, "%s/%s","/point_switch",gisunlink_system->deviceHWSn);
		sprintf(prv_upgrade_topic, "%s/%s","/point_common",gisunlink_system->deviceHWSn);
		sprintf(upgrade_topic, "%s","/point_common");
		gisunlink_mqtt_subscribe(task_topic,0);
		gisunlink_mqtt_subscribe(upgrade_topic,0);
		gisunlink_mqtt_subscribe(prv_upgrade_topic,0);
	} else {
		gisunlink_system_set_state(gisunlink_system,GISUNLINK_NETMANAGER_DISCONNECTED_SER);
		gisunlink_print(GISUNLINK_PRINT_ERROR,"MQTT connect failed");
	} 
}

static void gisunlink_mqtt_messageCb(gisunlink_mqtt_message *message) {

	if(update_hook.update == true) {
		char *msg = NULL;
		asprintf(&msg,"system upgrade! ver:%ld file_size:%ld progress_size: %ld",update_hook.version,update_hook.file_size,update_hook.send_size);
		mqttMessageRespond("transfer",message->behavior,message->id,false,msg);
		if(msg) {
			gisunlink_free(msg);
			msg = NULL;
		}
		return;
	} 

	mqttMessageHandle(message);
}

void app_main(void) {
	gisunlink_system = gisunlink_system_init(gisunlink_message_callback);

	if(gisunlink_system) {
		update_hook.query = firmwareQuery; 
		update_hook.transfer = firmwareTransfer; 
		update_hook.check = firmwareChk; 
		gisunlink_updatefirmware_register_hook(&update_hook);
	}

	while(1) {
		if(gisunlink_system->isConnectAp()) {
			if(gisunlink_system->waitHWSn == false) {
				gisunlink_system->waitHWSn = getDeviceHWSnOrFirmwareVersion(GISUNLINK_HW_SN,gisunlink_system->deviceHWSn);
			}

			if(gisunlink_system->waitFirmwareVersion == false) {
				gisunlink_system->waitFirmwareVersion = getDeviceHWSnOrFirmwareVersion(GISUNLINK_FIRMWARE_VERSION,gisunlink_system->deviceFWVersion);
			}

			if(gisunlink_system->waitHWSn && gisunlink_system->waitFirmwareVersion) {
				gisunlink_mqtt_connect(gisunlink_mqtt_connectCb,gisunlink_mqtt_messageCb);
				if(update_hook.update_retry) {
					if(++update_hook.update_retry_tick >= 60) {
						update_hook.update_retry_tick = 0;
						gisunlink_updatefirmware_start_signal();
					}
				}
			}
		}
		gisunlink_print(GISUNLINK_PRINT_INFO,"heap_size:%d rssi:%d time:%d - %d (us)",getHeapSize(),(signed char)getApRssi(),getNowTimeBySec(),getNowTimeByUSec());
		gisunlink_task_delay(1000 / portTICK_PERIOD_MS);
	}
}
