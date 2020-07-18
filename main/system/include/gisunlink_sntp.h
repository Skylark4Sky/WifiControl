/*
* _COPYRIGHT_
*
* File Name:gisunlink_sntp.h
* System Environment: JOHAN-PC
* Created Time:2019-04-10
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#ifndef __GISUNLINK_SNTP_H__
#define __GISUNLINK_SNTP_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum {
	GISUNLINK_SNTP_SUCCEED,
	GISUNLINK_SNTP_FAILURE,
} SNTP_RESPOND;

typedef void GISUNLINK_SNTP_RESPOND(SNTP_RESPOND repsond, void *parm);

/*! @brief 初始化获取网络时间接口
 * @param 执行回调
 * @return void
 */
void gisunlink_sntp_initialize(GISUNLINK_SNTP_RESPOND *respondCb,void *param);

/*! @brief 获取时间戳接口
 * @param void 
 * @return int
 */
int gisunlink_sntp_get_timestamp(void); 

#ifdef __cplusplus
}
#endif

#endif //__GISUNLINK_SNTP_H__
