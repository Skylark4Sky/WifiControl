/*
* _COPYRIGHT_
*
* File Name: gisunlink_mountDisk.c
* System Environment: JOHAN-PC
* Created Time:2019-01-22
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#include "esp_spiffs.h"
#include "gisunlink_print.h"
#include "gisunlink_mountDisk.h"

bool gisunlink_mount_disk(void) { 

	esp_vfs_spiffs_conf_t conf = {
		.base_path = "/spiffs",
		.partition_label = NULL,
		.max_files = 5,
		.format_if_mount_failed = true
	};	

	esp_err_t ret = esp_vfs_spiffs_register(&conf);

	if (ret != ESP_OK) {
		if (ret == ESP_FAIL) {
			gisunlink_print(GISUNLINK_PRINT_ERROR, "Failed to mount or format filesystem");
		} else if (ret == ESP_ERR_NOT_FOUND) {
			gisunlink_print(GISUNLINK_PRINT_ERROR, "Failed to find SPIFFS partition");
		} else {
			gisunlink_print(GISUNLINK_PRINT_ERROR, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
		}
		return false;
	}

	size_t total = 0, used = 0;
	ret = esp_spiffs_info(NULL, &total, &used);
	if (ret != ESP_OK) {
		gisunlink_print(GISUNLINK_PRINT_ERROR, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
		return false;
	} else {
		gisunlink_print(GISUNLINK_PRINT_WARN, "Partition size: total: %d, used: %d", total, used);
	}

	return true;
}

void gisunlink_unmount_disk(void) {
	esp_vfs_spiffs_unregister(NULL);
}

