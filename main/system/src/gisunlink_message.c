/*
* _COPYRIGHT_
*
* File Name: gisunlink_message.c
* System Environment: JOHAN-PC
* Created Time:2019-01-23
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#include "gisunlink.h"
#include "gisunlink_queue.h"
#include "gisunlink_atomic.h"
#include "gisunlink_message.h"

#define MAX_MSG_SIZE	20

typedef struct _gisunlink_module {
    uint8 id;
    GISUNLINK_MESSAGE_CALLBACK *pcallback;
    struct _gisunlink_module *next;
} gisunlink_module;

typedef struct _gisunlink_module_ctrl {
    uint32 module_flag;
    void *msg_queue;
    void *thread_id;
    gisunlink_module *link;
    void *async_notice;
    void *msg_mutex;
} gisunlink_module_ctrl;

static gisunlink_module_ctrl *modules  = NULL;

void *gisunlink_message_malloc(void *msg_data) {
    gisunlink_message *msg_info = NULL;

    msg_info = gisunlink_malloc(sizeof(gisunlink_message));
    if(msg_info == NULL) {
        return NULL;
    }
    memcpy(msg_info, msg_data, sizeof(gisunlink_message));
    return msg_info;
}

void *gisunlink_message_free(void *msg_data) {
    gisunlink_message *msg_info = (gisunlink_message *)msg_data;

    if(msg_info->data) {
        gisunlink_free(msg_info->data);
        msg_info->data = NULL;
    }

    gisunlink_free(msg_info);
    msg_info = NULL;
    return NULL;
}

static void gisunlink_message_async_main(void *param) {
	gisunlink_message *pmsg_info = NULL;
	gisunlink_module *pmodel_link = NULL;
	gisunlink_module_ctrl *module_list = (gisunlink_module_ctrl *)param;

	while(1) {
		gisunlink_get_sem(module_list->async_notice);
		while(gisunlink_queue_not_empty(module_list->msg_queue)) {
			pmsg_info = (gisunlink_message *)gisunlink_queue_pop(module_list->msg_queue);
			if(pmsg_info == NULL) {
				break;
			}

			gisunlink_get_lock(module_list->msg_mutex);
			if(module_list->module_flag & (1 << pmsg_info->dst_id)) {
				pmodel_link = module_list->link;
				while(pmodel_link) {
					if(pmodel_link->id == pmsg_info->dst_id) {
						break;
					}
					pmodel_link = pmodel_link->next;
				}
			}
			gisunlink_free_lock(module_list->msg_mutex);
			if(pmodel_link) {
				pmodel_link->pcallback(pmsg_info);
			}

			if(pmsg_info->free) {
				pmsg_info->free(pmsg_info);
			} else {
				gisunlink_message_free(pmsg_info);
			}
		}
	}
}

void gisunlink_message_init(void) {
	if(modules) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"[%s:%d] the model had been init\n",__func__, __LINE__);
		return;
	}

	modules = (gisunlink_module_ctrl *)gisunlink_malloc(sizeof(gisunlink_module_ctrl));

	if(modules == NULL) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"[%s:%d] it fails to malloc\n",__func__, __LINE__);
		return;
	}

	memset(modules, 0, sizeof(gisunlink_module_ctrl));
	modules->msg_mutex = gisunlink_create_lock(); 
	modules->msg_queue = gisunlink_queue_init(MAX_MSG_SIZE);
	modules->async_notice = gisunlink_create_sem(MAX_MSG_SIZE,0);
	modules->thread_id = gisunlink_create_task(gisunlink_message_async_main, "message_task", modules, 2048);
	return;	
}

void gisunlink_message_register(uint8 model_id, GISUNLINK_MESSAGE_CALLBACK *pModelCall) {
	gisunlink_module *pmodel_link = NULL, *new_model = NULL;

	if(modules == NULL) {
		return ;
	}

	gisunlink_get_lock(modules->msg_mutex);
	if(0 == (modules->module_flag & (1 << model_id))) {
		pmodel_link = modules->link;
		new_model = (gisunlink_module *)gisunlink_malloc(sizeof(gisunlink_module));
		if(new_model) {
			new_model->id = model_id;
			new_model->pcallback = pModelCall;
			new_model->next = NULL;
			if(pmodel_link) {
				while(pmodel_link->next) {
					pmodel_link = pmodel_link->next;
				}
				pmodel_link->next = new_model;
			} else {
				modules->link = new_model;
			}
		}
		modules->module_flag |= (1 << model_id);

	}
	gisunlink_free_lock(modules->msg_mutex);

}

void gisunlink_message_unregister(uint8 model_id) {
    gisunlink_module *pre_item = NULL, *pmodel_link = NULL;

    if(modules == NULL) {
        return ;
    }

	gisunlink_get_lock(modules->msg_mutex);

    if(modules->module_flag & (1 << model_id)) {
        pmodel_link = modules->link;
        while(pmodel_link) {
            if(pmodel_link->id == model_id) {
                if(pre_item == NULL) {
                    modules->link = pmodel_link->next;
                } else {
                    pre_item->next = pmodel_link->next;
                }
                gisunlink_free(pmodel_link);
                modules->module_flag &= ~(1 << model_id);
                break;
            }
            pre_item = pmodel_link;
            pmodel_link = pmodel_link->next;
        }
    }
	gisunlink_free_lock(modules->msg_mutex);
}

uint8 gisunlink_message_push(gisunlink_message *msg) {
    gisunlink_module *pmodel_link = NULL;

    uint8 ret = 0x00;

    if(modules == NULL || msg == NULL) {
        return 0xFF;
    }

	gisunlink_get_lock(modules->msg_mutex);
    if(msg->type) {
        gisunlink_message *pnew_msg = NULL;

        if(msg->malloc == NULL) {
            pnew_msg = (gisunlink_message *)gisunlink_message_malloc(msg);
        } else {
            pnew_msg = (gisunlink_message *)msg->malloc(msg);
        }
        gisunlink_queue_push(modules->msg_queue, pnew_msg);
        gisunlink_put_sem(modules->async_notice);
    } else {
        if(modules->module_flag & (1 << msg->dst_id)) {
            pmodel_link = modules->link;
            while(pmodel_link) {
                if(pmodel_link->id == msg->dst_id) {
                    break;
                }
                pmodel_link = pmodel_link->next;
            }
		}
	}
	gisunlink_free_lock(modules->msg_mutex);

	if(pmodel_link) {
		ret = pmodel_link->pcallback(msg);
	}

	return pmodel_link?ret:0xFF;
}
