/*
 * @Author: Lux1206
 * @Email: 862215606@qq.com
 * @LastEditors: Please set LastEditors
 * @FileName: &file name&
 * @Description: Files used to adapt to Hardware RF transmitting interface
 * @Date: 2020-11-09 09:57:18
 * @LastEditTime: 2021-03-20 16:07:29
 */

#include <string.h>
#include "rf_stack_cfg.h"
#include "pbuf.h"
#include "mac_net.h"
#include "radio_phy.h"
#include "radio_card.h"

// RF network card created externally
extern struct rf_card zig_card;
static void radio_low_level_input_callback(uint8_t *packet, uint16_t len);
uint8_t radio_low_level_input(struct rf_card* p_card, uint8_t* packet, uint8_t len);

/**
 * @brief: vendor radio input callback function
 * @param {in} packet: pointer of receive packet
 * @param {in} len: length of packet
 * @note: this function must be called by vendor rf receive.
 * @return: none
 */
static void vendor_radio_input_callback(uint8_t *packet, uint16_t len)
{
    radio_low_level_input_callback(packet, len);
}

/**
 * @brief: vendor radio send founction
 * @param {in} p_card: pointer of rf card
 * @param {in} packet: pointer of packet to send
 * @param {in} len: len of packet
 * @return: none
 */
void vendor_radio_output(struct rf_card* p_card, uint8_t* packet, uint16_t len)
{
    // TODO: adaptive Hardware RF transmitting interface
}

/**
 * @brief: vendor radio init function
 * @param {in} power: transmit power
 * @param {in} channel: RF channel
 * @return: none
 */
void vendor_radio_init(int8_t power, uint8_t channel)
{
    // TODO: Initialize vendor Hardware RF,we need to register the receiving 
    // function in the bottom layer of the vendor to receive data.

    // Register data receiving function  vendor_radio_input_callback()

    TY_LOGDEBUG("radio init.");
}


/**
 * @brief: radio low level input callback function
 * @param {in} packet: pointer of receive packet
 * @param {in} len: length of packet
 * @return: none
 */
static void radio_low_level_input_callback(uint8_t *packet, uint16_t len)
{
    int8_t ret;
    struct mac_hdr* p_hdr;

    if(len > MAX_PAYLOAD_LEN + MAC_HDR_LEN) {
        TY_LOGERROR("radio input data len error1.");
        return;
    }

    p_hdr = (struct mac_hdr*)packet;
    if (p_hdr->len < MAC_HDR_LEN ||
        p_hdr->len > zig_card.mtu) {
        TY_LOGERROR("radio input data len error2.");
        return;
    }

    // traverse the rf card interface list to match rf card
    if (!rf_card_get_from_addr(p_hdr->dest.addr) &&     // unicast to us?
        !MAC_ADDR_CMP(&p_hdr->dest, &mac_broadcast)) {  // broadcast?
        // no rf card match
        return;
    }
    // TY_LOGDEBUG("-->SRC:%x:%x, SEQ:0x%02x,\r\n",
    //     (uint8_t)p_hdr->src.addr[6], (uint8_t)p_hdr->src.addr[7],p_hdr->seqnum);
    ret = radio_low_level_input(&zig_card, (void *)p_hdr, p_hdr->len);
    if (ret != ERR_OK) {
        TY_LOGERROR("radio input error.");
    }
}

/**
 * @brief: radio low level output function
 * @param {in} p_card: pointer of rf card
 * @param {in} p_buf: pointer of pbuf
 * @return: ERR_OK: 0 sucess
 */
int8_t radio_low_level_output(struct rf_card* p_card, struct pbuf* p_buf)
{
    if (p_card == NULL || p_buf == NULL) {
        return ERR_PARA;
    }
    if (p_buf->len > p_card->mtu) {
        return ERR_PARA;
    }

    TY_LOGDEBUG("radio low output len: %d",p_buf->len);
    // get mac header information 
    // struct mac_hdr *p_hdr;
    // p_hdr = (struct mac_hdr *)p_buf->payload;
    // if( FRAME_TYPE_GET(p_hdr) != ACK_FRAME) {
    //     TY_LOGDEBUG("<--DST:%x:%x, SEQ:0x%02x,\r\n",
    //         (uint8_t)p_hdr->dest.addr[6], (uint8_t)p_hdr->dest.addr[7],p_hdr->seqnum);
    // }
    vendor_radio_output(p_card, (uint8_t *)p_buf->payload, p_buf->len);

    return ERR_OK;
}

/**
 * @brief: radio low level output function
 * @param {in} p_card: pointer of rf card
 * @param {in} packet: pointer of packet to send
 * @param {in} len: len of packet
 * @return: none
 */
uint8_t radio_low_level_input(struct rf_card* p_card, uint8_t* packet, uint8_t len)
{
    struct pbuf* p_buf = NULL;
    
    p_buf = pbuf_alloc(PBUF_RAW, len);
    if (p_buf == NULL) {
        TY_LOGERROR("pbuf alloc error");
        return ERR_MEM;
    }

    memcpy(p_buf->payload, packet, p_buf->len);

    if (p_card->input(p_buf, p_card) != ERR_OK) {
        TY_LOGERROR("up layer handler error");
    }

    return ERR_OK;
}

/**
 * @brief: rf card init function
 * @param {in} p_card: pointer of rf card
 * @param {in} input: upper input function: mac_input
 * @return: none
 */
int8_t rf_card_init(struct rf_card* p_card, input_fn input)
{
    p_card->name[0] = 'Z';
    p_card->name[1] = 'G';
    p_card->mtu = MAX_PAYLOAD_LEN + MAC_HDR_LEN;

    p_card->input = input;
    p_card->rf_output = radio_low_level_output;

    vendor_radio_init(p_card->rf_power, p_card->channel);

    return ERR_OK;
}

