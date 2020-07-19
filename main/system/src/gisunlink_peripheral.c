/*
* _COPYRIGHT_
*
* File Name: gisunlink_peripheral.c
* System Environment: JOHAN-PC
* Created Time:2019-01-26
* Author: johan
* E-mail: johaness@qq.com
* Description:串口通信 
*
*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_system.h"

#include "gisunlink.h"
#include "gisunlink_uart.h"
#include "gisunlink_queue.h"
#include "gisunlink_atomic.h"
#include "gisunlink_message.h"
#include "gisunlink_peripheral.h"

#define GISUNLINK_READ_TASK_SIZE			2048	
#define GISUNLINK_WRITE_TASK_SIZE			1024
#define GISUNLINK_RESPOND_TASK_SIZE			2048

#define GISUNLINK_MAX_PACKET_QUEUE			16	
#define GISUNLINK_MAX_PACKET_WAIT_QUEUE		32			
#define GISUNLINK_RETRY_NUM		3	
#define GISUNLINK_RECV_TIMEOUT  1500		//1500ms
#define GISUNLINK_RESPOND_TASK_DELAY 10

#define GISUNLINK_PACKET_HEAD   0XAA
#define GISUNLINK_PACKET_TAIL   0XBB

#define GISUNLINK_PACKET_HEAD_SIZE			1
#define GISUNLINK_PACKET_LEN_SIZE			2
#define GISUNLINK_PACKET_FLOW_SIZE			4
#define GISUNLINK_PACKET_DIR_SIZE			1
#define GISUNLINK_PACKET_CMD_SIZE			1
#define GISUNLINK_PACKET_CHKSUM_SIZE		2
#define GISUNLINK_PACKET_TAIL_SIZE			1

#define GISUNLINK_PACKET_HEAD_TAIL_SIZE		2

#define UART_PACKET_NO_HEAD_AND_TAIL_SIZE (GISUNLINK_PACKET_LEN_SIZE + GISUNLINK_PACKET_FLOW_SIZE + GISUNLINK_PACKET_DIR_SIZE + GISUNLINK_PACKET_CMD_SIZE + GISUNLINK_PACKET_CHKSUM_SIZE)

#define UART_PACKET_BUF_MAX_LEN (512)
#define UART_PACKET_BUF_MIN_LEN	(GISUNLINK_PACKET_HEAD_TAIL_SIZE + UART_PACKET_NO_HEAD_AND_TAIL_SIZE)

#define UART_PACKET_CMD_OFFSET (GISUNLINK_PACKET_HEAD_SIZE + GISUNLINK_PACKET_LEN_SIZE + GISUNLINK_PACKET_FLOW_SIZE + GISUNLINK_PACKET_DIR_SIZE)
#define UART_PACKET_DATA_OFFSET (GISUNLINK_PACKET_HEAD_SIZE + GISUNLINK_PACKET_LEN_SIZE + GISUNLINK_PACKET_FLOW_SIZE + GISUNLINK_PACKET_DIR_SIZE + GISUNLINK_PACKET_CMD_SIZE)

/*
包头一个字节为0xAA；
包长度两个字节；
包流控序号四个字节（4个字节的随机数，如 回复包该字段 直接拷贝请求包流控序号）;
命令类型一个字节（0x00为请求消息、0x01为回复消息）；
命令数据；
校验和;
包尾一个字节为0xBB；
*/

enum {
    GISUNLINK_RECV_HEAD,
    GISUNLINK_RECV_LEN,
	GISUNLINK_RECV_FLOW,
	GISUNLINK_RECV_DIR,
    GISUNLINK_RECV_CMD,
    GISUNLINK_RECV_DATA,
	GISUNLINK_RECV_CHECK_SUM,
    GISUNLINK_RECV_TAIL,
};

typedef enum {
	GISUNLINK_COMM_REQ,
	GISUNLINK_COMM_RES,
} COMM_TYPE;

typedef struct _gisunlink_raw_packet {
	uint16 len;
	uint8 *buf;
} gisunlink_raw_packet;

