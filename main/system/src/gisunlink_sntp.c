/*
* _COPYRIGHT_
*
* File Name: gisunlink_sntp.c
* System Environment: JOHAN-PC
* Created Time:2019-04-10
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "lwip/err.h"
#include "lwip/apps/sntp.h"
//#include "apps/sntp/sntp.h"

#include "gisunlink.h"
#include "gisunlink_atomic.h"
#include "gisunlink_sntp.h"

#define SNTP_SERVICE_TASK_SIZE			2048
#define SNTP_SERVICE_TASK_PRIORITY		6
#define SNTP_SERVICE_RETRY				10
#define SNTP_SERVICE_WAIT_DELAY			2000

typedef struct _gisunlink_sntp_ctrl {
	GISUNLINK_SNTP_RESPOND *respondCb;
	void *param;
} gisunlink_sntp_ctrl;

static gisunlink_sntp_ctrl *gisunlink_sntp = NULL;

static void gisunlink_sntp_init(void) {
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_setservername(0, "pool.ntp.org");
	sntp_setservername(1, "asia.pool.ntp.org");
	sntp_setservername(2, "cn.pool.ntp.org");
	sntp_setservername(3, "ntp.yidianting.xin");
	sntp_init();
}

static void gisunlink_sntp_stop(void) {
	sntp_stop();
}

static int gisunlink_sntp_obtain_time(gisunlink_sntp_ctrl *gisunlink_sntp) {
	time_t now = 0;
	struct tm timeinfo = { 0 };
	int retry = 0;
	const int retry_count = SNTP_SERVICE_RETRY;

	gisunlink_sntp_init();
	while(timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
		//gisunlink_print(GISUNLINK_PRINT_WARN,"waiting for system time to be set... (%d/%d)", retry, retry_count);
		if(gisunlink_sntp && gisunlink_sntp->respondCb) {
			gisunlink_sntp->respondCb(GISUNLINK_SNTP_FAILURE,gisunlink_sntp->param);
		}
		vTaskDelay(SNTP_SERVICE_WAIT_DELAY / portTICK_PERIOD_MS);
		time(&now);
		localtime_r(&now, &timeinfo);
	}

	if(retry == retry_count){
		return -1;
	} else {
		return 0;
	}
}

static void gisunlink_sntp_task(void *param) {
	while(1) {
		time_t now;
		struct tm timeinfo;
		if(gisunlink_sntp) {
			time(&now);
			localtime_r(&now, &timeinfo);
			if (timeinfo.tm_year < (2016 - 1900)) {
				if(gisunlink_sntp_obtain_time(gisunlink_sntp)) {
					if(gisunlink_sntp->respondCb) {
						gisunlink_sntp->respondCb(GISUNLINK_SNTP_FAILURE,gisunlink_sntp->param);
					}
					gisunlink_sntp_stop();
					break;
				} else {
					gisunlink_sntp_stop();
					char strftime_buf[64];
					setenv("TZ", "CST-8", 1); //CST-8
					tzset();
					time(&now);
					localtime_r(&now, &timeinfo);
					strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
					gisunlink_print(GISUNLINK_PRINT_WARN,"the current date/time in Shanghai is: %s", strftime_buf);
					if(gisunlink_sntp->respondCb) {
						gisunlink_sntp->respondCb(GISUNLINK_SNTP_SUCCEED,gisunlink_sntp->param);
					}
					break;
				}
			}

		} else {
			break;
		}
	}

	if(gisunlink_sntp) {
		gisunlink_free(gisunlink_sntp);
		gisunlink_sntp = NULL;
	}
	gisunlink_destroy_task(NULL);
}

void gisunlink_sntp_initialize(GISUNLINK_SNTP_RESPOND *respondCb, void *param) {
	if(gisunlink_sntp != NULL) {
		return;
	}

	gisunlink_sntp = (gisunlink_sntp_ctrl *)gisunlink_malloc(sizeof(gisunlink_sntp_ctrl));

	if(gisunlink_sntp) {
		gisunlink_sntp->respondCb = respondCb;
		gisunlink_sntp->param = param;
		gisunlink_create_task_with_Priority(gisunlink_sntp_task, "sntp_task", NULL, SNTP_SERVICE_TASK_SIZE,7);
	} else {
		respondCb(GISUNLINK_SNTP_FAILURE,gisunlink_sntp->param);	
		gisunlink_sntp = NULL;
	}
	return;
}

int gisunlink_sntp_get_timestamp(void) {
	return time(NULL);
}

uint64_t gisunlink_sntp_get_timestamp_ms(void) {
	struct timeval tv;    
	uint64_t timestamp = 0;
	gettimeofday(&tv,NULL);
	timestamp = (tv.tv_sec * 1000);
	timestamp += tv.tv_usec / 1000;
	return timestamp;
} 
