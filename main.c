/*
 * @Author: Lux1206
 * @Email: 862215606@qq.com
 * @LastEditors: Please set LastEditors
 * @FileName: &file name&
 * @Description: 
 * @Date: 2020-11-09 09:57:18
 * @LastEditTime: 2021-03-19 21:08:17
 */
#include <malloc.h>
#include "pbuf.h"
#include "rf_tmr.h"
#include "radio_phy.h"
#include "radio_card.h"
#include "rf_stack_cfg.h"

#define RF_POWER        10
#define RF_CHANNEL      11

#define RF_TIMER_ID     1
#define RF_INTERVAL     10  // bat: ms

struct rf_card zig_card;



static void* rf_malloc(uint32_t size)
{
    return malloc(size);
}

static void rf_free(void* ptr)
{
    free(ptr);
}

static uint8_t timer_init_and_start(void) 
{
    // TODO: init rf timer 
    // rf_tmr_t rf_tmr = {
    //     .tmr_id = RF_TIMER_ID,
    //     .interval = RF_INTERVAL,
    //     .tmr_start = 
    //     .tmr_stop = 
    // };
    // rf_tmr_init_and_start(rf_tmr_t *tmr)
}

int8_t rf_recv_callback(uint8_t* src_mac, recv_mtd_t type, uint8_t *p_data, uint16_t len)
{
    // TODO: receive handle
}


int main(void)
{
    int8_t ret;
    uint8_t mac_addr[MAX_HWADDR_LEN] = {0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88};
    
    mem_t mem = {
        .malloc_fn = rf_malloc,
        .free_fn = rf_free
    };
    mem_init(&mem);

    zig_card.channel = RF_CHANNEL;
    zig_card.rf_power = RF_POWER;
    zig_card.hwaddr_len = MAX_HWADDR_LEN;
    memcpy(zig_card.hwaddr, mac_addr, MAX_HWADDR_LEN);
    
    ret = rf_card_add_and_init(&zig_card, rf_recv_callback, rf_card_init);
    if (ret == ERR_OK) {
        rf_card_set_default(&zig_card);
        timer_init_and_start();
    }
}