typedef struct _gisunlink_packet {
	uint8 cmd;
	uint8 retry;
	uint8 behavior;
	uint32 id;
	uint32 heading;
	uint32 time;
	gisunlink_raw_packet *raw;
	gisunlink_raw_packet *respond;
	void *wait_sem;
	GISUNLINK_RESPOND_CALLBACK *respondCb;
} gisunlink_packet;

typedef struct _gisunlink_uart_read_ctrl {
    uint8 mode;
    uint16 len;
	uint32 flow_id;
	uint32 req_id;
	uint8 dir;
    uint8 cmd;
    uint8 data[UART_PACKET_BUF_MAX_LEN];
	uint16 data_offset;
	uint16 data_len;
	uint16 chk_sum;
} gisunlink_uart_read_ctrl;

typedef struct _gisunlink_uart_write_ctrl {
	uint32 flow_id;
	void *queue;
	void *ack_queue;
	void *respond_queue;
	void *thread;
	void *ack_thread;
	void *respond_thread;
	void *thread_sem;
	void *ack_thread_sem;
	void *respond_thread_sem;
} gisunlink_uart_write_ctrl;

gisunlink_uart_read_ctrl *gisunlink_uart_read = NULL;
gisunlink_uart_write_ctrl *gisunlink_uart_write = NULL;

static uint16 gisunlink_peripheral_check_sum(uint8 *data,uint16 data_len) {
	uint32 check_sum = 0;
	while(data_len--) {
		check_sum += *data++;
	}
	while (check_sum >> 16) {
		check_sum = (check_sum >> 16) + (check_sum & 0xffff);
	}
	return (uint16)(~check_sum);
}

static gisunlink_raw_packet *gisunlink_peripheral_create_raw_packet(uint32 flow_id, uint8 flow_dir, uint8 cmd,uint8 *data,uint16 data_len) {
	gisunlink_raw_packet *raw = NULL;
	uint8 *data_offset = NULL;
	uint16 raw_size = UART_PACKET_BUF_MIN_LEN + data_len;

	if(raw_size > UART_PACKET_BUF_MAX_LEN) {
		return NULL;
	}

	raw = (gisunlink_raw_packet *)gisunlink_malloc(sizeof(gisunlink_raw_packet));

	raw->len = raw_size;
	data_offset = (raw->buf = (uint8 *)gisunlink_malloc(raw->len));

	*(data_offset++) = GISUNLINK_PACKET_HEAD;

	*(data_offset++) = ((raw_size - GISUNLINK_PACKET_HEAD_TAIL_SIZE) & 0xFF);
	*(data_offset++) = (((raw_size - GISUNLINK_PACKET_HEAD_TAIL_SIZE) >> 8) & 0xFF);

	*(data_offset++) = (flow_id & 0xFF);
	*(data_offset++) = ((flow_id >> 8) & 0xFF);
	*(data_offset++) = ((flow_id >> 16) & 0xFF);
	*(data_offset++) = ((flow_id >> 24) & 0xFF);

	*(data_offset++) = flow_dir;
	*(data_offset++) = cmd;

	if(data_len) {
		memcpy(data_offset,data,data_len);
		data_offset += data_len;
	} 

	uint8 *chk_buf = raw->buf + GISUNLINK_PACKET_HEAD_SIZE;
	uint16 chk_len = raw->len - GISUNLINK_PACKET_HEAD_TAIL_SIZE - GISUNLINK_PACKET_CHKSUM_SIZE;
	uint16 chk_sum = gisunlink_peripheral_check_sum(chk_buf,chk_len);
	*(data_offset++) = (chk_sum & 0xFF);
	*(data_offset++) = ((chk_sum >> 8) & 0xFF);

	*(data_offset) = GISUNLINK_PACKET_TAIL; 
	return raw;
}

static void gisunlink_peripheral_free_raw_packet(gisunlink_raw_packet *raw) {
	if(raw) {
		if(raw->buf) {
			gisunlink_free(raw->buf);
			raw->buf = NULL;
			raw->len = 0;
		}
		gisunlink_free(raw);
		raw = NULL;
	}
	return;
}

