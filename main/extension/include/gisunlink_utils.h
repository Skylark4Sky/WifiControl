/*
* _COPYRIGHT_
*
* File Name:gisunlink_utils.h
* System Environment: JOHAN-PC
* Created Time:2020-07-21
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#ifndef __GISUNLINK_SYSTEM_UTILS_H__
#define __GISUNLINK_SYSTEM_UTILS_H__

#include "gisunlink.h"
#include "gisunlink_peripheral.h"

#ifdef __cplusplus
extern "C"
{
#endif

uint32 getNowTimeBySec(void);

uint32 getNowTimeByUSec(void);

char getApRssi(void);

uint32 getHeapSize(void);

void freeUartRespondMessage(gisunlink_respond_message *respond);

bool getDeviceHWSnOrFirmwareVersion(uint8 cmd,char *buffter);

#ifdef __cplusplus
}
#endif

#endif //__GISUNLINK_SYSTEM_UTILS_H__