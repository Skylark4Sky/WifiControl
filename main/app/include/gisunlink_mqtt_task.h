/*
* _COPYRIGHT_
*
* File Name:gisunlink_mqtt_task.h
* System Environment: JOHAN-PC
* Created Time:2020-07-20
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#ifndef __GISUNLINK_MQTT_TASK_H__
#define __GISUNLINK_MQTT_TASK_H__

#include "gisunlink.h"
#include "gisunlink_system.h"
#include "gisunlink_message.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*! @brief MQ消息发布
 * @param gisunlink_system_ctrl 
 * @param 动作 
 * @param 消息 
 * @return void
 */
void mqttMessagePublish(gisunlink_system_ctrl *gisunlink_system, char *act,void *message);

/*! @brief MQ消息回复
 * @param 动作 
 * @param 行为 
 * @param 请求的id 
 * @param 处理结果 
 * @param 消息 
 * @return void
 */
void mqttMessageRespond(const char *act,uint8 behavior,uint32 req_id,bool result,const char *msg);

/*! @brief MQ消息处理
 * @param gisunlink_mqtt_message 
 * @return void
 */
void mqttRecvMessageHandle(gisunlink_system_ctrl *gisunlink_system, gisunlink_mqtt_message *message);

#ifdef __cplusplus
}
#endif

#endif //__GISUNLINK_MQTT_TASK_H__
