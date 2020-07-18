/*
* _COPYRIGHT_
*
* File Name: gisunlink_ota.c
* System Environment: JOHAN-PC
* Created Time:2019-03-05
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#include "esp_ota_ops.h"
#include "gisunlink_ota.h" 
#include "gisunlink_print.h" 

void gisunlink_ota_init(void) {
	return;
#if 0
     esp_ota_handle_t update_handle = 0 ;
     const esp_partition_t *update_partition = NULL;

     const esp_partition_t *configured = esp_ota_get_boot_partition();
     const esp_partition_t *running = esp_ota_get_running_partition();

     if(configured != running) {
         if(configured && (configured->address && running->address)) {
             gisunlink_print(GISUNLINK_PRINT_ERROR,"Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x", configured->address, running->address);
         }
         gisunlink_print(GISUNLINK_PRINT_ERROR,"(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
         return;
     }

	 gisunlink_print(GISUNLINK_PRINT_ERROR,"Running partition type %d subtype %d (offset 0x%08x)", running->type, running->subtype, running->address);

	 update_partition = esp_ota_get_next_update_partition(NULL);

	 gisunlink_print(GISUNLINK_PRINT_ERROR,"Writing to partition subtype %d at offset 0x%x", update_partition->subtype,update_partition->address);
#endif

}
