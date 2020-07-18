/*
* _COPYRIGHT_
*
* File Name: gisunlink_uart.c
* System Environment: JOHAN-PC
* Created Time:2019-01-27
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/uart.h"
#include "gisunlink_uart.h"

#define  MAX_DELAY		(portTickType)portMAX_DELAY

static const int RX_BUF_SIZE = 512;
static const int TX_BUF_SIZE = 512;

int gisunlink_uart_send_data(uint8 *data, int len) {
	int tx_bytes = 0;
	tx_bytes= uart_write_bytes(UART_NUM_0, (const char *)data, len);
	return tx_bytes;
}

int gisunlink_uart_recv_data(uint8 *buffer, int count) {
	int rx_bytes = 0;
	rx_bytes = uart_read_bytes(UART_NUM_0, buffer, count, MAX_DELAY); // 没有数据一直阻塞
	return rx_bytes;
}

// 9600
void gisunlink_uart_init(void) {
	uart_config_t uart_config = {
		.baud_rate = 115200,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE
	};

	uart_param_config(UART_NUM_0, &uart_config);
	uart_driver_install(UART_NUM_0, RX_BUF_SIZE, TX_BUF_SIZE, 0, NULL,0);

}
