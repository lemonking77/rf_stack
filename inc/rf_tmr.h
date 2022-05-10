/*
 * @Author: Lux1206
 * @Email: 862215606@qq.com
 * @LastEditors: Please set LastEditors
 * @FileName: &file name&
 * @Description: 
 * @Date: 2020-11-07 17:16:29
 * @LastEditTime: 2021-01-03 08:57:21
 */

#ifndef _RF_TMR_H__
#define _RF_TMR_H__


#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*tmr_cb_fn)(uint8_t);
typedef void (*tmr_stop_fn)(uint8_t);
typedef void (*tmr_start_fn)(uint8_t, uint32_t, tmr_cb_fn);

typedef struct {
    // rf timer id
    uint8_t tmr_id;
    // rf timer interval 
    uint32_t interval;
    // pointer of timer start function
    tmr_start_fn tmr_start;
    // pointer of timer stop function
    tmr_stop_fn tmr_stop;
}rf_tmr_t;


void rf_tmr_start(void);
void rf_tmr_stop(void);
uint8_t rf_tmr_init_and_start(rf_tmr_t *tmr);

#ifdef __cplusplus
}
#endif

#endif  // end _RF_TMR_H__
