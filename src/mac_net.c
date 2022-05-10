/*
 * @Author: Lux1206
 * @Email: 862215606@qq.com
 * @LastEditors: Please set LastEditors
 * @FileName: &file name&
 * @Description: 
 * @Date: 2020-11-09 09:57:18
 * @LastEditTime: 2021-03-20 16:06:37
 */
#include "rf_stack_opt.h"
#include "rf_stack_cfg.h"
#include "mac_net.h"
#include "output_mgr.h"
#include "input_mgr.h"
#include "pbuf.h"
#include "packet_frag.h"

uint16_t g_seqnum = 0;
// broadcast mac address
const struct mac_addr mac_broadcast = {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};

/**
 * @brief: check sum
 * @param {in} data: pointer of data
 * @param {in} len: len of data
 * @return: sum
 */
uint8_t check_sum(const uint8_t *data, uint16_t len)
{
    uint16_t i;
    uint8_t  sum = 0;

    for (i = 0; i < len; i++) {
        sum = (sum + data[i]) & 0xff;
    }

    return sum;
}

/**
 * @brief: rf card router 
 * @param {in} dst: destination mac address
 * @return: pointer of rf card
 */
struct rf_card* mac_route(struct mac_addr* dst)
{
    return rf_card_get_from_addr(dst->addr);
}

/**
 * @brief: rf card match with source address
 * @param {in} src_addr: source mac address
 * @return: pointer of rf card
 */
static struct rf_card* rf_card_match(uint8_t* src_addr)
{
    struct rf_card* p_card;

    if (src_addr != NULL) {
        p_card = mac_route((struct mac_addr*)(void *)src_addr);
        if (p_card == NULL) {
            p_card = rf_card_get_default();
        }
    }
    else {
        p_card = rf_card_get_default();
    }
    
    return p_card;
}

/**
 * @brief: ack response
 * @param {in} p: pointer of packet buff
 * @param {in} p_card: rf card
 * @return: result
 */
static int8_t mac_frame_ack_response(struct pbuf* p,struct rf_card* p_card)
{
    int8_t ret;
    struct pbuf* q_buf;
    struct mac_hdr* p_hdr, *q_hdr;
    uint8_t cache[PBUF_STRUCT_SIZE + MAC_HDR_LEN];
    
    if (p == NULL) {
        return ERR_PARA; 
    }

    p_hdr = (struct mac_hdr *)p->payload;

    memset(cache, 0, sizeof(cache));

    q_buf = (struct pbuf*)cache;
    q_buf->payload = (void *)&cache[PBUF_STRUCT_SIZE];
    q_buf->tot_len = MAC_HDR_LEN;
    q_buf->len = MAC_HDR_LEN;
    q_buf->card_idx = p->card_idx;
    q_buf->data_id = p->data_id;
    q_buf->ref = 1;
    
    q_hdr = (struct mac_hdr *)q_buf->payload;

    // cache packet buff header
    memcpy((uint8_t *)q_hdr, (uint8_t *)p_hdr, MAC_HDR_LEN);
    // refill address
    memcpy(q_hdr->dest.addr, p_hdr->src.addr, p_card->hwaddr_len);
    memcpy(q_hdr->src.addr, p_hdr->dest.addr, p_card->hwaddr_len);
    FRAME_TYPE_SET(q_hdr, ACK_FRAME);
    ACK_REQUEST_SET(q_hdr, 0);
    q_hdr->len = MAC_HDR_LEN;

    // check summer
    q_hdr->sum = 0;
    q_hdr->sum = check_sum((uint8_t *)(void *)q_hdr, MAC_HDR_LEN);

    // send ack response
    ret = p_card->rf_output(p_card, q_buf);
    // TY_LOGDEBUG("response ack sum 0x%x.",q_hdr->sum);

    return ret;
}

/**
 * @brief: mac layer input function
 * @param {in} p: pointer of packet buf
 * @param {in} p_card: pointer of rf card
  * @return: result
 */