static gisunlink_packet *gisunlink_peripheral_create_packet(gisunlink_raw_packet *raw,gisunlink_peripheral_message *message) {
	gisunlink_packet *packet = NULL;

	if(raw)  {
		packet = (gisunlink_packet *)gisunlink_malloc(sizeof(gisunlink_packet));
		packet->retry = GISUNLINK_RETRY_NUM;
		packet->id = message->id;
		packet->cmd = message->cmd;
		packet->time = gisunlink_get_tick_count();
		packet->raw = raw;
		packet->cmd = message->cmd;
		packet->heading = message->heading;
		packet->behavior = message->behavior;
		packet->respondCb = message->respondCb;
		//如果没有带回调函数则是同步等待模式
		if(packet->respondCb == NULL) {
			packet->wait_sem = gisunlink_create_binary_sem();
		}
	}
	return packet;
}

static void gisunlink_peripheral_free_packet(gisunlink_packet *packet) {
	if(packet) {
		if(packet->raw) {
			gisunlink_peripheral_free_raw_packet(packet->raw);
		}

		if(packet->respond) {
			gisunlink_peripheral_free_raw_packet(packet->respond);
		}

		if(packet->wait_sem) {
			gisunlink_destroy_sem(packet->wait_sem);
		}

		gisunlink_free(packet);
		packet = NULL;
	}
	return;
}

static int gisunlink_peripheral_matching_package(void *src, void *item) {
	uint32 *src_data = (uint32 *)src;
	gisunlink_packet *data_item = (gisunlink_packet *)item;
	if(*src_data == data_item->id) {
		return 0;
	}
	return 1;
}

static int gisunlink_peripheral_matching_package_timeout(void *src, void *item) {
	uint32 *src_data = (uint32 *)src;
	gisunlink_packet *packet = (gisunlink_packet *)item;
	///aws_iot/port/timer.c
	if((*src_data - packet->time) >= GISUNLINK_RECV_TIMEOUT/portTICK_PERIOD_MS) {
		return 0;
	} else {
		return 1;
	}
}

static uint8 gisunlink_peripheral_message_callback(gisunlink_message *message) {
	if(message) {
	}
	return true;
}

static void gisunlink_peripheral_event_message_free(void *msg_data) {
	if(msg_data) {
		gisunlink_message *message = (gisunlink_message *)msg_data;
		gisunlink_uart_event *event = NULL;
		if(message->data_len && message->data) {
			event = (gisunlink_uart_event *)message->data;
			if(event) {
				if(event->data) {
					gisunlink_free(event->data);
					event->data = NULL;
				}
				gisunlink_free(event);
				event = NULL;
			}
		}
		gisunlink_free(message);
		message = NULL;
	}
}

static void gisunlink_peripheral_package_message(gisunlink_message *message,gisunlink_uart_event *event) {
	message->type = ASYNCMSG;
	message->src_id = GISUNLINK_PERIPHERAL_MODULE; 
	message->dst_id = GISUNLINK_MAIN_MODULE;
	message->message_id = GISUNLINK_PERIPHERAL_UART_MSG;
	message->data_len = sizeof(gisunlink_uart_event);
	message->data = (void *)event;
	message->malloc = NULL;
	message->free = gisunlink_peripheral_event_message_free; 
	message->callback = NULL;
}

static void gisunlink_peripheral_post_message(uint32 flow_id, uint8 cmd, uint8 *data, uint16_t data_len) {
	gisunlink_message message = {0}; 
	gisunlink_uart_event *event = (gisunlink_uart_event *)gisunlink_malloc(sizeof(gisunlink_uart_event));

	event->cmd = cmd;
	event->data_len = data_len;
	event->flow_id = flow_id;

	if(event->data_len) {
		event->data = (uint8 *)gisunlink_malloc(event->data_len);
		memcpy(event->data,data,data_len);
	}
	gisunlink_peripheral_package_message(&message,event);
	gisunlink_message_push(&message);
}

