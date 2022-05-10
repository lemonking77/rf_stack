/*
 * @Author: Lux1206
 * @Email: 862215606@qq.com
 * @LastEditors: Please set LastEditors
 * @FileName: &file name&
 * @Description: 
 * @Date: 2020-11-09 09:57:18
 * @LastEditTime: 2020-12-30 10:37:45
 */
#include "rf_stack_cfg.h"
#include "radio_card.h"
#include "mac_net.h"

static uint8_t rf_card_num = 0;
struct rf_card* card_list = NULL;
struct rf_card* default_card = NULL;

/**
 * @brief: add rf card 
 * @param {in} rf_card: pointer of rf card
 * @param {in} recv_cb: app layer reveive callback
 * @param {in} init: rf card init function
 * @return: result
 */
int8_t rf_card_add_and_init(struct rf_card *p_card, recv_cb_fn recv_cb, rf_card_init_fn init)
{
    if (init == NULL || p_card == NULL) {
        return ERR_RF;
    }

    p_card->recv_cb = recv_cb;

    p_card->num = rf_card_num;
    if (p_card->num == 254) {
        rf_card_num = 0;
    } 
    else {
        rf_card_num = p_card->num + 1;
    }

    // call user specified initialization function for rf card
    if (init(p_card, mac_input) != ERR_OK) {
        return ERR_RF;
    }

    // add new net card
    p_card->next = card_list;
    card_list = p_card;

    return ERR_OK;
}

/**
 * @brief: set default rf card
 * @param {in} num: card number
 * @return: result
 */
int8_t rf_card_set_default(struct rf_card* p_card)
{
    if (p_card == NULL) {
        return ERR_PARA;
    }
    default_card = p_card;
    
    return ERR_OK;
}
/**
 * @brief: get default rf card
 * @param {none}
 * @return pointer of default re card
 */
struct rf_card* rf_card_get_default(void)
{
    return default_card;
}

/**
 * @brief: get rf card interface through num
 * @param {in} num: card number
 * @return: pointer of rf card
 */
struct rf_card* rf_card_get_from_num(uint8_t num)
{
    struct rf_card* p_card = NULL;
    
    if (card_list == NULL) {
        return NULL;
    }

    for (p_card = card_list; p_card != NULL; p_card = p_card->next) {
        if (p_card->num == num) {
            return p_card;
        }
    }
    
    return NULL;
} 

/**
 * @brief: get rf card interface through addr
 * @param {in} num: card number
 * @return: pointer of rf card
 */
struct rf_card* rf_card_get_from_addr(uint8_t* dest)
{
    struct rf_card* p_card = NULL;
    
    if (card_list == NULL) {
        return NULL;
    }

    for (p_card = card_list; p_card != NULL; p_card = p_card->next) {
        if ((memcmp(p_card->hwaddr, dest, p_card->hwaddr_len) == 0)) {
            return p_card;
        }
    }

    return NULL;
}



