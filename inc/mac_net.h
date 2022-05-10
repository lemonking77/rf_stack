/*
 * @Author: Lux1206
 * @Email: 862215606@qq.com
 * @LastEditors: Please set LastEditors
 * @FileName: &file name&
 * @Description: 
 * @Date: 2020-11-07 17:16:29
 * @LastEditTime: 2020-12-30 11:11:06
 */

#ifndef _MAC_NET_H__
#define _MAC_NET_H__

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "rf_stack_cfg.h"
#include "radio_card.h"

#ifdef __cplusplus
extern "C" {
#endif

/* frame type */
typedef enum {
    BEACON_FRAME = 0,
    DATA_FRAME,
    ACK_FRAME,
    COMMAND_FRAME,
    MF_TEST_FRAME = 7,
}FRAME_T;


/* send result */
typedef enum {
    SEND_OK = 0,         // Successful delivery
    SEND_ERR,            // Transmission failed due to network failure
    SEND_MEMORY_ERR,     // Send failed due to memory error
    SEND_QUEUE_FULL_ERR, // Send failed because the queue was full
    SEND_TIMES_OUT,      // Sending failed due to no network
    SEND_ADDR_ERR,       // Delivery failed due to address error
    SEND_RTE_ERR         // fr card route error
}RESULT_T;

/* MAC address */
struct mac_addr{
    uint8_t addr[MAX_HWADDR_LEN];
}__PACKED;

/* mac head */
struct mac_hdr {
    struct mac_addr dest;
    struct mac_addr src;
    uint8_t options;
#define FRAME_TYPE  0xE0    // frame type
#define RECV1       0x18    // reserved
#define ACK_REQ     0x04    // ack request
#define RECV2       0x03    // reserved
    uint8_t sum;
    uint16_t seqnum;
    uint16_t offset;
#define RF_FLG      0x8000U // reserved fragment flag
#define DF_FLG      0x4000U // don't fragment flag
#define MF_FLG      0x2000U // more fragments flag
#define OFFSET_MASK 0x1FFFU // mask for fragmenting bits
    uint16_t len;           // total length of mac layer(hdr len + payload len)
}__PACKED;

/* send data structure */
typedef struct {
    uint8_t data_id;        // data id (not use)
    uint8_t priority;       // data priority (not use)
    FRAME_T frame_type;     // frame type
    uint8_t ack_req;        // ack request
    uint16_t len;           // len of buf
    void* buf;              // data buf
}send_data_t;

extern const struct mac_addr mac_broadcast;

/* send result callback */
typedef void (*result_fn)(RESULT_T, send_data_t*);

#define MAC_HDR_LEN         (sizeof(struct mac_hdr))

#define MAC_ADDR_CMP(addr1, addr2)  \
    (memcmp((addr1)->addr, (addr2)->addr, MAX_HWADDR_LEN) == 0)

#define SEQNUM_CMP(seq1, seq2) \
    (seq1 == seq2)

#define FRAME_TYPE_GET(hdr) \
    ((hdr->options & FRAME_TYPE) >> 5)

#define FRAME_TYPE_SET(hdr, ty) \
    (hdr->options = (hdr->options & ~(FRAME_TYPE)) | (ty << 5))

#define ACK_REQUEST_GET(hdr) \
    ((hdr->options & ACK_REQ) >> 2)

#define ACK_REQUEST_SET(hdr, ack) \
    (hdr->options = (hdr->options & ~(ACK_REQ)) | (ack << 2))

#define HDR_OFFSET_BYTES(hdr) \
    ((uint16_t)((hdr->offset & OFFSET_MASK) * 4U))


#define IS_FRAG_DATA(hdr) \
    ((hdr->offset & (OFFSET_MASK | MF_FLG)) != 0)

#define IS_MORE_FRAG_DATA(hdr) \
    ((hdr->offset & MF_FLG) != 0)


uint8_t check_sum(const uint8_t* data, uint16_t len);
struct rf_card* mac_route(struct mac_addr* dst);
int8_t mac_input(struct pbuf* p, struct rf_card* card);
int8_t mac_output(struct pbuf* p, struct rf_card* p_card, uint16_t data_len,
                  uint8_t* dest_addr, uint8_t type, uint8_t ack);
int8_t mac_output_to_src(uint8_t* dest_addr, uint8_t* src_addr, send_data_t* data);


#ifdef __cplusplus
}
#endif

#endif  // end _MAC_NET_H__
