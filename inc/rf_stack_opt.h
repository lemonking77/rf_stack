/*
 * @Author: Lux1206
 * @Email: 862215606@qq.com
 * @LastEditors: Please set LastEditors
 * @FileName: &file name&
 * @Description: 
 * @Date: 2020-12-08 06:18:12
 * @LastEditTime: 2021-03-19 16:44:07
 */
#ifndef _RF_STACK_OPT_H__
#define _RF_STACK_OPT_H__

#ifdef __cplusplus
extern "C" {
#endif

// support data fragmentation
#if !defined DATA_OUTPUT_FRAG
#define DATA_OUTPUT_FRAG                1
#endif

// support data reassembly
#if !defined DATA_INPUT_FRAG
#define DATA_INPUT_FRAG                 1
#endif

// data max retry times.
#if !defined MAC_MAX_RETRIES
#define MAC_MAX_RETRIES                 10
#endif

// maximum number of devices connected.
#if !defined MAC_MAX_DEV_NUM
#define MAC_MAX_DEV_NUM                 8
#endif



#ifdef __cplusplus
}
#endif

#endif // end _RF_MF_STACK_CFG_H__