static void gisunlink_peripheral_process_uart_packet(gisunlink_uart_read_ctrl *uart_recv) {
	if(uart_recv) {
		//如果是请求包马上回复ack
		if(uart_recv->dir == GISUNLINK_COMM_REQ) {
			gisunlink_peripheral_respond(uart_recv->flow_id,uart_recv->cmd,NULL,0); 
		}
		uint16 check_sum = gisunlink_peripheral_check_sum(uart_recv->data + GISUNLINK_PACKET_HEAD_SIZE,uart_recv->len - GISUNLINK_PACKET_HEAD_TAIL_SIZE - GISUNLINK_PACKET_CHKSUM_SIZE);
		uint32 packet_id = uart_recv->flow_id;
		if(check_sum == uart_recv->chk_sum) {
			if(uart_recv->dir == GISUNLINK_COMM_RES) { //回复包
				//取需等待的回复包
				gisunlink_packet *packet = gisunlink_queue_pop_cmp(gisunlink_uart_write->respond_queue, (void *)&packet_id, gisunlink_peripheral_matching_package);
				if(packet) {
					uint16_t has_content = uart_recv->len - UART_PACKET_BUF_MIN_LEN;
					//有回复的内容
					if(has_content) {
						packet->respond = (gisunlink_raw_packet *)gisunlink_malloc(sizeof(gisunlink_raw_packet));
						packet->respond->len = has_content;
						packet->respond->buf = (uint8 *)gisunlink_malloc(packet->respond->len); 
						memcpy(packet->respond->buf, uart_recv->data + UART_PACKET_DATA_OFFSET ,packet->respond->len);
					}
					//异步回复的包
					if(packet->respondCb) {
						gisunlink_respond_message respond = {
							.reason = GISUNLINK_SEND_SUCCEED,
							.cmd = packet->cmd,
							.behavior = packet->behavior,
							.heading = packet->heading,
							.id = packet->id,
						};
						if(has_content) {
							respond.data_len = packet->respond->len;
							respond.data = packet->respond->buf;
						}
						packet->respondCb(&respond);
						gisunlink_peripheral_free_packet(packet);
					} else if(packet->respondCb == NULL && packet->wait_sem) { //同步等待回复的包
						//发送信号
						gisunlink_put_sem(packet->wait_sem);
					} else {
						//异常
						gisunlink_peripheral_free_packet(packet);
					}
				}
			} else if(uart_recv->dir == GISUNLINK_COMM_REQ && uart_recv->flow_id != uart_recv->req_id) { //请求包
				uart_recv->req_id = uart_recv->flow_id;
				uint8 cmd = *(uart_recv->data + UART_PACKET_CMD_OFFSET);
				uint8 *data = uart_recv->data + UART_PACKET_DATA_OFFSET;
//				uint16 len = uart_recv->len - UART_PACKET_BUF_MIN_LEN;
				gisunlink_peripheral_post_message(packet_id,cmd,data,uart_recv->data_len);
			}
		}
	}
}

