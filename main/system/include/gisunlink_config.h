/*
* _COPYRIGHT_
*
* File Name:gisunlink_config.h
* System Environment: JOHAN-PC
* Created Time:2019-01-22
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#ifndef __GISUNLINK_CONFIG_H__
#define __GISUNLINK_CONFIG_H__

#include "gisunlink_type.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum {
    WIFI_CONFIG,
	POWERON_CONFIG,
	STRING_CONFIG,
    MAC_CONFIG,
    SYSTEM_CONFIG,
	AUTHORIZATION_CONFIG,
	SN_CODE_CONFIG,
	TOKEN_CONFIG,
	DOWNLOAD,
	FIRMWARE,
} CONFIG_TYPE;

/*! @brief 系统配置初始化
 * @param void
 * @return void
 */
void gisunlink_config_init(void);

/*! @brief 获取系统配置
 * @param CONFIG_TYPE
 * @param 配置指针
 * @return true false
 */
bool gisunlink_config_get(uint8 conf_type,void *conf);

/*! @brief 设置系统配置
 * @param CONFIG_TYPE
 * @param 配置指针
 * @return true false
 */
bool gisunlink_config_set(uint8 conf_type,void *conf);

/*! @brief 获取MAC地址 
 * @param 前缀
 * @return Mac 字串 
 */
char *gisunlink_get_mac_with_string(const char *prefix);

/*! @brief 获取固件版本 
 * @return 固件版本字串 
 */
char *gisunlink_get_firmware_version(void);

#ifdef __cplusplus
}
#endif

#endif //__GISUNLINK_CONFIG_H__
