/*
* _COPYRIGHT_
*
* File Name:gisunlink_uart.h
* System Environment: JOHAN-PC
* Created Time:2019-01-25
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#ifndef __GISUNLINK_UART_H__
#define __GISUNLINK_UART_H__

#include "gisunlink_type.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*! @brief 串口发送接口
 * @param 数据 
 * @param 长度 
 * @return int
 */
int gisunlink_uart_send_data(uint8 *data, int len);

/*! @brief 串口接收接口
 * @param 地址 
 * @param 数量 
 * @return int
 */
int gisunlink_uart_recv_data(uint8 *buffer, int count);

/*! @brief 初始化串口驱动
 * @param void 
 * @return void
 */
void gisunlink_uart_init(void);

#ifdef __cplusplus
}
#endif

#endif //__GISUNLINK_UART_H__