int8_t mac_input(struct pbuf* p, struct rf_card* p_card)
{
    int8_t ret;
    uint8_t tmp_chksum;
    recv_mtd_t is_broadcast;
    struct mac_hdr *p_hdr;

    if (p == NULL) {
        TY_LOGERROR("input pbuf is NULL.");
        return ERR_PARA;
    }
    if (p->len < MAC_HDR_LEN) {
        TY_LOGERROR("input data len error.");
        goto failed_return;
    }

    if (p->card_idx == 0) {
        p->card_idx = get_card_index(p_card);
    }
    // get mac header information 
    p_hdr = (struct mac_hdr *)p->payload;

    // determine whether it is broadcast data
    is_broadcast = (recv_mtd_t)MAC_ADDR_CMP(&p_hdr->dest, &mac_broadcast);
    
    tmp_chksum = p_hdr->sum;    // cache checksums
    p_hdr->sum = 0;             // set sum to zero
    p_hdr->sum = check_sum((uint8_t *)(void *)p_hdr, p_hdr->len);
    if (tmp_chksum != p_hdr->sum) {
        pbuf_free(p);
        TY_LOGERROR("check sum error, org 0x%x, new 0x%x.",tmp_chksum,p_hdr->sum);
        return ERR_OK;
    }

    switch(FRAME_TYPE_GET(p_hdr)) {
        case BEACON_FRAME:
        case DATA_FRAME:
        case COMMAND_FRAME:
        case MF_TEST_FRAME: {
            TY_LOGDEBUG("recv %d type frame\n",FRAME_TYPE_GET(p_hdr));
            if ((!is_broadcast) && 
                (p_hdr->options & ACK_REQ)) {
                // ack response 
                mac_frame_ack_response(p, p_card);
            }

            // TY_LOGDEBUG("---oft 0x%02x---MF %d---",HDR_OFFSET_BYTES(p_hdr),IS_FRAG_DATA(p_hdr));
            // uart_printf(0, "++>SRC:%x:%x, SEQ:0x%02x,\r\n",
            // (uint8_t)p_hdr->src.addr[6], (uint8_t)p_hdr->src.addr[7],p_hdr->seqnum);

            if (ERR_OK == iput_msg_handler(p)) {
                #if DATA_INPUT_FRAG
                if (IS_FRAG_DATA(p_hdr)) {
                    p = packet_reass(p);
                    if (p == NULL) {
                        return ERR_OK;
                    }
                    p_hdr = (struct mac_hdr *)p->payload;
                }
                #endif
                ret = pbuf_remove_header(p, MAC_HDR_LEN);
                if (ret == ERR_OK && p_hdr->len > MAC_HDR_LEN) {
                    // transfer to the upper layer
                    p_card->recv_cb(p_hdr->src.addr, is_broadcast, p->payload, (p_hdr->len - MAC_HDR_LEN));
                }
            }
            
            break;
        }

        case ACK_FRAME: {
            // receive ack to release pbuf resources, and use retransmission timeout 
            // for broadcast to release pbuf resources.
           TY_LOGDEBUG("receive ack.");
            oput_msg_del_packet_from_list(p);
            break;
        }
        
        default: 
            break;
    }

failed_return:
TY_LOGDEBUG("input %d pbuf free.",pbuf_counter(p));
    pbuf_free(p);
    return ERR_OK;
}


/**
 * @brief: mac layer output function
 * @param {in} p: pointer of packet buf
 * @param {in} len: len of p->payload
 * @param {in} p_card: pointer of rf card
 * @param {in} dest_addr: destionation addr
 * @param {in} type: frame type
 * @param {in} ack: ack request
 * @return: result
 */
