/*
* _COPYRIGHT_
*
* File Name:gisunlink.h
* System Environment: JOHAN-PC
* Created Time:2019-01-22
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#ifndef __GISUNLINK_H__
#define __GISUNLINK_H__

//Base
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
//Pro
#include "gisunlink_type.h"
#include "gisunlink_print.h"

#define MACMAXLEN           6
#define SSIDMAXLEN          32
#define PSWDMAXLEN          64
#define TOKENMAXLEN			32

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum {
    WIFI_CONFIGURED_MODE = 0x0F,
    WIFI_UNCONFIGURED_MODE = 0xF0,
} WIFI_POWERON_MODE;

typedef enum {
	DEVICE_AUTHORIZE_UNKNOWN = 0x00,
	DEVICE_AUTHORIZE_PASS = 0x01,
	DEVICE_AUTHORIZE_NOTPASS = 0x02,
} DEVICE_AUTHORIZE_STATE;

//串口通信
typedef enum {
	GISUNLINK_NETWORK_STATUS = 0x05,						//网络当前状态
	GISUNLINK_NETWORK_RESET = 0x06,							//重设置网络链接 (仅仅针对wifi模块)
	GISUNLINK_NETWORK_RSSI = 0x07,							//网络信号强度

	GISUNLINK_DEV_FW_INFO = 0x08,							//设置固件升级版本
	GISUNLINK_DEV_FW_TRANS = 0x09,							//固件数据传输
	GISUNLINK_DEV_FW_READY = 0x0A,							//固件确认完成
	GISUNLINK_DEV_SN = 0x0B,								//获取设备SN号
	GISUNLINK_TASK_CONTROL = 0x0C,							//网络数据透传

	GISUNLINK_HW_SN = 0x0D,									//获取硬件SN号
} GISUNLINK_UART_CMD;

typedef enum {
	GISUNLINK_NETMANAGER_IDLE,				//空闲
	GISUNLINK_NETMANAGER_START,				//开始连接
	GISUNLINK_NETMANAGER_CONNECTING,		//连接中
	GISUNLINK_NETMANAGER_CONNECTED,			//已连接
	GISUNLINK_NETMANAGER_DISCONNECTED,		//断开连接
	GISUNLINK_NETMANAGER_RECONNECTING,		//重连中
	GISUNLINK_NETMANAGER_ENT_CONFIG,		//进入配对
	GISUNLINK_NETMANAGER_EXI_CONFIG,		//退出配对
	GISUNLINK_NETMANAGER_SAVE_CONFIG,		//保存配对信息
	GISUNLINK_NETMANAGER_TIME_SUCCEED,		//同步时钟成功
	GISUNLINK_NETMANAGER_TIME_FAILED,		//同步时钟失败
	GISUNLINK_NETMANAGER_CONNECTED_SER,		//已连上平台
	GISUNLINK_NETMANAGER_DISCONNECTED_SER,	//已断开平台
	GISUNLINK_NETMANAGER_UNKNOWN
} GISUNLINK_NETMANAGER_WORK_STATE;

typedef struct _gisunlink_wifi_conf {
	uint8 ssid[SSIDMAXLEN];
	size_t ssid_len;
	uint8 pswd[PSWDMAXLEN];
	size_t pswd_len;
} gisunlink_wifi_conf;

typedef struct _gisunlink_token_conf {
	uint8 string[TOKENMAXLEN];
	size_t length;
} gisunlink_token_conf;

typedef struct _gisunlink_sn_code_conf {
	uint8 string[TOKENMAXLEN];
	size_t length;
} gisunlink_sn_code_conf;

typedef struct _gisunlink_conf {
	uint8 mac_addr[MACMAXLEN];
	uint8 poweron;			//WIFI启动模式
	uint8 is_authorization;	//设备是否授权
	gisunlink_wifi_conf wifi;
	gisunlink_token_conf token;
	gisunlink_sn_code_conf sn_code;
} gisunlink_conf;

typedef struct _gisunlink_netmanager_event {
	uint8 id;
	void *param;
	void (*param_free)(void *param);
} gisunlink_netmanager_event;

typedef struct _gisunlink_uart_event {
	uint32 flow_id;				//流id
	uint8 cmd;
	uint8 *data;
	uint16 data_len;
} gisunlink_uart_event;

typedef struct _gisunlink_firmware_download {
	uint8 *path;
	uint16 path_len;
	uint8 *md5; 
	uint8 md5_len;
	uint32 size;
	uint32 ver;
	uint8 download_over;
} gisunlink_firmware_download;

typedef struct _gisunlink_firmware_update {
	gisunlink_firmware_download *download;
	bool transfer_over;
} gisunlink_firmware_update;

#ifdef __cplusplus
}
#endif


#endif //__GISUNLINK_H__
