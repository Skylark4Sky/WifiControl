/*
* _COPYRIGHT_
*
* File Name:gisunlink_queue.h
* System Environment: JOHAN-PC
* Created Time:2019-01-22
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#ifndef __GISUNLINK_QUEUE_H__
#define __GISUNLINK_QUEUE_H__

#include "gisunlink_type.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef void gisunlink_queue_free(void *item);

typedef int gisunlink_queue_compare(void *item1, void *item2);

/*! @brief 队列初始化
 * @param max_item : 队列中最大储存项
 * @return 队列句柄
 */
void *gisunlink_queue_init(int maxitem);

/*! @brief 队列释放，将释放队列中所有未处理的数据项，只释放相应内存系统将丢失所有数据
 * @param queue_handle : 队列句柄
 * @param queue_free : 队列释放数据项的内存回调
 * @return void
 */
void gisunlink_queue_destroy(void *queue_handle, gisunlink_queue_free *queue_free);

/*! @brief 将用户数据插入队列的头部
 * @param queue_handle : 队列句柄
 * @param data : 用户数据指针
 * @return false-->fail; true-->ok
 */
bool gisunlink_queue_push_head(void *queue_handle, void *data);

/*! @brief 将用户数据放入队列中
 * @param queue_handle : 队列句柄
 * @param data : 用户数据指针
 * @return false-->fail; true-->ok
 */

bool gisunlink_queue_push(void *queue_handle, void *data);

/*! @brief 将用户数据返回给用户，队列中弹出此数据项
 * @param queue_handle : 队列句柄
 * @return false-->fail; true-->ok
 */
void *gisunlink_queue_pop_tail(void *queue_handle);

/*! @brief 将用户数据返回给用户，队列中弹出此数据项
 * @param queue_handle : 队列句柄
 * @return 返返回队列头数据
 */
void *gisunlink_queue_pop(void *queue_handle);

/*! @brief 对队列中的数据项进行排序
 * @param queue_handle : 队列句柄
 * @param compare : 数据项比较回调函数
 * @return void
 */
void gisunlink_queue_sort(void *queue_handle, gisunlink_queue_compare *compare);

/*! @brief 判断队列是否为空
 * @param queue_handle : 队列句柄
 * @return false-->empty; true-->have
 */
bool gisunlink_queue_not_empty(void *queue_handle);

/*! @brief 判断队列是否为满
 * @param queue_handle : 队列句柄
 * @return false-->not full; true-->full
 */
bool gisunlink_queue_is_full(void *queue_handle);

/*! @brief 返回队列尾的数据项，并不解除在队列的位置
 * @param queue_handle : 队列句柄
 * @return 队列尾数据项
 */
void *gisunlink_queue_get_tail_note(void *queue_handle);

/*! @brief 返回队列头的数据项，并不解除在队列的位置
 * @param queue_handle : 队列句柄
 * @return 队列头数据项
 */
void *gisunlink_queue_get_head_note(void *queue_handle);

/*! @brief 返回队列数据项数目
 * @param queue_handle : 队列句柄
 * @return 数据项数目
 */
uint16 gisunlink_queue_items(void *queue_handle);

/*! @brief 返回指定数据项数据
 * @param queue_handle : 队列句柄
 * @param index : 数据项序号
 * @return 用户数据项
 */
void *gisunlink_queue_get_index_item(void *queue_handle, int index);

/*! @brief 复位查找链表，将其指向头结点
 * @param queue_handle : 队列句柄
 * @return 当前队列中元素数目
 */
uint16 gisunlink_queue_reset(void *queue_handle);

/*! @brief 对队列加锁，以便配合gisunlink_queue_get_next使用
 * @param queue_handle : 队列句柄
 * @return void
 */
void gisunlink_queue_lock(void *queue_handle);

/*! @brief 对队列解锁，以便配合gisunlinkqueuegetnext使用
 * @param queue_handle : 队列句柄
 * @return void
 */
void gisunlink_queue_unlock(void *queue_handle);

/*! @brief 得到队列的下一个数据，并不会对队列进行操作
 * @param queue_handle : 队列句柄
 * @param item : 用户要使用的比较数据
 * @param pcmp_chk : 用户使用的比较函数,如果为空的话，将不判断条件
 * @return 返回队列下一个数据
 */
void *gisunlink_queue_get_next(void *queue_handle, void *item, gisunlink_queue_compare *pcmp_chk);

/*! @brief 将指定项的用户数据返回给用户，队列中弹出此数据项
 * @param queue_handle : 队列句柄
 * @param item : 使用push传入的用户数据
 * @return 返回队指定的数据项
 */
void *gisunlink_queue_pop_item(void *queue_handle, void *item);

/*! @brief 将用户数据返回给用户，队列中弹出此数据项
 * @param queue_handle : 队列句柄
 * @param compare : 用户要使用的比较数据
 * @param pcmp_chk : 用户使用的比较函数
 * @return 返回队指定的数据项
 */
void *gisunlink_queue_pop_cmp(void *queue_handle, void *item, gisunlink_queue_compare *pcmp_chk);


#ifdef __cplusplus
}
#endif

#endif //__GISUNLINK_QUEUE_H__
