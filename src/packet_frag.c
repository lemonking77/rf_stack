/*
 * @Author: Lux1206
 * @Email: 862215606@qq.com
 * @LastEditors: Please set LastEditors
 * @FileName: &file name&
 * @Description: 
 * @Date: 2020-11-09 09:57:18
 * @LastEditTime: 2021-03-19 10:22:12
 */
#include <string.h>

#include "rf_stack_cfg.h"
#include "rf_stack_opt.h"
#include "packet_frag.h"
#include "output_mgr.h"

// zigbee: the maximum length of packet data is 2K, 
// and each payload in the data packet is up to 100 bytes, 
// so the maximum number of packets per device is 21;
#define REASS_MAX_PBUFS 			(MAC_MAX_DEV_NUM*21)

// 21 packs can hold 2K data
#define REASS_DATA_MAX_AGE          21

// packet reassembly status
#define REASS_VALIDATE_FINISHED     1
#define REASS_VALIDATE_DOING        0
#define REASS_VALIDATE_OVERLAP      -1
#define REASS_VALIDATE_INVALID      -2

/* virtual header of pbuf, used for packet reassembly */
struct reass_hdr {
    struct pbuf* next_p;
    uint16_t start;
    uint16_t end;
} __PACKED;

uint8_t g_reass_pbuf_cunt;
static struct reass_data* p_reass_list;

static int8_t free_reass_data_item_and_pbuf(struct reass_data *p_reass, struct reass_data *prev);

/**
 * @brief: packet reassembly timer
 * @param {none}
 * @return: none
 */
void packet_reass_tmr(void)
{
    struct reass_data *tmp;
    struct reass_data *p_reass, *prev = NULL;

    p_reass = p_reass_list;
    while (p_reass != NULL) {
        if (p_reass->tmr < REASS_DATA_MAX_AGE) {
            p_reass->tmr++;
            prev = p_reass;
            p_reass = p_reass->next;
        } 
        else {
            // reassembly timed out
            tmp = p_reass;
            // get the next pointer before freeing
            p_reass = p_reass->next;
            TY_LOGDEBUG("reass data old age.");
            // free the helper struct and all enqueued pbufs
            free_reass_data_item_and_pbuf(tmp, prev);
        }
    }
}

/**
 * @brief: creat packet reassembly structure
 * @param {none}
 * @return: none
 */
struct reass_data* packet_reass_data_new(struct mac_hdr* p_hdr)
{
    struct reass_data* p_reass;

    p_reass = (struct reass_data*)ty_malloc(sizeof(struct reass_data));
    if (p_reass == NULL) {
        TY_LOGERROR("reass data malloc fail.");
        return NULL;
    }
    
    memset(p_reass, 0, sizeof(struct reass_data));
    p_reass->tmr = 0;

    // insert the head of the linked list
    p_reass->next = p_reass_list;
    p_reass_list = p_reass;

    memcpy(&(p_reass->hdr), p_hdr, MAC_HDR_LEN);
    
    return p_reass;
}

/**
 * @brief: remove reass_data itme from reass_data_list
 * @param {in} p_reass: point to reass_data itme which will remove
 * @param {in} prev: the previous item of p_reass
 * @return: none
 */
static void reass_data_item_remove_from_list(struct reass_data *p_reass, struct reass_data *prev)
{
    if (p_reass_list == p_reass) {
        // it was the first in the list
        p_reass_list = p_reass->next;
    } else {
        // it wasn't the first, so it must have a valid 'prev'
        if (prev != NULL) {
            prev->next = p_reass->next;
        }
    }
    TY_LOGDEBUG("reass data removed.");
    ty_free(p_reass);
}

/**
 * @brief: remove the reass_data item and the pbuf below from the reass_data_list
 * @param {in} p_reass: point to reass_data itme which will remove
 * @param {in} prev: the previous item of p_reass
 * @return: none
 */
