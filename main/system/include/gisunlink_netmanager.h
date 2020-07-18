/*
* _COPYRIGHT_
*
* File Name:gisunlink_netmanager.h
* System Environment: JOHAN-PC
* Created Time:2019-01-23
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#ifndef __GISUNLINK_NETMANAGER_H__
#define __GISUNLINK_NETMANAGER_H__

#include "gisunlink.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*! @brief 初始化网络管理模块
 * @param gisunlink_conf
 * @return void
 */

void gisunlink_netmanager_init(void);

/*! @brief 获取网络状态
 * @param void 
 * @return 状态
 */
uint8 gisunlink_netmanager_get_state(void);

/*! @brief 是否在配对模式
 * @param void 
 * @return true or false
 */
bool gisunlink_netmanager_is_enter_pairing(void);

/*! @brief 进入网络配对模式
 * @param void 
 * @return void
 */
void gisunlink_netmanager_enter_pairing(uint8 module_id); 

/*! @brief 获取AP信号强度
 * @param void 
 * @return int8 
 */
int8 gisunlink_netmanager_get_ap_rssi(void);

/*! @brief 是否已链接AP
 * @return bool 
 */
bool gisunlink_netmanager_is_connected_ap(void);

#ifdef __cplusplus
}
#endif

#endif //__GISUNLINK_NETMANAGER_H__
