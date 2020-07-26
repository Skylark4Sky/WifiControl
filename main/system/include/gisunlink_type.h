/*
* _COPYRIGHT_
*
* File Name:gisunlink_type.h
* System Environment: JOHAN-PC
* Created Time:2019-01-22
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#ifndef __GISUNLINK_TYPE_H__
#define __GISUNLINK_TYPE_H__

#ifdef __cplusplus
extern "C"
{
#endif

typedef char				int8;
typedef short				int16;
typedef long				int32;
typedef long long			int64;
typedef unsigned char		uint8;
typedef unsigned short		uint16;
typedef unsigned long		uint32;
typedef unsigned long long	uint64;

#ifndef bool
typedef uint8 bool;
#endif

#ifndef false
#define false 0
#endif
#ifndef true
#define true 1
#endif
#define U8_MAX  0XFF
#define U16_MAX  0XFFFF
#define U32_MAX  0XFFFFFFFF

#ifdef __cplusplus
}
#endif

#endif //__GISUNLINK_TYPE_H__