static int8_t free_reass_data_item_and_pbuf(struct reass_data *p_reass, struct reass_data *prev)
{
    uint16_t pbufs_freed = 0;
    uint16_t clen;
    struct pbuf *p_buf;
    struct reass_hdr *vs_reass_hdr;

    // first release pbuf under reass_data
    p_buf = p_reass->p;
    while (p_buf != NULL) {
        struct pbuf *pcur;
        vs_reass_hdr = (struct reass_hdr *)p_buf->payload;
        pcur = p_buf;
        // get the next pointer before freeing
        p_buf = vs_reass_hdr->next_p;
        clen = pbuf_counter(pcur);
        pbufs_freed = (uint16_t)(pbufs_freed + clen);
        TY_LOGDEBUG("+++++reass free pbuf %d.",pbuf_counter(pcur));
        pbuf_free(pcur);
    }
    // free reass_data
    reass_data_item_remove_from_list(p_reass, prev);
    if (g_reass_pbuf_cunt >= pbufs_freed) {
        g_reass_pbuf_cunt -= pbufs_freed;
    }

  return pbufs_freed;
}

/**
 * @brief: reassembled data packet insertion and verification
 * @param {in} p_reass: point to reass_data itme
 * @param {in} new_p: new pbuf to insert
 * @param {in} is_last: is last packet
 * @return: none
 */
static int8_t packet_reass_inserte_and_validate(struct reass_data* p_reass, struct pbuf* new_p, uint8_t is_last)
{
    struct reass_hdr* vs_hdr, *vs_hdr_tmp, *pre_vs_hdr = NULL; 
    struct mac_hdr* p_hdr;
    struct pbuf* p_buf;
    uint16_t len, offset_len;
    uint8_t valid = 1;

    p_hdr = (struct mac_hdr*)new_p->payload;
    len = p_hdr->len - MAC_HDR_LEN;
    offset_len = HDR_OFFSET_BYTES(p_hdr);
    
    vs_hdr = (struct reass_hdr*)new_p->payload;
    vs_hdr->next_p = NULL;
    vs_hdr->start = offset_len;
    vs_hdr->end = offset_len + len;
    
    TY_LOGDEBUG("vs hdr start len %d, end len %d.",vs_hdr->start,vs_hdr->end);
    if (vs_hdr->end < offset_len) {
        //  overflow, cannot handle this
        TY_LOGERROR("vs hdr len error.");
        return REASS_VALIDATE_INVALID;
    }

/* 遍历p_reass上的pbuf */
/* 分包链表上中间缺少了部分数据包(这种情况不会发生，理由：
    1、发送方式按顺序传输，且是单跳情况；
    2、数据的发送是单跳且基于ack尽力传输的；
    3、发送方数据发送超时会重新发送一个大包) 
*/ 
    for (p_buf = p_reass->p; p_buf != NULL;) {

        vs_hdr_tmp = (struct reass_hdr*)p_buf->payload;
        TY_LOGDEBUG("tmp vs hdr start len %d, end len %d.",vs_hdr_tmp->start,vs_hdr_tmp->end);

        if (vs_hdr->start == vs_hdr_tmp->end) {
            // sequentially received data
            if (vs_hdr_tmp->next_p == NULL) {
                vs_hdr_tmp->next_p = new_p;
            }
            else {
                // duplicate data received
                struct reass_hdr* vs_hdr_next = NULL;
                vs_hdr_next = (struct reass_hdr*)(vs_hdr_tmp->next_p->payload);
                if (vs_hdr->start == vs_hdr_next->start) {
                    return REASS_VALIDATE_OVERLAP;
                } 
            }
            break;
        }
        else if (vs_hdr->start < vs_hdr_tmp->end) {
            // data overlap
            return REASS_VALIDATE_OVERLAP;
        }
        // other cases will be handled in p_buf == NULL
        
        p_buf = vs_hdr_tmp->next_p;
        pre_vs_hdr = vs_hdr_tmp;
    }

    // did not find a suitable position in the package reassembly list
    if (p_buf == NULL) {
        // the first one is empty
        if (pre_vs_hdr == NULL) {
            p_reass->p = new_p;
        }
        else {
            // if there is a disorder, discard all pbuf directly.
            // (requires the sender to send in packets in order, and resend all packets if the sending fails)
            return REASS_VALIDATE_INVALID;
        }
    }

    // Receive the last packet of data
    if (is_last) {
        if (valid) {
            // 1. did not receive any data before, received the end frame directly,
            // 2. the start in the first pbuf is not the first frame of the packet, but the end frame is received.
            if ((p_reass->p == NULL) || 
                (((struct reass_hdr *)p_reass->p->payload)->start != 0)) {
                TY_LOGDEBUG("receive last packet error.");
                return REASS_VALIDATE_INVALID;
            }
        }
        return valid ? REASS_VALIDATE_FINISHED : REASS_VALIDATE_DOING;
    }

    return REASS_VALIDATE_DOING;
}

