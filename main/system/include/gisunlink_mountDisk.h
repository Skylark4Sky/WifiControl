/*
* _COPYRIGHT_
*
* File Name:gisunlink_mountDisk.h
* System Environment: JOHAN-PC
* Created Time:2019-01-22
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#ifndef __GISUNLINK_MOUNTDISK_H__
#define __GISUNLINK_MOUNTDISK_H__

#include "gisunlink_type.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*! @brief 挂载spiffs
 * @param void
 * @return true or false
 */
bool gisunlink_mount_disk(void);

/*! @brief 卸载spiffs
 * @param void
 * @return void
 */
void gisunlink_unmount_disk(void);

#ifdef __cplusplus
}
#endif

#endif //__GISUNLINK_MOUNTDISK_H__
