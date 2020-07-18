/*
* _COPYRIGHT_
*
* File Name:gisunlink_airlink.h
* System Environment: JOHAN-PC
* Created Time:2019-02-12
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#ifndef __GISUNLINK_AIRLINK_H__
#define __GISUNLINK_AIRLINK_H__

#include "gisunlink.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*! @brief 初始化socket
 * @param gisunlink_conf 
 * @return void
 */
void gisunlink_airlink_init(void);

/*! @brief airlink 是否在运行
 * @param void 
 * @return void
 */
uint8 gisunlink_airlink_is_run(void);

/*! @brief 销毁socket
 * @param void 
 * @return void
 */
void gisunlink_airlink_deinit(void);


#ifdef __cplusplus
}
#endif

#endif //__GISUNLINK_AIRLINK_H__
