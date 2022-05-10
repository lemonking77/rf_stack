/*
 * @Author: Lux1206
 * @Email: 862215606@qq.com
 * @LastEditors: Please set LastEditors
 * @FileName: &file name&
 * @Description: 
 * @Date: 2020-12-18 10:16:38
 * @LastEditTime: 2020-12-23 02:25:11
 */
#ifndef _INPUT_MGR_H__
#define _INPUT_MGR_H__


#include <stdio.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

int8_t iput_msg_handler(struct pbuf* p_buf);
int8_t input_pbuf_add_to_list(struct pbuf* p_buf);

#ifdef __cplusplus
}
#endif

#endif  // end  _INPUT_MGR_H__
