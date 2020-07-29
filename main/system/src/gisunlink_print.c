/*
* _COPYRIGHT_
*
* File Name: gisunlink_print.c
* System Environment: JOHAN-PC
* Created Time:2019-01-22
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "gisunlink_print.h"

#define GISUNLINK_TAG "GISUNLINK_LOG"
#define LOG_BUF_MAX 768

void gisunlink_print(PRINTMODE mode,int8* fmt, ...) {
    char buf[LOG_BUF_MAX];
    va_list ap;
    memset(buf, 0, sizeof(buf));
    va_start(ap, fmt);
    vsnprintf(buf, LOG_BUF_MAX, fmt, ap);
    if(strlen(buf) >= LOG_BUF_MAX) {
        ESP_LOGE(GISUNLINK_TAG, "*****************buf overflow**********************\n");
    }
    buf[LOG_BUF_MAX - 1] = 0;
    va_end(ap);

	switch(mode) {
		case GISUNLINK_PRINT_INFO:
			{
				ESP_LOGI(GISUNLINK_TAG,"%s", buf);
				break;
			}
		case GISUNLINK_PRINT_WARN:
			{
				ESP_LOGW(GISUNLINK_TAG,"%s", buf);
				break;
			}
		case GISUNLINK_PRINT_DEBUG:
			{
				//ESP_LOGD(GISUNLINK_TAG,"%s", buf);
				ESP_LOGE(GISUNLINK_TAG,"%s", buf);
				break;
			}
		case GISUNLINK_PRINT_ERROR:
			{
				ESP_LOGE(GISUNLINK_TAG,"%s", buf);
				break;
			}
	}
}

void gisunlink_printCb(char *fmt, ...) {
	return gisunlink_print(GISUNLINK_PRINT_ERROR,fmt);
}

void gisunlink_print_byteaddr(uint8 *byteAddr, uint32 byteLen) {
//	return esp_log_buffer_hex(GISUNLINK_TAG, byteAddr, byteLen);
//	esp_log_buffer_hex_internal(GISUNLINK_TAG, byteAddr, byteLen,1);
}