static void gisunlink_peripheral_uart_read(void *param) {
	uint8 *buffer = NULL;
	gisunlink_uart_read_ctrl *uart_recv = (gisunlink_uart_read_ctrl *)param;
	uart_recv->data_offset = 0;
	while(1) {
		buffer = uart_recv->data + uart_recv->data_offset; 
		switch(uart_recv->mode) {
			case GISUNLINK_RECV_HEAD:
				gisunlink_uart_recv_data(buffer, GISUNLINK_PACKET_HEAD_SIZE);
				if(buffer[0] == GISUNLINK_PACKET_HEAD) {
					uart_recv->data_offset += GISUNLINK_PACKET_HEAD_SIZE;
					uart_recv->mode = GISUNLINK_RECV_LEN;
				} else {
					uart_recv->data_offset = 0;
					uart_recv->mode = GISUNLINK_RECV_HEAD;
				}
				break;
			case GISUNLINK_RECV_LEN:
				gisunlink_uart_recv_data(buffer, GISUNLINK_PACKET_LEN_SIZE);
				uart_recv->len = buffer[0] + (buffer[1] << 8);
				uart_recv->len += GISUNLINK_PACKET_HEAD_TAIL_SIZE;
				if(uart_recv->len > UART_PACKET_BUF_MAX_LEN || uart_recv->len < UART_PACKET_BUF_MIN_LEN) {
					uart_recv->data_offset = 0;
					uart_recv->mode = GISUNLINK_RECV_HEAD;
				} else {
					uart_recv->data_offset += GISUNLINK_PACKET_LEN_SIZE;
					uart_recv->mode = GISUNLINK_RECV_FLOW;
				}
				break;
			case GISUNLINK_RECV_FLOW: 
				gisunlink_uart_recv_data(buffer, GISUNLINK_PACKET_FLOW_SIZE);
				uart_recv->flow_id = buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24);
				uart_recv->data_offset += GISUNLINK_PACKET_FLOW_SIZE;
				uart_recv->mode = GISUNLINK_RECV_DIR;
				break;
			case GISUNLINK_RECV_DIR:
				gisunlink_uart_recv_data(buffer, GISUNLINK_PACKET_DIR_SIZE);
				uart_recv->dir = buffer[0];
				if(uart_recv->dir == GISUNLINK_COMM_REQ || uart_recv->dir == GISUNLINK_COMM_RES) {
					uart_recv->data_offset += GISUNLINK_PACKET_DIR_SIZE;
					uart_recv->mode = GISUNLINK_RECV_CMD;
				} else {
					uart_recv->data_offset = 0;
					uart_recv->mode = GISUNLINK_RECV_HEAD;
				}
				break;
			case GISUNLINK_RECV_CMD:			
				gisunlink_uart_recv_data(buffer, GISUNLINK_PACKET_CMD_SIZE);
				uint8 cmd = buffer[0];
				switch(cmd) {
					case GISUNLINK_NETWORK_STATUS:
					case GISUNLINK_NETWORK_RESET:
					case GISUNLINK_NETWORK_RSSI:

					case GISUNLINK_DEV_FW_INFO:
					case GISUNLINK_DEV_FW_TRANS:
					case GISUNLINK_DEV_FW_READY:
					case GISUNLINK_DEV_SN:

					case GISUNLINK_TASK_CONTROL:
					case GISUNLINK_HW_SN:
					case GISUNLINK_FIRMWARE_VERSION:
						uart_recv->cmd = cmd;
						uart_recv->data_offset += GISUNLINK_PACKET_CMD_SIZE;
						uart_recv->mode = GISUNLINK_RECV_DATA;
						break;
					default:
				//		gisunlink_print(GISUNLINK_PRINT_ERROR,"ERROR cmd %d\n", cmd);
						uart_recv->data_offset = 0;
						uart_recv->mode = GISUNLINK_RECV_HEAD;
				}
				break;
			case GISUNLINK_RECV_DATA:
				uart_recv->data_len = uart_recv->len - UART_PACKET_BUF_MIN_LEN;
				if(uart_recv->data_len) {
					gisunlink_uart_recv_data(buffer, uart_recv->data_len);
				}
				uart_recv->data_offset += uart_recv->data_len;
				uart_recv->mode = GISUNLINK_RECV_CHECK_SUM;
				break;
			case GISUNLINK_RECV_CHECK_SUM:
				gisunlink_uart_recv_data(buffer, GISUNLINK_PACKET_CHKSUM_SIZE);
				uart_recv->chk_sum = buffer[0] + (buffer[1] << 8); 
				uart_recv->data_offset += GISUNLINK_PACKET_CHKSUM_SIZE;
				uart_recv->mode = GISUNLINK_RECV_TAIL;
				break;
			case GISUNLINK_RECV_TAIL:
				gisunlink_uart_recv_data(buffer, GISUNLINK_PACKET_TAIL_SIZE);
				if(buffer[0] == GISUNLINK_PACKET_TAIL) {
					gisunlink_peripheral_process_uart_packet(uart_recv);
				}
				uart_recv->data_offset = 0;
				uart_recv->mode = GISUNLINK_RECV_HEAD;
				break;
		}
	}
}

