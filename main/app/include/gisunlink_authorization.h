/*
* _COPYRIGHT_
*
* File Name:gisunlink_authorization.h
* System Environment: JOHAN-PC
* Created Time:2020-08-07
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#ifndef __GISUNLINK_AUTHORIZATION_H__
#define __GISUNLINK_AUTHORIZATION_H__

#include "gisunlink_system.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*! @brief 加载认证模块 
 * @return void
 */
void gisunlink_authorization_init(gisunlink_system_ctrl *gisunlink_system);

/*! @brief 启动认证线程 
 * @return void
 */
void gisunlink_authorization_start_task(void);

#ifdef __cplusplus
}
#endif

#endif //__GISUNLINK_AUTHORIZATION_H__