/**
 * @brief: reassembled data packet insertion and verification
 * @param {in} p_reass: point to reass_data itme
 * @param {in} new_p: new pbuf to insert
 * @param {in} is_last: is last packet
 * @return: none
 */
struct pbuf *reass_data_validate_finished(struct reass_data* p_reass)
{
    struct mac_hdr* p_hdr;
    struct pbuf* q_buf;
    struct pbuf* p_buf = NULL;
    struct reass_data* prev;
    struct reass_hdr* vs_reass = NULL;
    
    p_reass->datagram_len += MAC_HDR_LEN;
    TY_LOGDEBUG("frag data totlen %d", p_reass->datagram_len);

    // save the second pbuf before copying the header over the pointer.
    p_buf = ((struct reass_hdr *)p_reass->p->payload)->next_p;

    // copy the original mac header back to the first pbuf.
    p_hdr = (struct mac_hdr *)(p_reass->p->payload);
    memcpy(p_hdr, &p_reass->hdr, MAC_HDR_LEN);
    p_hdr->len = p_reass->datagram_len;
    p_hdr->offset = 0;
    p_hdr->sum = 0;
    
    q_buf = p_reass->p;
    // process the subsequent pbuf and re-train it into a pbuf chain.
    while(p_buf != NULL) {
        vs_reass = (struct reass_hdr*)p_buf->payload;
        // the subsequent pbuf->payload all point to the data area.
        pbuf_remove_header(p_buf, MAC_HDR_LEN);
        pbuf_cat(q_buf, p_buf);
        p_buf = vs_reass->next_p;
    }
    
    // find the previous entry in the linked list 
    if (p_reass == p_reass_list) {
        prev = NULL;
    } 
    else {
        // find the previous item of p_reass that needs to be released.
        for (prev = p_reass_list; prev != NULL; prev = prev->next) {
            if (prev->next == (struct reass_data*)vs_reass) {
                break;
            }
        }
    }

    // release the sources allocate for the fragment queue entry.
    reass_data_item_remove_from_list(p_reass, prev);

    // and adjust the number of pbufs currently queued for reassembly. 
    g_reass_pbuf_cunt -= pbuf_counter(q_buf);

    // alloc a new big pbuf to storage data after reassembly. 
    p_buf = pbuf_alloc(PBUF_RAW, p_reass->datagram_len);
    if (p_buf == NULL) {
        TY_LOGDEBUG("reass new pbuf alloc error.");
        goto null_return;
    }
    // copy old pbuf head to new pbuf head
    // memcpy(p_buf, q_buf, PBUF_STRUCT_SIZE); // will recover p_buf->payload adddr
    
    p_buf->len = q_buf->len;
    p_buf->tot_len = q_buf->tot_len;
    p_buf->data_id = q_buf->data_id;
    p_buf->card_idx = q_buf->card_idx;
    p_buf->flag = q_buf->flag;
    pbuf_copy_to_swap(q_buf, p_buf->payload, p_reass->datagram_len, 0);

null_return:
    TY_LOGDEBUG("****reass free pbuf %d.",pbuf_counter(q_buf));
    pbuf_free(q_buf);
    return p_buf;
}

/**
 * @brief: data reassembly
 * 1、发送方必须是发送完一个大包数据的多个碎片数据后才可以发送下一个大包数据；
 * 2、发送方必须是收到接收方的ack后才能发送下一个碎片数据包，单个碎片超时则需要把整个大包从发送链表上删除；
 * 3、接收方在取碎片数据的时候，依据数据头中是否是最后一包来判断是否收到了完成数据；
 * @param {in} p: point to input packet
 * @return: pbuf after reassembly
 */
struct pbuf* packet_reass(struct pbuf* p)
{
    int8_t valid;
    uint8_t is_last;
    uint8_t pbuf_cunt;
    uint8_t packet_len;
    uint16_t offset_len;
    struct pbuf* p_buf = NULL;
    struct mac_hdr* p_hdr;
    struct reass_data* p_reass;
    
    p_hdr = (struct mac_hdr*)p->payload;
    
    packet_len = p_hdr->len;