//写队列
static void gisunlink_peripheral_uart_write(void *param) {
	gisunlink_raw_packet *raw = NULL;
	gisunlink_uart_write_ctrl *uart_send = (gisunlink_uart_write_ctrl *)param;
	while(1) {
		gisunlink_get_sem(uart_send->thread_sem);
		while(gisunlink_queue_not_empty(uart_send->queue)) {
			raw = (gisunlink_raw_packet *)gisunlink_queue_pop(uart_send->queue);
			if(raw) {
				gisunlink_uart_send_data(raw->buf,raw->len);
				gisunlink_peripheral_free_raw_packet(raw);
				raw = NULL;
			}
		}
	}
}

static gisunlink_raw_packet *gisunlink_peripheral_copy_raw_from_packet(gisunlink_packet *packet) {
	gisunlink_raw_packet *raw = NULL;
	if(packet) {
		raw = (gisunlink_raw_packet *)gisunlink_malloc(sizeof(gisunlink_raw_packet));
		if(raw) {
			raw->len = packet->raw->len;
			raw->buf = (uint8 *)gisunlink_malloc(raw->len);
			if(raw->buf) {
				memcpy(raw->buf,packet->raw->buf,packet->raw->len);
			} else {
				gisunlink_free(raw);
				raw = NULL;
			}
		}
	}
	return raw;
}

static void gisunlink_peripheral_uart_write_ack(void *param) {
	gisunlink_packet *packet = NULL;
	gisunlink_uart_write_ctrl *uart_send = (gisunlink_uart_write_ctrl *)param;
	while(1) {
		gisunlink_get_sem(uart_send->ack_thread_sem);
		while(gisunlink_queue_not_empty(uart_send->ack_queue)) {
			packet = (gisunlink_packet *)gisunlink_queue_pop(uart_send->ack_queue);
			if(packet) {
				//等待回复队列空闲
				while(gisunlink_queue_is_full(uart_send->respond_queue)) {
					//空出cpu
					gisunlink_task_delay(GISUNLINK_RESPOND_TASK_DELAY / portTICK_PERIOD_MS);
				}

				gisunlink_raw_packet *raw = gisunlink_peripheral_copy_raw_from_packet(packet);
				//构建裸数据并压入写队列
				if(raw) {
					packet->time = gisunlink_get_tick_count();
					packet->retry--;
					if(gisunlink_queue_push(gisunlink_uart_write->queue, raw)) {
						gisunlink_put_sem(gisunlink_uart_write->thread_sem);
					} 
				}

				//压入等待回复队列
				gisunlink_queue_push(uart_send->respond_queue,packet);
				gisunlink_put_sem(uart_send->respond_thread_sem);
			}
		}
	}
}

static void gisunlink_peripheral_uart_write_respond(void *param) {
	gisunlink_packet *packet = NULL;
	gisunlink_uart_write_ctrl *uart_send = (gisunlink_uart_write_ctrl *)param;
	while(1) {
		gisunlink_get_sem(uart_send->respond_thread_sem);
		while(gisunlink_queue_not_empty(uart_send->respond_queue)) {
			uint16 wait_responds = gisunlink_queue_items(uart_send->respond_queue);
			gisunlink_queue_reset(uart_send->respond_queue);
			for(int i = 0; i < wait_responds; i++) {
				uint32 time = gisunlink_get_tick_count(); 
				gisunlink_queue_lock(uart_send->respond_queue);
				packet = gisunlink_queue_get_next(uart_send->respond_queue, (void *)&time, gisunlink_peripheral_matching_package_timeout);
				if(packet && packet->retry) {
					gisunlink_raw_packet *raw = gisunlink_peripheral_copy_raw_from_packet(packet);
					if(raw) {
						packet->time = gisunlink_get_tick_count();
						packet->retry--;
						//重新压入写队列
						if(gisunlink_queue_push(gisunlink_uart_write->queue, raw)) {
							gisunlink_put_sem(gisunlink_uart_write->thread_sem);
						} 
					}
				}
				gisunlink_queue_unlock(uart_send->respond_queue);
				if(packet && packet->retry == 0) {  //超时了 则从等待队列弹出
					packet = gisunlink_queue_pop_item(uart_send->respond_queue,packet);
					if(packet) {
						//检查是否为异步等待
						if(packet->respondCb) {
							//构建回复包
							gisunlink_respond_message respond = {
								.reason = GISUNLINK_SEND_TIMEOUT,
								.cmd = packet->cmd,
								.behavior = packet->behavior,
								.heading = packet->heading,
								.id = packet->id,
							};
							packet->respondCb(&respond);
							gisunlink_peripheral_free_packet(packet);
						} else if(packet->respondCb == NULL && packet->wait_sem) { //同步则发送信号给相关调用线程
							gisunlink_put_sem(packet->wait_sem);
						} else {
							gisunlink_peripheral_free_packet(packet);
						} 
					}
				}
			}
			gisunlink_task_delay(GISUNLINK_RESPOND_TASK_DELAY / portTICK_PERIOD_MS);
		}
	}
}

