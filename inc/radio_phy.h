/*
 * @Author: Lux1206
 * @Email: 862215606@qq.com
 * @LastEditors: Please set LastEditors
 * @FileName: &file name&
 * @Description: 
 * @Date: 2020-11-07 17:16:29
 * @LastEditTime: 2020-12-30 11:43:38
 */

#ifndef _RADIO_PHY_H__
#define _RADIO_PHY_H__


#include <stdio.h>
#include <stdint.h>
#include "rf_stack_cfg.h"
#include "radio_card.h"

#ifdef __cplusplus
extern "C" {
#endif


int8_t rf_card_init(struct rf_card* p_card, input_fn input);

#ifdef __cplusplus
}
#endif

#endif  // end _RADIO_PHY_H__
