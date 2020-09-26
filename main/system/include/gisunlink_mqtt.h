/*
* _COPYRIGHT_
*
* File Name:gisunlink_mqtt.h
* System Environment: JOHAN-PC
* Created Time:2019-04-07
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#ifndef __GISUNLINK_MQTT_H__
#define __GISUNLINK_MQTT_H__

#include "gisunlink.h"

#define SPLIT_PACKET_SIZE 10

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum {
	UNKNOWN_ACT,
	TRANSFER_ACT,
	UPDATE_VER_ACT,
	DEVICE_INFO_ACT,
} TOPIC_TYPE_ACT;

typedef struct _gisunlink_mqtt_message {
	uint32 id; 
	uint8 behavior;
	uint16 topic_len;
	uint16 act_len;
	uint16 data_len;
	TOPIC_TYPE_ACT act_type;
	char *topic;
	char *act; 
	char *data;
} gisunlink_mqtt_message;

typedef enum {
	MQTT_CONNECT_SUCCEED,
	MQTT_CONNECT_FAILED,
} MQTT_CONNECT_STATUS;

typedef enum {
	MQTT_PUBLISH_NOACK,
	MQTT_PUBLISH_NEEDACK,
} MQTT_PUBLISH_ACKSTATUS;

typedef void GISUNLINK_MQTT_CONNECT(MQTT_CONNECT_STATUS status);
typedef void GISUNLINK_MQTT_MESSAGE(gisunlink_mqtt_message *message);

/*! @brief 初始化MQTT接口
 * @param void 
 * @return void
 */
void gisunlink_mqtt_init(char *DeviceHWSn_addr,char *FirmwareVersion);

/*! @brief 连接MQTT服务器
 * @param 连接回调
 * @param 消息回调
 * @return void
 */
void gisunlink_mqtt_connect(GISUNLINK_MQTT_CONNECT *connectCb,GISUNLINK_MQTT_MESSAGE *messageCb); 

/*! @brief 是否已链接MQTT服务器
 * @return void
 */
bool gisunlink_mqtt_isconnected(void);

/*! @brief 订阅主题
 * @param 主题
 * @param 等级
 * @return void
 */
bool gisunlink_mqtt_subscribe(const char *topic,uint8 qos); 

/*! @brief 发布主题
 * @param 主题
 * @param 消息
 * @param 等级 
 * @param 是否需要确认服务器ack反馈 
 * @return bool
 */
bool gisunlink_mqtt_publish(char *topic,const char *payload,uint8 qos, uint32 publish_id, bool ackstatus); 

/*! @brief 返回客户端ID
 * @return void
 */
char *gisunlink_mqtt_clientid(void);

/*! @brief 释放mqtt message
 * @param 指针
 * @return void
 */
void gisunlink_mqtt_free_message(gisunlink_mqtt_message *message);

#ifdef __cplusplus
}
#endif

#endif //__GISUNLINK_MQTT_H__