void gisunlink_peripheral_init(void) {
	if(gisunlink_uart_read || gisunlink_uart_write) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"[%s:%d] the model had been init\n",__func__, __LINE__);
		return;
	}

	gisunlink_uart_read = (gisunlink_uart_read_ctrl *)gisunlink_malloc(sizeof(gisunlink_uart_read_ctrl));
	gisunlink_uart_write = (gisunlink_uart_write_ctrl *)gisunlink_malloc(sizeof(gisunlink_uart_write_ctrl));

	if(gisunlink_uart_read == NULL || gisunlink_uart_write == NULL) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"[%s:%d] it fails to malloc\n",__func__, __LINE__);
		return;
	}

	gisunlink_uart_read->req_id = 0;
	gisunlink_uart_write->flow_id = 0;
	gisunlink_uart_write->queue = gisunlink_queue_init(GISUNLINK_MAX_PACKET_QUEUE); 
	gisunlink_uart_write->ack_queue = gisunlink_queue_init(GISUNLINK_MAX_PACKET_QUEUE);
	gisunlink_uart_write->respond_queue = gisunlink_queue_init(GISUNLINK_MAX_PACKET_WAIT_QUEUE); 

	gisunlink_uart_write->thread_sem = gisunlink_create_sem(GISUNLINK_MAX_PACKET_QUEUE,0);
	gisunlink_uart_write->ack_thread_sem = gisunlink_create_sem(GISUNLINK_MAX_PACKET_QUEUE,0);
	gisunlink_uart_write->respond_thread_sem = gisunlink_create_sem(GISUNLINK_MAX_PACKET_WAIT_QUEUE,0);

	//初始化串口
	gisunlink_uart_init();
	//注册消息传递
	gisunlink_message_register(GISUNLINK_PERIPHERAL_MODULE,gisunlink_peripheral_message_callback);
	//注册写队列任务
	gisunlink_uart_write->thread = gisunlink_create_task(gisunlink_peripheral_uart_write, "uart_write", gisunlink_uart_write, GISUNLINK_WRITE_TASK_SIZE);
	//注册写确认队列任务
	gisunlink_uart_write->ack_thread = gisunlink_create_task(gisunlink_peripheral_uart_write_ack, "uart_write_ack", gisunlink_uart_write, GISUNLINK_WRITE_TASK_SIZE);
	//注册写回复队列任务
	gisunlink_uart_write->respond_thread = gisunlink_create_task(gisunlink_peripheral_uart_write_respond, "uart_respond", gisunlink_uart_write, GISUNLINK_RESPOND_TASK_SIZE);
	//注册读任务
	gisunlink_create_task(gisunlink_peripheral_uart_read, "uart_read", gisunlink_uart_read, GISUNLINK_READ_TASK_SIZE);
}