int8_t mac_output(struct pbuf* p, struct rf_card* p_card, uint16_t data_len,
                  uint8_t* dest_addr, uint8_t type, uint8_t ack)
{
    uint8_t ack_req;
    struct mac_hdr* p_hdr;
    int8_t ret = ERR_OTHER;

    if (pbuf_add_header(p, MAC_HDR_LEN) != ERR_OK) {
        TY_LOGERROR("pbuf add hdr error.");
        return ERR_BUF;
    }

    p_hdr = (struct mac_hdr*)p->payload;
    ack_req = (ack > 0) ? 1 : 0;
    FRAME_TYPE_SET(p_hdr, type);        // fill frame type
    ACK_REQUEST_SET(p_hdr, ack_req);    // fill ack request    
    p_hdr->seqnum = g_seqnum++;         // fill seqnum
    memcpy(p_hdr->dest.addr, dest_addr, p_card->hwaddr_len);        // fill destination address
    memcpy(p_hdr->src.addr, p_card->hwaddr, p_card->hwaddr_len);    // fill source address
#if 0
    TY_LOGDEBUG("dest: %x:%x:%x:%x:%x:%x:%x:%x, src: %x:%x:%x:%x:%x:%x:%x:%x, seq:0x%02x, oft:0x%02x, opt:0x%x\n",
                (uint8_t)p_hdr->dest.addr[0], (uint8_t)p_hdr->dest.addr[1], (uint8_t)p_hdr->dest.addr[2], (uint8_t)p_hdr->dest.addr[3],
                (uint8_t)p_hdr->dest.addr[4], (uint8_t)p_hdr->dest.addr[5], (uint8_t)p_hdr->dest.addr[6], (uint8_t)p_hdr->dest.addr[7],
                (uint8_t)p_hdr->src.addr[0], (uint8_t)p_hdr->src.addr[1], (uint8_t)p_hdr->src.addr[2], (uint8_t)p_hdr->src.addr[3],      
                (uint8_t)p_hdr->src.addr[4], (uint8_t)p_hdr->src.addr[5], (uint8_t)p_hdr->src.addr[6], (uint8_t)p_hdr->src.addr[7],
                p_hdr->seqnum, p_hdr->offset, p_hdr->options);
#endif

    // pbuf insert to oput_msg list,
    // if data packet too long we need data fragmentation
    if (p->len <= p_card->mtu) {
        // fill totle len
        p_hdr->len = data_len + MAC_HDR_LEN;
        ret = oput_msg_handler(p, p_card);
    }
#if DATA_OUTPUT_FRAG
    else {
        TY_LOGDEBUG("enter to frage handle.");
        ret = packet_frag(p, p_card, (struct mac_addr*)dest_addr);
    }
#endif

    return ret;
}

/**
 * @brief: mac layer output function
 * @param {in} dest_addr: destination addr
 * @param {in} src_addr: source addr
 * @param {in} p_data: pointer of data which will be send
 * @return: result
 */
int8_t mac_output_to_src(uint8_t* dest_addr, uint8_t* src_addr, send_data_t* p_data)
{
    int8_t ret;
    struct pbuf* p_buf;
    struct rf_card* p_card;

    if (dest_addr == NULL || p_data == NULL) {
        TY_LOGERROR("args error.");
        return ERR_PARA;
    }

    // p_data->buff is NULL will be send frame head
    if (p_data->len > 0xFFFE) {
        TY_LOGERROR("args error.");
        return ERR_PARA;
    }
    
    p_card = rf_card_match(src_addr);
    if (p_card == NULL) {
        TY_LOGERROR("rf card find error.");
        return SEND_RTE_ERR;
    }

    p_buf = pbuf_alloc(PBUF_MAC, p_data->len);
    if (p_buf == NULL) {
        TY_LOGERROR("pbuf alloc error.");
        return ERR_MEM;
    }
    
    ret = pbuf_take(p_buf, p_data->buf, p_data->len);
    if (ret != ERR_OK) {
        pbuf_free(p_buf);
        TY_LOGERROR("pbuf take error.");
        return ERR_OTHER;
    }

    p_buf->data_id = p_data->data_id;
    p_buf->card_idx = get_card_index(p_card);
    PBUF_PRIORITY_SET(p_buf, p_data->priority);

    ret = mac_output(p_buf, p_card, p_data->len, dest_addr, 
                     p_data->frame_type, p_data->ack_req);

    return ret;
}

