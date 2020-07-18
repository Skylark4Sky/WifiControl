/*
* _COPYRIGHT_
*
* File Name: gisunlink_atomic.c
* System Environment: JOHAN-PC
* Created Time:2019-01-25
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "gisunlink_atomic.h"
#include "gisunlink_print.h"

void *gisunlink_create_sem(int max_count, int init_count) {
	return xSemaphoreCreateCounting(max_count, init_count);
}

void *gisunlink_create_binary_sem(void) {
	return xSemaphoreCreateBinary();
}

void gisunlink_get_sem(void *handle) {
	xSemaphoreTake(handle, (portTickType)portMAX_DELAY);
}

void gisunlink_put_sem(void *handle) {
	xSemaphoreGive(handle);
}

void gisunlink_destroy_sem(void *handle) {
	return vSemaphoreDelete(handle);
}

void *gisunlink_create_lock(void) {
	return xSemaphoreCreateMutex();
}

void gisunlink_get_lock(void *handle) {
	xSemaphoreTake(handle, (portTickType)portMAX_DELAY);
}

void gisunlink_free_lock(void *handle) {
	xSemaphoreGive(handle);
}

void gisunlink_destroy_lock(void *handle) {
	return vSemaphoreDelete(handle);
}

void *gisunlink_malloc(size_t sz) {
	void *memory = heap_caps_malloc(sz, MALLOC_CAP_8BIT);
	if(memory) {
		memset(memory,0x00,sz);
	}
	return memory;// calloc(1,sz);
}

void gisunlink_free(void *ptr) {
	if(ptr) {
		heap_caps_free(ptr); 
		ptr = (void *)0;
//		free(ptr);ptr = NULL;
	}
	return;
} 

void *gisunlink_malloc_dbg(size_t sz,int line) {
	void *ptr = gisunlink_malloc(sz);

	gisunlink_print(GISUNLINK_PRINT_DEBUG,"Malloc addr:%p size:%d ----------->Line:%d",ptr,sz,line);

	return ptr;
}

void gisunlink_free_dbg(void *ptr) {
	gisunlink_print(GISUNLINK_PRINT_DEBUG,"Free addr:%p",ptr);
	return gisunlink_free(ptr);
} 

void *gisunlink_create_task(GISUNLINK_TASK_CALLBACK func, const char *pname, void *para, int stack_size) {
	void *task_handle = NULL;
	xTaskCreate(func, pname, stack_size, para, configMAX_PRIORITIES, task_handle);
	return task_handle;	
}

void *gisunlink_create_task_with_Priority(GISUNLINK_TASK_CALLBACK func, const char *pname, void *para, int stack_size,int uxPriority) {
	void *task_handle = NULL;
	xTaskCreate(func, pname, stack_size, para, uxPriority, task_handle);
	return task_handle;
}

void gisunlink_task_delay(unsigned int xTicksToDelay) {
	return vTaskDelay(xTicksToDelay);
}

void gisunlink_pause_task(void *handle) {
	return vTaskSuspend(handle);
}

void gisunlink_run_task(void *handle) {
	return vTaskResume(handle);
}

void gisunlink_destroy_task(void *handle) {
	return vTaskDelete(handle);
}

unsigned int gisunlink_get_tick_count(void) {
	return xTaskGetTickCount();
}

unsigned int gisunlink_set_interva_ms(unsigned int time) {
	return (time/portTICK_RATE_MS);
}