    if (packet_len <= MAC_HDR_LEN) {
        TY_LOGERROR("packet_reass: packet len error.");
        goto null_return;
    }

    offset_len = HDR_OFFSET_BYTES(p_hdr);
    packet_len -=  MAC_HDR_LEN;

    pbuf_cunt = pbuf_counter(p);
    // limit the maximum number of pbuf used for message reassembly
    if (g_reass_pbuf_cunt + pbuf_cunt > REASS_MAX_PBUFS) {
        TY_LOGERROR("packet_reass: packets overflow.");
        goto null_return;
    }

    // traverse the reass_data list to find the reass_data item corresponding to the input package
    for (p_reass = p_reass_list; p_reass != NULL; p_reass = p_reass->next) {
        // create a p_reass structure for each large packet with a different seqnum
        if (ADDR_AND_SEQNUM_MATCH(&p_reass->hdr, p_hdr)) {
            TY_LOGDEBUG("find reass list item.");
            break;
        }
    }

    if (p_reass == NULL) {
        p_reass = packet_reass_data_new(p_hdr);
        if (p_reass == NULL) {
            goto null_return;
        }
    }
    else {
        // check receive first fragmented data or not
        if (((p_hdr->offset) & OFFSET_MASK) == 0 &&
            ((p_reass->hdr.offset) & OFFSET_MASK) != 0) {
            goto null_return;
        }
    }

    // check receive last fragmented data or not
    is_last = (p_hdr->offset & MF_FLG) == 0;
    if (is_last) {
        // check receive data len overflow
        uint16_t datagram_len = (offset_len + packet_len);
        if ((datagram_len < offset_len) || (datagram_len > (0xFFFF - MAC_HDR_LEN))) {
            goto null_return_reass;
        }
    }
    TY_LOGDEBUG("is last packet %d",is_last);

    valid = packet_reass_inserte_and_validate(p_reass, p, is_last);
    if (valid == REASS_VALIDATE_INVALID) {
        goto null_return_reass;
    }
    else if(valid == REASS_VALIDATE_OVERLAP){
        goto null_return;
    }
    g_reass_pbuf_cunt += pbuf_cunt;
    if (is_last) {
        p_reass->datagram_len = offset_len + packet_len;
        TY_LOGDEBUG("recv last packet,now totlen %d",p_reass->datagram_len);
    }

    if (valid == REASS_VALIDATE_FINISHED) {
        p_buf = reass_data_validate_finished(p_reass);
        TY_LOGDEBUG("len %d, totlen %d",p_buf->len, p_buf->tot_len);
        pbuf_free(p);
        return p_buf; 
    }
    return NULL;

null_return_reass:
    if (p_reass->p == NULL) {
        // dropped pbuf after creating a new datagram entry: remove the entry, too 
        TY_LOGDEBUG("not first although just enqueued.");
        reass_data_item_remove_from_list(p_reass, NULL);
    }

null_return:
    TY_LOGDEBUG("reass free pbuf %d.",pbuf_counter(p));
    pbuf_free(p);
    return NULL;
}

/**
 * @brief: create new fragmented pbuf
 * @param {in} old_hdr: old pbuf head
 * @param {in} offset: new packet offset
 * @param {in} len: len of new pbuf
 * @return: new pbuf
 */
struct pbuf* packet_frag_buf_creat(struct mac_hdr* old_hdr, uint16_t offset, uint16_t len)
{
    struct pbuf* p_buf;
    struct mac_hdr* p_hdr;

    p_buf = pbuf_alloc(PBUF_RAW, len);
    if (p_buf == NULL) {
        return NULL;
    }

    p_hdr = (struct mac_hdr*)p_buf->payload;
    memcpy(p_hdr, old_hdr, MAC_HDR_LEN);
    p_hdr->offset = offset;
    p_hdr->len = len;
    p_hdr->sum = 0;
    p_hdr->sum = check_sum((uint8_t *)(void *)p_hdr, p_hdr->len);

    pbuf_remove_header(p_buf, MAC_HDR_LEN);

    return p_buf;
}

/**
 * @brief: fragmented packet
 * @param {in} p: packets that need to be fragmented
 * @param {in} p_card: rf card
 * @param {in} dest: destination address
 * @return: ERR_OK: 0 sucess
 */
