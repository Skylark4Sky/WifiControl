/*
* _COPYRIGHT_
*
* File Name:gisunlink_updatefirmware.h
* System Environment: JOHAN-PC
* Created Time:2019-04-18
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#ifndef __GISUNLINK_UPDATEFIRMWARE_H__
#define __GISUNLINK_UPDATEFIRMWARE_H__

#include "gisunlink.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef uint8 GISUNLINK_FIRMWARE_QUERY(gisunlink_firmware_update *firmware);
typedef bool GISUNLINK_FIRMWARE_TRANSFER(uint16 offset,uint8 *data,uint16 len);
typedef uint8 GISUNLINK_FIRMWARE_CHK(void);

typedef struct _gisunlink_firmware_update_hook {
	bool update;
	uint32 version;
	uint32 file_size;
	uint32 send_size;
	GISUNLINK_FIRMWARE_QUERY *query;
	GISUNLINK_FIRMWARE_TRANSFER *transfer;
	GISUNLINK_FIRMWARE_CHK *check;
} gisunlink_firmware_update_hook;

/*! @brief 固件下载检查
 * @param json 
 * @param json_len
 * @return void
 */
void gisunlink_updatefirmware_download_new_firmware(const char *jsonData, uint16 json_len);

/*! @brief 固件更新初始化
 * @return void
 */
void gisunlink_updatefirmware_init(void);

/*! @brief 固件更新回调
 * @param gisunlink_firmware_update_hook 
 *  @return void
 */
void gisunlink_updatefirmware_register_hook(gisunlink_firmware_update_hook *hook);

/*! @brief 系统启动后给个就绪调用
 * @return void
 */
void gisunlink_updatefirmware_start_signal(void);

#ifdef __cplusplus
}
#endif

#endif //__GISUNLINK_UPDATEFIRMWARE_H__
