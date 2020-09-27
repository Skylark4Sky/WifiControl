/*
* _COPYRIGHT_
*
* File Name:gisunlink_ota.h
* System Environment: JOHAN-PC
* Created Time:2019-03-05
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#ifndef __GISUNLINK_OTA_H__
#define __GISUNLINK_OTA_H__

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct _gisunlink_ota {
	uint8 *path;
	uint32_t size;
	uint32 download_process;
} gisunlink_ota;


/*! @brief 初始化OTA升级模块
 * @param void 
 * @return void
 */
void gisunlink_ota_task(const char *url,unsigned int size);


void gisunlink_ota_init(void);

#ifdef __cplusplus
}
#endif

#endif //__GISUNLINK_OTA_H__
