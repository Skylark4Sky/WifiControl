/*
* _COPYRIGHT_
*
* File Name: gisunlink_queue.c
* System Environment: JOHAN-PC
* Created Time:2019-01-22
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#include <stdio.h>
#include <string.h>

#include "gisunlink_print.h"
#include "gisunlink_queue.h"
#include "gisunlink_atomic.h"

typedef struct _gisunlink_queue_info {
    void *data;
    struct _gisunlink_queue_info *next;
} gisunlink_queue_info;

typedef struct _gisunlink_queue {
    unsigned short total_item;
    unsigned short max_item;
	void *queue_mutex;
    gisunlink_queue_info *head;
    gisunlink_queue_info *find;
    gisunlink_queue_info *tail;
} gisunlink_queue;

void *gisunlink_queue_init(int maxitem) {
    gisunlink_queue *new_queue = gisunlink_malloc(sizeof(gisunlink_queue));
    if(NULL == new_queue) {
        gisunlink_print(GISUNLINK_PRINT_ERROR,"[%s:%s] it fails to malloc!", __FILE__, __func__);
        return NULL;
    }
    new_queue->head = NULL;
    new_queue->tail = NULL;
    new_queue->find = NULL;
    new_queue->total_item = 0;
    new_queue->max_item = maxitem;
    new_queue->queue_mutex = gisunlink_create_lock();
    return new_queue;
}

void gisunlink_queue_destroy(void *queue_handle, gisunlink_queue_free *queue_free) {
    gisunlink_queue *queue_info = (gisunlink_queue *)queue_handle;
    gisunlink_queue_info *head, *next;

    if(queue_handle == NULL) {
        return;
    }
    gisunlink_get_lock(queue_info->queue_mutex);
    head = queue_info->head;

    while(head) {
        next = head->next;
        queue_free(head->data);
        gisunlink_free(head);
        head = next;
    }
	gisunlink_free_lock(queue_info->queue_mutex);
    gisunlink_free(queue_info);
}

void *gisunlink_queue_get_tail_note(void *queue_handle) {
    gisunlink_queue *queue_info = (gisunlink_queue *)queue_handle;

    if(queue_handle == NULL) {
        return NULL;
    }

    return queue_info->tail->data;
}

void *gisunlink_queue_get_head_note(void *queue_handle) {
    gisunlink_queue *queue_info = (gisunlink_queue *)queue_handle;

    if(queue_handle == NULL) {
        return NULL;
    }
    return queue_info->head->data;
}

bool gisunlink_queue_push_head(void *queue_handle, void *data) {
	gisunlink_queue *queue_info = (gisunlink_queue *)queue_handle;

	gisunlink_queue_info *newnote = NULL;

	if(queue_handle == NULL) {
		return false;
	}

	gisunlink_get_lock(queue_info->queue_mutex);

	if(queue_info->total_item >= queue_info->max_item) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"[%s] the queue items is too many(max:%d)!", __func__, queue_info->max_item);
		gisunlink_free_lock(queue_info->queue_mutex);
		return false;
	}

	queue_info->total_item++;
	newnote = gisunlink_malloc(sizeof(gisunlink_queue_info));
	if(newnote == NULL) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"[%s] it fails to malloc!", __func__);
		gisunlink_free_lock(queue_info->queue_mutex);
		return false;
	}

	newnote->data = data;
	newnote->next = queue_info->head;
	queue_info->head = newnote;

	if (queue_info->tail == NULL) {
		queue_info->tail = newnote;
	}

	gisunlink_free_lock(queue_info->queue_mutex);
	return true;
}

bool gisunlink_queue_push(void *queue_handle, void *data) {

    gisunlink_queue *queue_info = (gisunlink_queue *)queue_handle;
    gisunlink_queue_info *newnote = NULL;

    if(queue_handle == NULL) {
        return false;
    }

    gisunlink_get_lock(queue_info->queue_mutex);

    if(queue_info->total_item >= queue_info->max_item) {
        gisunlink_print(GISUNLINK_PRINT_ERROR,"[%s] the queue items is too many(max:%d)!", __func__, queue_info->max_item);
        gisunlink_free_lock(queue_info->queue_mutex);
        return false;
    }
    queue_info->total_item++;
    newnote= gisunlink_malloc(sizeof(gisunlink_queue_info));
    if(newnote == NULL) {
        gisunlink_print(GISUNLINK_PRINT_ERROR,"[%s] it fails to malloc!", __func__);
        gisunlink_free_lock(queue_info->queue_mutex);
        return false;
    }
    newnote->data = data;
    newnote->next = NULL;

    if(queue_info->tail){
        queue_info->tail->next = newnote;
        queue_info->tail = newnote;
    } else {
        queue_info->tail = newnote;
        queue_info->head = newnote;
    }

    gisunlink_free_lock(queue_info->queue_mutex);
    return true;
}

void *gisunlink_queue_pop(void *queue_handle) {
    gisunlink_queue *queue_info = (gisunlink_queue *)queue_handle;
    void *data;
    gisunlink_queue_info *head = NULL;

	if(queue_handle == NULL) {
        return NULL;
    }

    gisunlink_get_lock(queue_info->queue_mutex);
    if(NULL == queue_info->head) {
        gisunlink_free_lock(queue_info->queue_mutex);
        return NULL;
    }

    queue_info->total_item--;

    data = queue_info->head->data;
    head = queue_info->head;
    queue_info->head = queue_info->head->next;

    if(queue_info->head == NULL) {
        queue_info->head = NULL;
        queue_info->tail = NULL;
    }

    if(head == queue_info->find) {
        queue_info->find = queue_info->head;
    }

    gisunlink_free_lock(queue_info->queue_mutex);

    gisunlink_free(head);
    return data;
}

void *gisunlink_queue_pop_tail(void *queue_handle) {
    gisunlink_queue *queue_info = (gisunlink_queue *)queue_handle;
    void *data;
    gisunlink_queue_info *head = NULL, *find = NULL;

    if(queue_handle == NULL) {
        return NULL;
    }

    gisunlink_get_lock(queue_info->queue_mutex);
    if(NULL == queue_info->head) {
        gisunlink_free_lock(queue_info->queue_mutex);
        return NULL;
    }

    queue_info->total_item--;
    data = queue_info->tail->data;
    head = queue_info->head;
    while(head->next) {
        find = head;
        head = head->next;
    }

    if(find) {
        queue_info->tail = find;
        find->next = NULL;
    }

    if(find == NULL) {
        queue_info->find = NULL;
        queue_info->head = NULL;
        queue_info->tail = NULL;
    } else if(head == queue_info->find){
        queue_info->find = find;
    }

    gisunlink_free_lock(queue_info->queue_mutex);
    gisunlink_free(head);
    return data;
}

void gisunlink_queue_sort(void *queue_handle, gisunlink_queue_compare *compare) {
    int i, total = 0;
    gisunlink_queue *queue_info = (gisunlink_queue *)queue_handle;
    gisunlink_queue_info **sorted_queue = NULL, *head = NULL;

    if(queue_handle == NULL) {
        return ;
    }

    if(queue_info->total_item == 0) {
        return;
    }

    sorted_queue = gisunlink_malloc(sizeof(gisunlink_queue_info) * queue_info->total_item);
    if(sorted_queue == NULL) {
        return;
    }

    memset(sorted_queue, 0, sizeof(gisunlink_queue_info) * queue_info->total_item);
    gisunlink_get_lock(queue_info->queue_mutex);
    head = queue_info->head;
    while(head) {
        for(i = 0; i < total; i++) {
            if(compare(head->data, sorted_queue[i]->data) < 0) {
                memmove((void *)((uint32)sorted_queue + (i + 1)* sizeof(gisunlink_queue_info)),
                    (void *)((uint32)sorted_queue + i * sizeof(gisunlink_queue_info)), (total - i) * sizeof(gisunlink_queue_info));
                break;
            }
        }
        sorted_queue[i] = head;
        total++;
        head = head->next;
    }
    queue_info->head = sorted_queue[0];
    queue_info->tail = sorted_queue[queue_info->total_item-1];
    queue_info->tail->next = NULL;
    for(i = 1; i < queue_info->total_item; i ++) {
        sorted_queue[i-1]->next = sorted_queue[i];
    }
    gisunlink_free(sorted_queue);
    gisunlink_free_lock(queue_info->queue_mutex);
}

bool gisunlink_queue_not_empty(void *queue_handle) {
    gisunlink_queue *queue_info = (gisunlink_queue *)queue_handle;
    uint8 ret;

    if(queue_handle == NULL) {
        return false;
    }

    gisunlink_get_lock(queue_info->queue_mutex);
    ret = queue_info && queue_info->head;
    gisunlink_free_lock(queue_info->queue_mutex);
    return ret;
}

uint16 gisunlink_queue_reset(void *queue_handle) {
    uint16 items;
    gisunlink_queue *queue_info = (gisunlink_queue *)queue_handle;

    if(queue_handle == NULL) {
        return false;
    }

    gisunlink_get_lock(queue_info->queue_mutex);
    queue_info->find = queue_info->head;
    items = queue_info->total_item;
    gisunlink_free_lock(queue_info->queue_mutex);
    return items;
}

void gisunlink_queue_lock(void *queue_handle) {
    gisunlink_queue *queue_info = (gisunlink_queue *)queue_handle;
    if(queue_handle == NULL) {
        return;
    }
    gisunlink_get_lock(queue_info->queue_mutex);
}

void gisunlink_queue_unlock(void *queue_handle) {
    gisunlink_queue *queue_info = (gisunlink_queue *)queue_handle;
    if(queue_handle == NULL) {
        return;
    }
    gisunlink_free_lock(queue_info->queue_mutex);
}

void *gisunlink_queue_get_next(void *queue_handle, void *item, gisunlink_queue_compare *pcmp_chk) {
    gisunlink_queue *queue_info = (gisunlink_queue *)queue_handle;
    gisunlink_queue_info *find_item = NULL;

    if(queue_handle == NULL) {
        return NULL;
    }

    while(queue_info->find) {
        if(pcmp_chk == NULL || (pcmp_chk(item, queue_info->find->data) == 0)) {
            find_item = queue_info->find;
            queue_info->find = queue_info->find->next;
            return find_item->data;
        }
        queue_info->find = queue_info->find->next;
    }
    return NULL;
}

void *gisunlink_queue_pop_item(void *queue_handle, void *item) {
    gisunlink_queue *queue_info = (gisunlink_queue *)queue_handle;
    gisunlink_queue_info *find_item = NULL, *pre_item = NULL;
    void *data;

    if(queue_handle == NULL) {
        return 0;
    }

    gisunlink_get_lock(queue_info->queue_mutex);
    find_item = queue_info->head;
    while(find_item) {
        if((uint32)item == (uint32)find_item->data) {
            break;
        }
        pre_item = find_item;
        find_item = find_item->next;
    }

    if(find_item) {
        if(find_item == queue_info->find) {
            queue_info->find = find_item->next;
        }

        if(pre_item == NULL) {
            queue_info->head = queue_info->head->next;
        } else {
            pre_item->next = find_item->next;
        }
        if(find_item == queue_info->tail) {
            queue_info->tail = pre_item;
        }
		queue_info->total_item--;
    }

    gisunlink_free_lock(queue_info->queue_mutex);
    if(find_item == NULL) {
        return NULL;
    }
    data = find_item->data;
    gisunlink_free(find_item);
    return data;
}

void *gisunlink_queue_pop_cmp(void *queue_handle, void *item, gisunlink_queue_compare *pcmp_chk) {
    gisunlink_queue *queue_info = (gisunlink_queue *)queue_handle;
    gisunlink_queue_info *find_item = NULL, *pre_item = NULL;
    void *data;

    if(queue_handle == NULL) {
        return 0;
    }

    gisunlink_get_lock(queue_info->queue_mutex);
    find_item = queue_info->head;
    while(find_item) {
        if(pcmp_chk(item ,find_item->data) == 0) {
            break;
        }
        pre_item = find_item;
        find_item = find_item->next;
    }

    if(find_item) {
        if(find_item == queue_info->find) {
            queue_info->find = find_item->next;
        }

        if(pre_item == NULL) {
            queue_info->head = queue_info->head->next;
        } else {
            pre_item->next = find_item->next;
        }

        if(find_item == queue_info->tail) {
            queue_info->tail = pre_item;
        }
		queue_info->total_item--;
    }

    gisunlink_free_lock(queue_info->queue_mutex);
    if(find_item == NULL) {
        return NULL;
    }
    data = find_item->data;
    gisunlink_free(find_item);
    return data;
}

bool gisunlink_queue_is_full(void *queue_handle) {
    gisunlink_queue *queue_info = (gisunlink_queue *)queue_handle;
    uint8 ret;

    if(queue_handle == NULL) {
        return false;
    }

    gisunlink_get_lock(queue_info->queue_mutex);
    ret = (queue_info->total_item + 1 >= queue_info->max_item);
    gisunlink_free_lock(queue_info->queue_mutex);

    return ret;
}

uint16 gisunlink_queue_items(void *queue_handle) {
    gisunlink_queue *queue_info = (gisunlink_queue *)queue_handle;
    uint16 ret;

    if(queue_handle == NULL) {
        return 0;
    }

    gisunlink_get_lock(queue_info->queue_mutex);
    ret = queue_info->total_item;
    gisunlink_free_lock(queue_info->queue_mutex);

    return ret;
}

void *gisunlink_queue_get_index_item(void *queue_handle, int index) {
    gisunlink_queue *queue_info = (gisunlink_queue *)queue_handle;
    gisunlink_queue_info *head = NULL;

    if(queue_handle == NULL) {
        return NULL;
    }

   	gisunlink_get_lock(queue_info->queue_mutex);

	if(index >= queue_info->total_item) {
		gisunlink_free_lock(queue_info->queue_mutex);
        return NULL;
    }

    head = queue_info->head;
	if(head == NULL) {
		gisunlink_free_lock(queue_info->queue_mutex);
		return NULL;
	}

    while(index && head) {
        head = head->next;
        index --;
    }

    gisunlink_free_lock(queue_info->queue_mutex);

	if(head) {
    	return head->data;
	} else {
		return NULL;
	}
}

