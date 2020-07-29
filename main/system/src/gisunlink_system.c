/*
* _COPYRIGHT_
*
* File Name: gisunlink_system.c
* System Environment: JOHAN-PC
* Created Time:2019-01-22
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


#include "gisunlink_mqtt.h"
#include "gisunlink_sntp.h"
#include "gisunlink_config.h"
#include "gisunlink_atomic.h"
#include "gisunlink_system.h"
#include "gisunlink_message.h"
#include "gisunlink_netmanager.h"
#include "gisunlink_peripheral.h"
//#include "gisunlink_authorization.h"
#include "gisunlink_updatefirmware.h"

static void gisunlink_sntp_respond(SNTP_RESPOND repsond,void *param) {
	if(param) {
		gisunlink_system_ctrl *gisunlink_system = (gisunlink_system_ctrl *)param;
		if(GISUNLINK_SNTP_SUCCEED == repsond) {
			gisunlink_print(GISUNLINK_PRINT_ERROR,"GISUNLINK_NETMANAGER_TIME_SUCCEED");
			//gisunlink_system_set_state(gisunlink_system,GISUNLINK_NETMANAGER_TIME_SUCCEED);
			gisunlink_system->time_sync = true;
		} else {
			//gisunlink_system_set_state(gisunlink_system,GISUNLINK_NETMANAGER_TIME_FAILED);
			gisunlink_system->time_sync = false;
		}
	}
}

static void gisunlink_system_netmanager_event(void *message, void *param) {
	gisunlink_netmanager_event *event = (gisunlink_netmanager_event *)message;
	if(message) {
		gisunlink_system_set_state((gisunlink_system_ctrl *)param,event->id);
		switch((uint8)event->id) {
			case GISUNLINK_NETMANAGER_IDLE:
			case GISUNLINK_NETMANAGER_START:
			case GISUNLINK_NETMANAGER_CONNECTING:
			case GISUNLINK_NETMANAGER_RECONNECTING:
			case GISUNLINK_NETMANAGER_ENT_CONFIG: 
			case GISUNLINK_NETMANAGER_EXI_CONFIG:
			case GISUNLINK_NETMANAGER_SAVE_CONFIG: 
				break;
			case GISUNLINK_NETMANAGER_CONNECTED:
				//时间同步完成后，先给固件模块一个信号
				gisunlink_sntp_initialize(gisunlink_sntp_respond,param);
				gisunlink_updatefirmware_start_signal();
				//gisunlink_authorization_check();
				break;
			case GISUNLINK_NETMANAGER_DISCONNECTED:
				((gisunlink_system_ctrl *)param)->time_sync = false;
				break;
		}
	}
}

static void gisunlink_system_uart_event(void *message, void *param) {
	gisunlink_uart_event *uart = (gisunlink_uart_event *)message;
	if(uart) {
		gisunlink_print(GISUNLINK_PRINT_WARN,"Recv ID:%d CMD:0x%02X",uart->flow_id,uart->cmd);
		switch(uart->cmd) {
			case GISUNLINK_NETWORK_RESET: //网络重设
				{
					if(false == gisunlink_netmanager_is_enter_pairing()) {
						gisunlink_print(GISUNLINK_PRINT_ERROR,"GISUNLINK_RESETNET_CMD");
						gisunlink_netmanager_enter_pairing(GISUNLINK_MAIN_MODULE);
					}
				}
				break;
			case GISUNLINK_DEV_SN:		//设备SN号
				{
					char *device_sn = gisunlink_get_mac_with_string("GSL");
					gisunlink_peripheral_message uart_message = {
						.cmd = GISUNLINK_DEV_SN,
						.data = (uint8 *)device_sn,
						.data_len = strlen(device_sn),
						.respond = UART_NO_RESPOND,
						.respondCb = NULL,
					};
					gisunlink_peripheral_send_message(&uart_message);

					gisunlink_free(device_sn);
					device_sn = NULL;
				}
				break;
			case GISUNLINK_NETWORK_STATUS: //获取设备网络状态
				{
					uint8 state = ((gisunlink_system_ctrl *)param)->state; 
					gisunlink_peripheral_message uart_message = {
						.cmd = GISUNLINK_NETWORK_STATUS,
						.data = &state,
						.data_len = 1,
						.respond = UART_NO_RESPOND,
						.respondCb = NULL,
					};
					gisunlink_peripheral_send_message(&uart_message);
				}
				break;
		}
	}
}

static void gisunlink_system_setparm(gisunlink_system_ctrl *gisunlink_system) {
	gisunlink_system->time_sync = false;
	gisunlink_system->isConnectAp = gisunlink_netmanager_is_connected_ap; 
	gisunlink_system->uartHandle = gisunlink_system_uart_event;
	gisunlink_system->routeHandle = gisunlink_system_netmanager_event;
}

gisunlink_system_ctrl *gisunlink_system_init(GISUNLINK_MESSAGE_CB *messageCb) {
	gisunlink_system_ctrl *gisunlink_system = (gisunlink_system_ctrl *)gisunlink_malloc(sizeof(gisunlink_system_ctrl)); 
	if(gisunlink_system) {
		memset(gisunlink_system,0x0,sizeof(gisunlink_system_ctrl));
		gisunlink_system->state = gisunlink_netmanager_get_state();
		//设置系统参数
		gisunlink_system_setparm(gisunlink_system);
		//初始化消息传递模块
		gisunlink_message_init();
		if(messageCb) {
			gisunlink_message_register(GISUNLINK_MAIN_MODULE,(uint8 (*)(gisunlink_message *))messageCb);
		}
		//初始化配置
		gisunlink_config_init();
		//初始化外围模块
		gisunlink_peripheral_init();
		//初始化网络管理模块
		gisunlink_netmanager_init();
		//初始化设备授权模块
		//gisunlink_authorization_init(gisunlink_system->conf);
		//初始化MQTT
		gisunlink_mqtt_init(gisunlink_system->deviceHWSn,gisunlink_system->deviceFWVersion);
		//初始化固件下载
		gisunlink_updatefirmware_init();
	}
	return gisunlink_system;
}

bool gisunlink_system_time_ok(gisunlink_system_ctrl *gisunlink_system) {
	if(gisunlink_system) {
		if(gisunlink_system->time_sync) {
			return true;
		} else {
			gisunlink_sntp_initialize(gisunlink_sntp_respond,gisunlink_system);
		}
	}
	return false;
}

void gisunlink_system_set_state(gisunlink_system_ctrl *gisunlink_system,uint8 state) {
	if(gisunlink_system) {
		gisunlink_system->state = state;
		gisunlink_peripheral_message uart_message = {
			.cmd = GISUNLINK_NETWORK_STATUS,
			.data = &state,
			.data_len = 1,
			.respond = UART_NO_RESPOND,
			.respondCb = NULL,
		};

		gisunlink_print(GISUNLINK_PRINT_ERROR,"current netWork state:0x%02X",state);

		if(gisunlink_system_pairing() == false) {
			switch(state) {
				case GISUNLINK_NETMANAGER_ENT_CONFIG: 
				case GISUNLINK_NETMANAGER_EXI_CONFIG:
				case GISUNLINK_NETMANAGER_SAVE_CONFIG: 
				case GISUNLINK_NETMANAGER_CONNECTED_SER:
					gisunlink_peripheral_send_message(&uart_message);
					break;
			}
		} else {
			gisunlink_peripheral_send_message(&uart_message);
		}
	}
}

bool gisunlink_system_pairing(void) {
	return gisunlink_netmanager_is_enter_pairing();
}
