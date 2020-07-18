/*
* _COPYRIGHT_
*
* File Name:gisunlink_peripheral.h
* System Environment: JOHAN-PC
* Created Time:2019-01-26
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#ifndef __GISUNLINK_PERIPHERAL_H__
#define __GISUNLINK_PERIPHERAL_H__

#include "gisunlink_type.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum {
	GISUNLINK_SEM_ERROR,
	GISUNLINK_SEND_TIMEOUT,
	GISUNLINK_SEND_BUSY,
	GISUNLINK_SEND_SUCCEED,
} SEND_RESPOND_RESULT;

typedef struct _gisunlink_respond_message {
	uint8 reason;
	uint8 cmd;
	uint8 behavior;
	uint32 heading;
	uint32 id;
	uint8 *data;
	uint16 data_len;
} gisunlink_respond_message;

typedef void GISUNLINK_RESPOND_CALLBACK(gisunlink_respond_message *message);

typedef enum {
    UART_NO_RESPOND = 0x00,
	UART_RESPOND = 0x01,
} UART_RESPOND_FLAGS;

typedef struct _gisunlink_peripheral_message {
	uint8 cmd;
	uint8 behavior;
	uint32 heading;
	uint32 id;
	uint8 *data;
	uint16 data_len;
	uint8 respond;
	GISUNLINK_RESPOND_CALLBACK *respondCb;
} gisunlink_peripheral_message;

/*! @brief 初始化外围接口
 * @param void 
 * @return void
 */
void gisunlink_peripheral_init(void);

/*! @brief 发送通信请求数据 第一位为控制命令 
 * @param gisunlink_peripheral_message
 * @return gisunlink_respond_message
 */
gisunlink_respond_message *gisunlink_peripheral_send_message(gisunlink_peripheral_message *message);

/*! @brief 发送通信应答数据 第一位为控制命令 
 * @param 流控ID 
 * @param 命令 
 * @param 数据 
 * @param 数据长度 
 * @return bool true 成功 false 失败
 */
bool gisunlink_peripheral_respond(uint32 flow_id,uint8 cmd,uint8 *data,uint16 data_len);

#ifdef __cplusplus
}
#endif

#endif //__GISUNLINK_PERIPHERAL_H__
