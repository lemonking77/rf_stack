/*
 * @Author: Lux1206
 * @Email: 862215606@qq.com
 * @LastEditors: Please set LastEditors
 * @FileName: &file name&
 * @Description:
 * @Date: 2020-11-07 17:16:29
 * @LastEditTime: 2020-12-30 10:38:18
 */

#ifndef _RADIO_CARD_H__
#define _RADIO_CARD_H__


#include <stdio.h>
#include <stdint.h>
#include "rf_stack_cfg.h"
#include "pbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

struct rf_card;

/* receive type */
typedef enum {
    UNICAST = 0,
    BROADCAST,
}recv_mtd_t;

typedef int8_t (*input_fn)(struct pbuf *p, struct rf_card *card);
typedef int8_t (*rf_output_fn)(struct rf_card *card, struct pbuf *p);
typedef int8_t (*rf_card_init_fn)(struct rf_card *card, input_fn input);
typedef int8_t (*recv_cb_fn)(uint8_t* src_mac, recv_mtd_t type, uint8_t *p_data, uint16_t len);

struct rf_card {
    struct rf_card *next;
    /* mac layer input function */
    input_fn input;
    /* radio phy layer output function */
    rf_output_fn rf_output;
    /* app layer recvieve callback function */
    recv_cb_fn recv_cb;
    /* rf transmitting power */
    uint8_t rf_power;
    /* rf transmitting channel */
    uint8_t channel;
    /* number of bytes used in hwaddr */
    uint8_t hwaddr_len;
    /* link level hardware address of this interface */
    uint8_t hwaddr[MAX_HWADDR_LEN];
    /* Maximum segment size(hdr + payload) */
    uint8_t mtu;
    /* number of this interface. */
    uint8_t num;
    /* descriptive abbreviation */
    char name[2];
};


#define get_card_index(card)      ((uint8_t)((card)->num + 1))

int8_t rf_card_add_and_init(struct rf_card *card, recv_cb_fn recv_cb, rf_card_init_fn init);
struct rf_card* rf_card_get_default(void);
int8_t rf_card_set_default(struct rf_card *p_card);
struct rf_card* rf_card_get_from_num(uint8_t num);
struct rf_card* rf_card_get_from_addr(uint8_t* dest);


#ifdef __cplusplus
}
#endif

#endif  // end _RADIO_CARD_H__
