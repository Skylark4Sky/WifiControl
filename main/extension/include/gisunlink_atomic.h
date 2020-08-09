/*
* _COPYRIGHT_
*
* File Name:gisunlink_atomic.h
* System Environment: JOHAN-PC
* Created Time:2019-01-25
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#ifndef __GISUNLINK_ATOMIC_H__
#define __GISUNLINK_ATOMIC_H__

#ifdef __cplusplus
extern "C"
{
#endif

typedef void GISUNLINK_TASK_CALLBACK(void *param);

/*! @brief 创建信号量
 * @param max_count
 * @param init_count
 * @return 句柄
 */
void *gisunlink_create_sem(int max_count, int init_count);

/*! @brief 创建信号量
 * @return 句柄
 */
void *gisunlink_create_binary_sem(void);

/*! @brief 获取信号量
 * @param 句柄
 * @return void
 */
void gisunlink_get_sem(void *handle);

/*! @brief 释放信号量
 * @param 句柄
 * @return void
 */
void gisunlink_put_sem(void *handle);

/*! @brief 销毁信号量
 * @param 句柄
 * @return void
 */
void gisunlink_destroy_sem(void *handle);

/*! @brief 创建信号量
 * @return 句柄
 */
void *gisunlink_create_lock(void);

/*! @brief 获取锁
 * @param 句柄
 * @return void
 */
void gisunlink_get_lock(void *handle);

/*! @brief 释放锁
 * @param 句柄
 * @return void
 */
void gisunlink_free_lock(void *handle);

/*! @brief 销毁锁
 * @param 句柄
 * @return void
 */
void gisunlink_destroy_lock(void *handle);

/*! @brief 分配内存
 * @param size
 * @return 地址
 */
void *gisunlink_malloc(size_t sz);

/*! @brief 释放内存
 * @param ptr
 * @return void
 */
void gisunlink_free(void *ptr); 

void *gisunlink_malloc_dbg(size_t sz,int line);
void gisunlink_free_dbg(void *ptr);

/*! @brief 创建任务1
 * @param func : 任务名
 * @param func : 任务主函数
 * @param para : 任务参数
 * @param stack_size : 栈大小
 * @return 句柄
 */
void *gisunlink_create_task(GISUNLINK_TASK_CALLBACK func, const char *pname, void *para, int stack_size);

/*! @brief 创建任务带优先级
 * @param func : 任务名
 * @param func : 任务主函数
 * @param para : 任务参数
 * @param stack_size : 栈大小
 * @param task_priorities : 栈优先级 数值越小，任务优先级越低 ,最高为configMAX_PRIORITIES
 * @return 句柄
 */
void *gisunlink_create_task_with_Priority(GISUNLINK_TASK_CALLBACK func, const char *pname, void *para, int stack_size,int uxPriority);

/*! @brief 创建任务1
 * @param 延时时间
 * @return void
 */
void gisunlink_task_delay(unsigned int xTicksToDelay);

/*! @brief 停止任务的运行
 * @param 任务句柄
 * @return void
 */
void gisunlink_pause_task(void *handle);

/*! @brief 启动任务的运行
 * @param 任务句柄
 * @return void
 */
void gisunlink_run_task(void *handle);

/*! @brief 销毁任务
 * @param 任务句柄
 * @return void
 */
void gisunlink_destroy_task(void *handle);

/*! @brief 获取系统tick记数
 * @return 数值
 */
unsigned int gisunlink_get_tick_count(void);

/*! @brief 设置时间系统
 * @param time
 * @return 时间
 */
unsigned int gisunlink_set_interva_ms(unsigned int time);

/*! @brief 长度对齐
 * @param 原来长度
 * @return 对齐大小
 */
unsigned int gisunlink_bytes_align(unsigned int n,unsigned align);

#ifdef __cplusplus
}
#endif

#endif //__GISUNLINK_ATOMIC_H__
