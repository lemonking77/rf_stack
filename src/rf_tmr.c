/*
 * @Author: Lux1206
 * @Email: 862215606@qq.com
 * @LastEditors: Please set LastEditors
 * @FileName: &file name&
 * @Description: 
 * @Date: 2020-11-09 09:57:18
 * @LastEditTime: 2021-03-19 21:08:17
 */
#include <time.h>
#include <stdlib.h>
#include "rf_stack_opt.h"
#include "rf_stack_cfg.h"
#include "rf_tmr.h"


rf_tmr_t rf_tmr;

extern void oput_msg_tmr(void);
extern void iput_msg_tmr(void);
extern void packet_reass_tmr(void);

/**
 * @brief: rf stack timer callback
 * @param {in} tmr_id: timer id
 * @return: none
 */
static void rf_tmr_callback(uint8_t tmr_id)
{
    time_t t;
    uint32_t rf_time_ms = 0;

    oput_msg_old_age_tmr();
    oput_msg_tmr();
    iput_msg_tmr();
    
#if DATA_INPUT_FRAG
    packet_reass_tmr();
#endif

    srand((unsigned) time(&t));
    rf_time_ms = rf_tmr.interval + (rand() % 5);
    if (rf_tmr.tmr_start != NULL) {
        rf_tmr.tmr_start(rf_tmr.tmr_id, rf_time_ms, rf_tmr_callback);
    }
}


/**
 * @brief: rf stack timer start function
 * @param {in} cb_f: timer callback function
 * @return: none
 */
void rf_tmr_start(void)
{
    if (rf_tmr.tmr_start == NULL) {
        return;
    }

    rf_tmr.tmr_start(rf_tmr.tmr_id, rf_tmr.interval, rf_tmr_callback);
}

/**
 * @brief: rf stack timer stop function
 * @param {none}
 * @return: none
 */
void rf_tmr_stop(void)
{
    if (rf_tmr.tmr_stop == NULL) {
        return;
    }
    rf_tmr.tmr_stop(rf_tmr.tmr_id);
}

/**
 * @brief: rf stack timer init function
 * @param {in} tmr: rf stack timer structure
 * @return: 0: success
 */
uint8_t rf_tmr_init_and_start(rf_tmr_t *tmr)
{
    if (tmr == NULL) {
        return -1;
    }

    rf_tmr.interval = tmr->interval;
    rf_tmr.tmr_id = tmr->tmr_id;
    rf_tmr.tmr_start = tmr->tmr_start;
    rf_tmr.tmr_stop = tmr->tmr_stop;
    
    rf_tmr_start();
    
    return 0;
}

