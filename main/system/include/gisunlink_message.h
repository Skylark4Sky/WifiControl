/*
* _COPYRIGHT_
*
* File Name:gisunlink_message.h
* System Environment: JOHAN-PC
* Created Time:2019-01-23
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#ifndef __GISUNLINK_MESSAGE_H__
#define __GISUNLINK_MESSAGE_H__

#include "gisunlink_type.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum {
	SYNCMSG,                    //同步消息
    ASYNCMSG,                   //异步消息
} MESSAGETYPE;

enum GISUNLINK_MSG_MODULE_ID {
    GISUNLINK_BROADCAST,
    GISUNLINK_MAIN_MODULE,
    GISUNLINK_SYSTEM_MODULE,
    GISUNLINK_NETMANAGER_MODULE,
	GISUNLINK_PERIPHERAL_MODULE,
	GISUNLINK_AUTHORIZATION_MODULE,
};

enum GISUNLINK_MESSAGE_ID {
    GISUNLINK_NETMANAGER_STATE_MSG,		//网络状态更新消息
    GISUNLINK_NETMANAGER_PAIRING_MSG,	//进入网络配对状态
	GISUNLINK_PERIPHERAL_UART_MSG,		//串口消息
	GISUNLINK_AUTHORIZATION_MSG,		//设备验证消息
};

typedef void *GISUNLINK_MESSAGE_MALLOC(void *msg_data);
typedef void GISUNLINK_MESSAGE_FREE(void *msg_data);
typedef uint32 GISUNLINK_MESSAGE_FINISH_CALLBACK(void *data);

typedef struct _gisunlink_message {
    uint8 type;									//消息类型，0:同步消息，1:异步消息
    uint8 src_id;                               //发出消息源模块ID
    uint32 dst_id;                              //目的地模块ID
    uint8 attr;                                 //发出模块的一些特殊信息
    uint8 message_id;                           //模块命令ID
    uint16 data_len;                            //数据长度
    uint8 *data;                                //数据
    GISUNLINK_MESSAGE_MALLOC *malloc;
    GISUNLINK_MESSAGE_FREE *free;
    GISUNLINK_MESSAGE_FINISH_CALLBACK *callback; //当消息处理完后给出的回调，如果不需要通知，可以调协NULL
} gisunlink_message;

typedef uint8 GISUNLINK_MESSAGE_CALLBACK(gisunlink_message *msg);

/*! @brief 初始化消息传递模块
 * @param void 
 * @return void
 */
void gisunlink_message_init(void);

/*! @brief 注册消息传递
 * @param 模块ID 
 * @param 消息处理回调 
 * @return void
 */
void gisunlink_message_register(uint8 model_id, GISUNLINK_MESSAGE_CALLBACK *pModelCall);

/*! @brief 取消消息传递
 * @param 模块ID 
 * @return void
 */
void gisunlink_message_unregister(uint8 model_id);

/*! @brief 传递消息 同步消息需手动回收内存
 * @param 消息 
 * @return void
 */
uint8 gisunlink_message_push(gisunlink_message *msg); 

#ifdef __cplusplus
}
#endif

#endif //__GISUNLINK_MESSAGE_H__
