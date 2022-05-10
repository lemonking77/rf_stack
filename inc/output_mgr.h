/*
 * @Author: Lux1206
 * @Email: 862215606@qq.com
 * @LastEditors: Please set LastEditors
 * @FileName: &file name&
 * @Description: 
 * @Date: 2020-11-07 17:16:29
 * @LastEditTime: 2020-12-18 03:17:32
 */

#ifndef _OUTPUT_MGR_H__
#define _OUTPUT_MGR_H__


#include <stdio.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

int8_t oput_msg_handler(struct pbuf* p, struct rf_card* card);
int8_t oput_msg_add_packet_to_list(struct pbuf* p_buf);
int8_t oput_msg_del_packet_from_list(struct pbuf* p);

#ifdef __cplusplus
}
#endif

#endif  // end  _OUTPUT_MGR_H__
