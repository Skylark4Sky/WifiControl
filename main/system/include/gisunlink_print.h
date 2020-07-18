/*
* _COPYRIGHT_
*
* File Name:gisunlink_print.h
* System Environment: JOHAN-PC
* Created Time:2019-01-22
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#ifndef __GISUNLINK_PRINT_H__
#define __GISUNLINK_PRINT_H__

#include "gisunlink_type.h"

#define gisunlink_print_info(fmt, ... ) gisunlink_print(GISUNLINK_PRINT_INFO, fmt, ##__VA_ARGS__)
#define gisunlink_print_warn(fmt, ... ) gisunlink_print(GISUNLINK_PRINT_WARN, fmt, ##__VA_ARGS__)
#define gisunlink_print_error(fmt, ... ) gisunlink_print(GISUNLINK_PRINT_ERROR, fmt, ##__VA_ARGS__)

#define gisunlink_print_debug() gisunlink_print(GISUNLINK_PRINT_WARN, "Function:%s --> Line:%d", __FUNCTION__,__LINE__)

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum {
	GISUNLINK_PRINT_INFO,
	GISUNLINK_PRINT_WARN,
	GISUNLINK_PRINT_DEBUG,
	GISUNLINK_PRINT_ERROR,
}PRINTMODE;

/*! @brief 基础打印函数
 * @param char * fmt, 可变参数
 * @return void
 */
void gisunlink_print(PRINTMODE mode,char* fmt, ...); 

void gisunlink_printCb(char *fmt, ...);

void gisunlink_print_byteaddr(uint8 *byteAddr, uint32 byteLen);

#ifdef __cplusplus
}
#endif

#endif //__GISUNLINK_PRINT_H__
