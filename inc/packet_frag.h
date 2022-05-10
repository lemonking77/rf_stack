/*
 * @Author: Lux1206
 * @Email: 862215606@qq.com
 * @LastEditors: Please set LastEditors
 * @FileName: &file name&
 * @Description: 
 * @Date: 2020-11-07 17:16:29
 * @LastEditTime: 2020-12-16 11:32:22
 */

#ifndef _PACKET_FRAG_H__
#define _PACKET_FRAG_H__

#include <stdio.h>
#include <stdint.h>

#include "pbuf.h"
#include "mac_net.h"

#ifdef __cplusplus
extern "C" {
#endif

/* packet reassembly structure */
struct reass_data {
    struct reass_data* next;    // next reass_data
    struct pbuf* p;             // point to pbuf data received sequentially
    struct mac_hdr hdr;         // pbuf head
    uint16_t datagram_len;      // total length of reassembled data(max is 2K)
    uint8_t tmr;                // packet reassembly max age
};

#define ADDR_AND_SEQNUM_MATCH(hdr1, hdr2)  \
    (MAC_ADDR_CMP(&(hdr1)->src, &(hdr2)->src) && \
    MAC_ADDR_CMP(&(hdr1)->dest, &(hdr2)->dest) && \
    (hdr1)->seqnum == (hdr2)->seqnum) ? 1 : 0

void packet_reass_tmr(void);
struct pbuf* packet_reass(struct pbuf* p);
int8_t packet_frag(struct pbuf* p, struct rf_card* card, const struct mac_addr* dest);

#ifdef __cplusplus
}
#endif

#endif  // end _PACKET_FRAG_H__