int8_t packet_frag(struct pbuf* p, struct rf_card* p_card, const struct mac_addr* dest)
{
    uint8_t i;
    uint8_t pbuf_num;
    uint8_t last_pbuf_len;
    uint16_t data_len;
    struct pbuf* q_buf;
    struct pbuf* first_pbuf;
    struct mac_hdr* old_hdr;

    uint16_t tmp = 0;
    uint16_t ofo = 0;
    const uint8_t nfb = (p_card->mtu - MAC_HDR_LEN) / 4;
    uint8_t new_pbuf_len = p_card->mtu;

    data_len = p->tot_len - MAC_HDR_LEN;
    old_hdr = (struct mac_hdr*)p->payload;

    pbuf_num = (data_len / (p_card->mtu - MAC_HDR_LEN)) + 1;
    last_pbuf_len = data_len % (p_card->mtu - MAC_HDR_LEN);

    tmp = (ofo | MF_FLG);
    first_pbuf = packet_frag_buf_creat(old_hdr, tmp, new_pbuf_len);
    if (first_pbuf == NULL) {
        pbuf_free(p);
        return ERR_MEM;
    }
    first_pbuf->data_id = p->data_id;
    first_pbuf->card_idx = p->card_idx;
    TY_LOGDEBUG("offset: %d, MF: %d",(OFFSET_MASK & ofo),(tmp & 0xE000));

    for (i = 1; i < pbuf_num; i++) {
        ofo += nfb;
        tmp = (OFFSET_MASK & ofo);
        if (pbuf_num - 1 > i) {
            tmp = (ofo | MF_FLG);
            new_pbuf_len = p_card->mtu;
        }
        else {
            new_pbuf_len = last_pbuf_len + MAC_HDR_LEN;
        }
        // create a new fragmented pbuf 
        q_buf = packet_frag_buf_creat(old_hdr, tmp, new_pbuf_len);
        if (q_buf == NULL) {
            pbuf_free(p);
            pbuf_free(first_pbuf);
            return ERR_MEM;
        }
        q_buf->data_id = p->data_id;
        q_buf->card_idx = p->card_idx;
        TY_LOGDEBUG("offset: %d, MF: %d",(OFFSET_MASK & ofo),(tmp & 0xE000));
        // fragmentation of pbuf into a chain
        pbuf_cat(first_pbuf, q_buf);
    }

    // remove old pbuf payload pointer to data
    pbuf_remove_header(p, MAC_HDR_LEN);
    // copy old pbuf payload data to fragmented pbuf
    pbuf_take(first_pbuf, p->payload, p->tot_len);
    pbuf_free(p);
    
    for (q_buf = first_pbuf; q_buf != 0; q_buf = q_buf->next) {
        if (pbuf_add_header(q_buf, MAC_HDR_LEN) != ERR_OK) {
            pbuf_free(first_pbuf);
            return ERR_BUF;
        }
        // TY_LOGDEBUG("tot len %d, len %d\n",q_buf->tot_len,q_buf->len);
        // old_hdr = (struct mac_hdr*)q_buf->payload;
        // TY_LOGDEBUG("dest: %x:%x:%x:%x:%x:%x:%x:%x, src: %x:%x:%x:%x:%x:%x:%x:%x, seq:0x%02x, oft:0x%02x, opt:0x%x\n",
        //     (uint8_t)old_hdr->dest.addr[0], (uint8_t)old_hdr->dest.addr[1], (uint8_t)old_hdr->dest.addr[2], (uint8_t)old_hdr->dest.addr[3],
        //     (uint8_t)old_hdr->dest.addr[4], (uint8_t)old_hdr->dest.addr[5], (uint8_t)old_hdr->dest.addr[6], (uint8_t)old_hdr->dest.addr[7],
        //     (uint8_t)old_hdr->src.addr[0], (uint8_t)old_hdr->src.addr[1], (uint8_t)old_hdr->src.addr[2], (uint8_t)old_hdr->src.addr[3],      
        //     (uint8_t)old_hdr->src.addr[4], (uint8_t)old_hdr->src.addr[5], (uint8_t)old_hdr->src.addr[6], (uint8_t)old_hdr->src.addr[7],
        //     old_hdr->seqnum, old_hdr->offset, old_hdr->options);
    }
    
    // mount the fragmented pbuf chain to the sending list
    oput_msg_add_packet_to_list(first_pbuf);
    
    return ERR_OK;
}

