/*
* _COPYRIGHT_
*
* File Name:gisunlink_system.h
* System Environment: JOHAN-PC
* Created Time:2019-01-22
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#ifndef __GISUNLINK_SYSTEM_H__
#define __GISUNLINK_SYSTEM_H__

#include "gisunlink.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef uint32_t GISUNLINK_HEAP_SIZE(void);
typedef uint32 GISUNLINK_TIME(void);
typedef char GISUNLINK_WIFI_RSSI(void);
typedef bool GISUNLINK_IS_CONNECT_AP(void);
typedef void GISUNLINK_MESSAGE_EVENT(void *message, void *param);
typedef void GISUNLINK_TIME_SYNC(bool);
typedef uint8 GISUNLINK_MESSAGE_CB(void *message);

typedef struct _gisunlink_system_ctrl {
	bool time_sync;
	uint8 state;
	GISUNLINK_HEAP_SIZE *heapSize;
	GISUNLINK_WIFI_RSSI *apRssi;
	GISUNLINK_IS_CONNECT_AP *isConnectAp;
	GISUNLINK_MESSAGE_EVENT *uartHandle;
	GISUNLINK_MESSAGE_EVENT *routeHandle;
	GISUNLINK_TIME *getSec;
	GISUNLINK_TIME *getUsec;
} gisunlink_system_ctrl;

/*! @brief 系统初始化
 * @param void 
 * @return void
 */
gisunlink_system_ctrl *gisunlink_system_init(char *DeviceHWSn_addr,GISUNLINK_MESSAGE_CB *messageCb);

/*! @brief 检查网络时间是否同步
 * @param gisunlink_system_ctrl 
 * @return true or false
 */
bool gisunlink_system_time_ok(gisunlink_system_ctrl *gisunlink_system);

/*! @brief 设置系统状态 
 * @param GISUNLINK_NETMANAGER_WORK_STATE
 * @return void 
 */
void gisunlink_system_set_state(gisunlink_system_ctrl *gisunlink_system,uint8 state);

/*! @brief 网络配置状态 
 * @return true or false 
 */
bool gisunlink_system_pairing(void);

#ifdef __cplusplus
}
#endif

#endif //__GISUNLINK_SYSTEM_H__