gisunlink_respond_message *gisunlink_peripheral_send_message(gisunlink_peripheral_message *message) {
	gisunlink_respond_message *respond = NULL;
	if(gisunlink_uart_write && message) {
		message->id = gisunlink_uart_write->flow_id++;
		gisunlink_raw_packet *raw_packet = gisunlink_peripheral_create_raw_packet(message->id,GISUNLINK_COMM_REQ,message->cmd,message->data,message->data_len);
		if(raw_packet) {
			if(UART_RESPOND == message->respond) { //需要回复 
				gisunlink_packet *packet = gisunlink_peripheral_create_packet(raw_packet,message);
				if(packet->respondCb) { //异步回复
					if(gisunlink_queue_push(gisunlink_uart_write->ack_queue, packet)) { //压入队列 
						gisunlink_put_sem(gisunlink_uart_write->ack_thread_sem);
					} else { //如果压入失败通知上层
						if(message->respondCb) {
							gisunlink_respond_message respond = {
								.reason = GISUNLINK_SEND_BUSY,
								.cmd = message->cmd,
								.behavior = message->behavior,
								.heading = message->heading,
								.id = message->id,
								.data = NULL,
								.data_len = 0
							};
							packet->respondCb(&respond);
						}
						gisunlink_peripheral_free_packet(packet);
					}
				} else { //同步回复
					respond = (gisunlink_respond_message *)gisunlink_malloc(sizeof(gisunlink_respond_message));
					respond->id = packet->id;
					respond->cmd = packet->cmd;
					respond->heading = packet->heading; 
					respond->behavior = packet->behavior;

					if(packet->wait_sem == NULL) {
						respond->reason = GISUNLINK_SEM_ERROR;
						gisunlink_peripheral_free_packet(packet);
						return respond;
					}

					//压入ack处理队列
					if(gisunlink_queue_push(gisunlink_uart_write->ack_queue, packet)) {
						gisunlink_put_sem(gisunlink_uart_write->ack_thread_sem);
						gisunlink_get_sem(packet->wait_sem);
						//取到信号量 将压进 等待回复队列 的包 弹出
						if(packet) {
							//timeout
							if(packet->retry == 0) {
//								gisunlink_print(GISUNLINK_PRINT_ERROR,"queue:%d ack_queue:%d respond_queue:%d",gisunlink_queue_items(gisunlink_uart_write->ack_queue),gisunlink_queue_items(gisunlink_uart_write->queue),gisunlink_queue_items(gisunlink_uart_write->respond_queue));
								respond->reason = GISUNLINK_SEND_TIMEOUT;
							} else {
								respond->reason = GISUNLINK_SEND_SUCCEED;
								if(packet->respond && packet->respond->len) {
									respond->data_len = packet->respond->len;
									respond->data = (uint8 *)gisunlink_malloc(respond->data_len);
									memcpy(respond->data,packet->respond->buf,respond->data_len);
								}
							}
						} 
					} else { //如果压入失败通知上层
						respond->reason = GISUNLINK_SEND_BUSY;
					}

					if(packet) {
						gisunlink_peripheral_free_packet(packet);
					} 
					return respond;
				} 
			} else { //不需要回复
				if(gisunlink_queue_push(gisunlink_uart_write->queue, raw_packet)) {
					gisunlink_put_sem(gisunlink_uart_write->thread_sem);
				} 
			}
		}
	}
	return respond;
}

bool gisunlink_peripheral_respond(uint32 flow_id,uint8 cmd,uint8 *data,uint16 data_len) {
	gisunlink_raw_packet *raw_packet = NULL;
	if(gisunlink_uart_write) {
		raw_packet = gisunlink_peripheral_create_raw_packet(flow_id,GISUNLINK_COMM_RES,cmd,data,data_len);
		if(raw_packet) {
			if(gisunlink_queue_push(gisunlink_uart_write->queue, raw_packet)) {
				gisunlink_put_sem(gisunlink_uart_write->thread_sem);
			} else {
				gisunlink_peripheral_free_raw_packet(raw_packet);
				return false;
			}
		} 
		return true;
	}
	return false;
}
