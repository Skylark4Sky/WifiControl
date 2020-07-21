/*
* _COPYRIGHT_
*
* File Name:gisunlink_update_task.h
* System Environment: JOHAN-PC
* Created Time:2020-07-20
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#ifndef __GISUNLINK_UPDATE_TASK_H__
#define __GISUNLINK_UPDATE_TASK_H__

#include "gisunlink.h"

#ifdef __cplusplus
extern "C"
{
#endif

uint8 firmwareQuery(gisunlink_firmware_update *firmware);

uint8 firmwareChk(void);

bool firmwareTransfer(uint16 offset,uint8 *data,uint16 len);

#ifdef __cplusplus
}
#endif

#endif //__GISUNLINK_UPDATE_TASK_H__
