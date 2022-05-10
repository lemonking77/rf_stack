/*
 * @Author: Lux1206
 * @Email: 862215606@qq.com
 * @LastEditors: Please set LastEditors
 * @FileName: &file name&
 * @Description: 
 * @Date: 2020-11-09 09:57:18
 * @LastEditTime: 2021-03-20 16:48:43
 */
#include <string.h>

#include "rf_stack_opt.h"
#include "rf_stack_cfg.h"
#include "radio_card.h"
#include "output_mgr.h"
#include "mac_net.h"
#include "pbuf.h"

/*
 * 重点：
 * 1.输出链表上的pbuf的释放问题：重传超时、收到对对应的ACK
 * 2.输出链表释放问题：会在每一次是否链表上的pbuf时进行判断，packets为0则删除
 * 3.输入队列项清除问题：超时老化清除
 * 4.相关操作函数的功能测试
*/

// data max retry times.
#define OUT_MSG_RETRY_ATTEMPTS      MAC_MAX_RETRIES

// number of concurrent devices.
#define OPUT_MSG_ITEM_MAX_NUM       MAC_MAX_DEV_NUM

#define OPUT_MSG_OLD_AGE            (300)    // 6s

// the maximum number of pbufs that can be mounted on each device.
// because the maximum amount of packet data is 2k and the maximum 
// payload is 100 bytes, so the maximum number of pbufs is 21.
#define OPUT_MSG_MAX_PBUFS          21

/* 发送时pbuf释放时机：插入链表失败(满了)、重传超时、收到对对应的ACK */
/* 接收时pbuf释放时机：收到数据包出错(校验出错、长度过短)，收到ack数据包 */

/* output message structure */
struct oput_msg {
    struct oput_msg *next;  // next oput_msg in singly linked oput_msg chain
    struct pbuf* p;         // pointer of pbuf which will be send
    uint8_t packets;        // packets number on oput_msg list,a packet may consist of multiple pbufs
    uint16_t old_age;
    uint16_t seqnum;        // output message data packet seqnum
    struct mac_addr addr;   // output message destination address
};

#define OPUT_MSG_STRUCT_SIZE (sizeof(struct oput_msg))

static struct oput_msg* g_curr_msg = NULL;
static struct oput_msg* oput_msg_list = NULL;

/**
 * @brief: generate a new oput_msg list item
 * @param {in} addr: mac addr
 * @param {in} plen: none
 * @param {in} addr: addr
 * @return: pointer of oput_msg which removed
 */
static struct oput_msg* oput_msg_new(struct mac_addr* addr, uint8_t plen)
{
    struct oput_msg* p_msg;
    struct oput_msg* q_msg;

    p_msg = (struct oput_msg*)ty_malloc(OPUT_MSG_STRUCT_SIZE);
    if (p_msg == NULL) {
        TY_LOGERROR("oput_msg_new malloc failed");
        return NULL;
    }
    memset(p_msg, 0, OPUT_MSG_STRUCT_SIZE);
    memcpy(p_msg->addr.addr, addr->addr, MAX_HWADDR_LEN);

    // tail insertion
    if (oput_msg_list == NULL) {
        
        uint32_t irqState = ENTER_CRITICAL();
        g_curr_msg = p_msg;
        oput_msg_list = p_msg;
        EXIT_CRITICAL(irqState);

        goto ok_return;
    }

    for (q_msg = oput_msg_list; q_msg->next != NULL; q_msg = q_msg->next) {
    }
    q_msg->next = p_msg;

ok_return:
    TY_LOGDEBUG("create oput msg ok.");

    return p_msg;
}

/**
 * @brief: get the number of items on the oput_msg list
 * @param {in} p_msg: oput_msg list
 * @return: numbers
 */
static uint8_t oput_msg_counter(const struct oput_msg* p_msg)
{
    uint8_t len = 0;;

    while (p_msg != NULL) {
        ++len;
        p_msg = p_msg->next;
    }

    return len;
}

/**
 * @brief: find items matching addr on oput_msg list item
 * @param {in} addr: mac addr
 * @return: pointer of oput_msg
 */
static struct pbuf* packet_find_with_ack_hdr(struct oput_msg* p_msg,struct mac_hdr* p_hdr)
{
    struct pbuf* p_buf;
    struct mac_hdr* q_hdr;
    
    // traverse the list to find device
    for (p_buf = p_msg->p; p_buf != NULL; p_buf = p_buf->next) {
        q_hdr = (struct mac_hdr*)p_buf->payload;

        if (q_hdr->offset == p_hdr->offset &&
            q_hdr->seqnum == p_hdr->seqnum) {
            return p_buf;
        }
    }

    return NULL;
}

/**
 * @brief: find items matching addr on oput_msg list item
 * @param {in} addr: mac addr
 * @return: pointer of oput_msg
 */
static struct oput_msg* oput_msg_find_match_addr(struct mac_addr* addr)
{
    struct oput_msg* p_msg;
    
    // traverse the list to find device
    for (p_msg = oput_msg_list; p_msg != NULL; p_msg = p_msg->next) {
        
        if (p_msg->packets != 0 &&
            MAC_ADDR_CMP(&p_msg->addr, addr)) {
            return p_msg;
        }
    }

    return NULL;
}

/**
 * @brief: remove pbuf
 * @param {in} hdr_pbuf: pointer to pbuf list hdr
 * @param {in} tmp_pbuf: pointer to pbuf which need to delete
 * @param {in} prev_pbuf: pointer to previous pbuf which need to delete
 * @return: result
 */

void oput_pbuf_move(struct oput_msg* p_msg, struct pbuf* tmp_pbuf, struct pbuf* prev_pbuf)
{
    if(prev_pbuf == NULL) {
        p_msg->p = tmp_pbuf->next;
    }
    else {
        prev_pbuf->next = tmp_pbuf->next;
    }
    tmp_pbuf->next = NULL;
    p_msg->packets--;
    
    pbuf_free(tmp_pbuf);
}

/**
 * @brief: recv ack delete pbuf
 * @param {in} p_msg: pointer to msg
 * @param {in} seqnum: seq of p_buf which need to delete
 * @return: noll
 */
static int8_t packet_recv_ack_move(struct oput_msg* p_msg, uint16_t seqnum)
{
    struct mac_hdr *p_hdr;
    struct pbuf *tmp;
    struct pbuf *q_buf, *prev = NULL;

    q_buf = p_msg->p;
    while (q_buf != NULL) {
        p_hdr = (struct mac_hdr *)q_buf->payload;
        // uart_printf(0, ">>>SRC:%x:%x, SEQ:0x%02x, OFFSET %d\r\n",
        // (uint8_t)p_hdr->src.addr[6], (uint8_t)p_hdr->src.addr[7],p_hdr->seqnum, HDR_OFFSET_BYTES(p_hdr));
        
        if (p_hdr->seqnum != seqnum) {            
            prev = q_buf;
            q_buf = q_buf->next;
        } 
        else {
            // reassembly timed out
            tmp = q_buf;
            // get the next pointer before freeing
            q_buf = q_buf->next;
            
            oput_pbuf_move(p_msg, tmp, prev);
            // fragmentation data needs to delete all pbufs with the same seqnum one by one
            if(!IS_MORE_FRAG_DATA(p_hdr)) {
                return ERR_OK;
            }
        }
    }

    return ERR_OTHER;
}

/**
 * @brief: send timeout delete pbuf
 * @param {in} p_msg: pointer to msg
 * @param {in} p_buf: pointer to p_buf which need to delete
 * @return: noll
 */
static int8_t packet_send_retry_times_out_move(struct oput_msg* p_msg, struct pbuf* p_buf)
{
    struct pbuf *tmp;
    struct pbuf *q_buf, *prev = NULL;

    q_buf = p_msg->p;
    while (q_buf != NULL) {
        if (q_buf != p_buf) {            
            prev = q_buf;
            q_buf = q_buf->next;
        } 
        else {
            // reassembly timed out
            tmp = q_buf;
            // get the next pointer before freeing
            q_buf = q_buf->next;

            oput_pbuf_move(p_msg, tmp, prev);
            return ERR_OK;
        }
    }
    
    return ERR_OTHER;
}

/**
 * @brief: insert packet buf to msg (pbufs must with same seqnum)
 * @param {in} p_msg: pointer of oput_msg
 * @param {in} p_buf: pointer of pbuf which will insert
 * @param {in} seqnum: packet seqnum
 * @return: result
 */
static int8_t insert_oput_msg_insert_packet(struct oput_msg* p_msg, struct pbuf* buf, uint16_t seqnum)
{
    struct mac_hdr* p_hdr;
    struct pbuf *p_buf, *prev_buf = NULL;

    if (p_msg->p == NULL) {
        p_msg->p = buf;
        p_msg->packets++;
        TY_LOGDEBUG("oput_msg frist times add pbuf.");
        goto ok_return;
    }

    // traverse the list to find the same seqnum packets 
    for (p_buf = p_msg->p; p_buf != NULL; p_buf = p_buf->next) {
        p_hdr = (struct mac_hdr *)p_buf->payload;
        // check seqnum already exist
        if (seqnum == p_hdr->seqnum) {
            TY_LOGDEBUG("output %d seq overlap.",seqnum);
            goto ok_return;
        }
        prev_buf = p_buf;
    }
    // only link the pbuf to msg without increasing the ref
    prev_buf->next = buf;
    p_msg->packets++;

ok_return:
    TY_LOGDEBUG("all oput msg num %d.",oput_msg_counter(p_msg));
    TY_LOGDEBUG("pbuf num %d.",pbuf_counter(p_msg->p));
    return ERR_OK;
}

/**
 * @brief: move msg itme func
 * @param {in}  p_msg: pointer to msg which need to detete
 * @param {in} p_prev: pointer to the previous item that needs to be deleted from the message
 * @return: noll
 */
void oput_msg_item_move(struct oput_msg *p_msg, struct oput_msg *p_prev)
{
    uint32_t irqState = ENTER_CRITICAL();
    if(p_prev == NULL) {
        oput_msg_list = p_msg->next;
    }
    else {
        p_prev->next = p_msg->next;
    }
    p_msg->next = NULL;

    if(p_msg == g_curr_msg) {
        if(p_msg->next != NULL) {
            g_curr_msg = p_msg->next;
        }
        else {
            g_curr_msg = oput_msg_list;
        }
    }
    EXIT_CRITICAL(irqState);
    
    if (p_msg->packets != 0) {
        pbuf_free(p_msg->p);
    }
    ty_free(p_msg);
} 

/**
 * @brief: out_msg item time out func
 * @param {in} noll
 * @return: noll
 */
void oput_msg_old_age_tmr(void)
{
    struct oput_msg *tmp;
    struct oput_msg *p_msg, *prev = NULL;

    p_msg = oput_msg_list;
    while (p_msg != NULL) {
        if (p_msg->old_age < OPUT_MSG_OLD_AGE) {
            p_msg->old_age++;
            
            prev = p_msg;
            p_msg = p_msg->next;
        } 
        else {
            // reassembly timed out
            tmp = p_msg;
            // get the next pointer before freeing
            p_msg = p_msg->next;
            oput_msg_item_move(tmp, prev);
        }
    }
}

/**
 * @brief: add packet to oput_msg list
 * @param {in} p_buf: pointer of pbuf which will insert
 * @return: result
 */
int8_t oput_msg_add_packet_to_list(struct pbuf* p_buf)
{
    uint8_t ret;
    uint8_t dev_num;
    uint8_t new_plen, tot_plen;
    struct mac_hdr* p_hdr;
    struct oput_msg *tmp, *p_msg, *prev = NULL;
    
#if 0
    struct pbuf* q_buf;
    for (q_buf = p_buf; q_buf != NULL; q_buf = q_buf->next) {
        p_hdr = (struct mac_hdr*)q_buf->payload;
        TY_LOGDEBUG("dest: %x:%x:%x:%x:%x:%x:%x:%x, src: %x:%x:%x:%x:%x:%x:%x:%x, opt:0x%x",
                    (uint8_t)p_hdr->dest.addr[0], (uint8_t)p_hdr->dest.addr[1], (uint8_t)p_hdr->dest.addr[2], (uint8_t)p_hdr->dest.addr[3],
                    (uint8_t)p_hdr->dest.addr[4], (uint8_t)p_hdr->dest.addr[5], (uint8_t)p_hdr->dest.addr[6], (uint8_t)p_hdr->dest.addr[7],
                    (uint8_t)p_hdr->src.addr[0], (uint8_t)p_hdr->src.addr[1], (uint8_t)p_hdr->src.addr[2], (uint8_t)p_hdr->src.addr[3],      
                    (uint8_t)p_hdr->src.addr[4], (uint8_t)p_hdr->src.addr[5], (uint8_t)p_hdr->src.addr[6], (uint8_t)p_hdr->src.addr[7],
                    p_hdr->options);
        
        TY_LOGDEBUG("offset: %d, MF: %d",(OFFSET_MASK & p_hdr->offset),(p_hdr->offset & 0xE000));
        TY_LOGDEBUG("packet len: %d",p_hdr->len);
        TY_LOGDEBUG("payload: %d, %d, %d\n",*(uint8_t *)&q_buf->payload[0],*(uint8_t *)&q_buf->payload[1],*(uint8_t *)&q_buf->payload[11]);
    }
#endif
    
    new_plen = pbuf_counter(p_buf);
    p_hdr = (struct mac_hdr *)p_buf->payload;

    // ack frame need not inserte output list 
    if (FRAME_TYPE_GET(p_hdr) == ACK_FRAME) {
        TY_LOGERROR("ack frame need not inster.");
        pbuf_free(p_buf);
        return ERR_OK;
    }

    dev_num = oput_msg_counter(oput_msg_list);
    // concurrent device number check
    if (dev_num > OPUT_MSG_ITEM_MAX_NUM) {
        TY_LOGERROR("too many devices %d", dev_num);
        pbuf_free(p_buf);
        return ERR_VAL;
    }
    
    // check the number of pbuf mounted on the device
    if (new_plen > OPUT_MSG_MAX_PBUFS) {
        TY_LOGERROR("come too many pbuf %d", new_plen);
        pbuf_free(p_buf);
        return ERR_VAL;
    }
    TY_LOGDEBUG("dev_num: %d, new_plen: %d",dev_num, new_plen);

    p_msg = oput_msg_list;
    while (p_msg != NULL) {
        // oput_msg already exists
        if (MAC_ADDR_CMP(&p_hdr->dest, &p_msg->addr)) {
            p_msg->old_age = 0;
            TY_LOGDEBUG("find device in oput_msg.");
            break;
        }

        if (p_msg->old_age < OPUT_MSG_OLD_AGE && p_msg->packets != 0) {
            prev = p_msg;
            p_msg = p_msg->next;
        } 
        else {
            // reassembly timed out
            tmp = p_msg;
            // get the next pointer before freeing
            p_msg = p_msg->next;
            oput_msg_item_move(tmp, prev);
        }
    }
    
    if (p_msg != NULL) {
        // pbuf insert pbuf to 
        tot_plen = new_plen + pbuf_counter(p_msg->p);
        if (tot_plen >= OPUT_MSG_MAX_PBUFS) {
            TY_LOGERROR("total number of pbuf %d", tot_plen);
            pbuf_free(p_buf);
            return ERR_VAL;
        }
        TY_LOGDEBUG("%d pbuf on msg list.",tot_plen);
    }
    else {
        // create a new dev msg
        p_msg = oput_msg_new(&p_hdr->dest, new_plen);
        if (p_msg == NULL) {
            pbuf_free(p_buf);
            TY_LOGERROR("create new oput msg error.");
            return ERR_BUF;
        }
    }
    
    // insert pbuf with same seqnum 
    ret = insert_oput_msg_insert_packet(p_msg, p_buf, p_hdr->seqnum);
    if (ret != ERR_OK) {
        TY_LOGERROR("inserte pbuf ok.");
    }
    return ERR_OK;
}

/**
 * @brief: delete packet buf from oput_msg list after receive ack
 * @param {in} p: pointer of packet buf
 * @return: result
 */
int8_t oput_msg_del_packet_from_list(struct pbuf* p)
{
    int8_t ret;
    struct pbuf* p_buf;
    struct mac_hdr* p_hdr;
    struct oput_msg* p_msg,*q_msg;
    
    if (p == NULL) {
        return ERR_PARA;
    }
    
    p_hdr = (struct mac_hdr*)p->payload;
    p_msg = oput_msg_find_match_addr(&p_hdr->src);
    if (p_msg == NULL) {
        TY_LOGERROR("not find math output_msg.");
        return ERR_OK;
    }
    
    // fragment is allowed and there are more fragment segments.
    if (IS_MORE_FRAG_DATA(p_hdr)) {
        // after the ack of the fragments data arrives, it will not be deleted from oput_msg list, 
        // unless the whole packet will be deleted after the ack of the last fragments data arrives;
        p_buf = packet_find_with_ack_hdr(p_msg, p_hdr);
        if (p_buf != NULL) {
            // when recv fragments data set pbuf send ok but not delete pbuf
            SEND_STATUS_SET(p_buf, SEND_SUCESS);
        }
        return ERR_OK;
    }

    // remove packet from oput_msg
    ret = packet_recv_ack_move(p_msg, p_hdr->seqnum);
    if (ret != ERR_OK) {
        TY_LOGDEBUG("%d seqnum not on output message list.",p_hdr->seqnum);
    }
    
    return ERR_OK;
}

/**
 * @brief: search for pbuf which will be send
 * @param {in} f_msg: pointer of oput_msg which start for search
 * @param {in} t_msg: pointer of oput_msg which end for search
 * @return: pointer of pbuf
 * @note: 1.if it is not a fragmented data pbuf, only one pbuf 
 *        under oput_msg is sent each time, and next time it jumps to another oput_msg;
 *        2.if it is fragmented data pbuf, all fragmented data pbufs will be sent;
 */
static struct pbuf* oput_msg_send_pbuf_iterator(struct oput_msg* p_msg)
{
    struct mac_hdr *p_hdr;
    struct pbuf *ret_buf = NULL;
    struct pbuf *p_buf = p_msg->p;
    
    if(p_buf == NULL ||
       p_msg->packets == 0 || 
       p_msg->old_age >= OPUT_MSG_OLD_AGE) {

        return NULL;
    }
    
    TY_LOGDEBUG("packets: %d, seqnum: %d, arg: %d",p_msg->packets, p_msg->seqnum, p_msg->old_age);

    while(p_buf != NULL) {
        p_hdr = (struct mac_hdr*)p_buf->payload;

        // find the status of pbuf which is not sucess 
        if(SEND_STATUS_GET(p_buf) != SEND_SUCESS && 
            p_buf->send_retry < OUT_MSG_RETRY_ATTEMPTS) {
            return p_buf;
        }
        else if(p_buf->send_retry >= OUT_MSG_RETRY_ATTEMPTS) {
            // delete send times out pbuf
            packet_send_retry_times_out_move(p_msg, p_buf);
            return NULL;
        }
        
        p_buf = p_buf->next;
    }

    return ret_buf;
}

/**
 * @brief: find the pbuf that needs to be sent
 * @param {in} p_msg: pointer of current oput_msg
 * @return: pointer of pbuf
 */
static struct pbuf* send_pbuf_find(struct oput_msg* target_msg)
{
    struct pbuf* p_buf = NULL;

    struct oput_msg *tmp;
    struct oput_msg *p_msg, *prev = NULL;

    p_msg = oput_msg_list;
    while (p_msg != NULL) {
        if (p_msg != target_msg) {

            prev = p_msg;
            p_msg = p_msg->next;
        } 
        else {
            // reassembly timed out
            tmp = p_msg;
            // get the next pointer before freeing
            p_msg = p_msg->next;
            p_buf = oput_msg_send_pbuf_iterator(tmp);
            if(p_buf != NULL) {
                // find pbuf which will be send
                TY_LOGDEBUG("find pbuf.");
                return p_buf;
            }
            else {
                // when the msg without pbuf or the pbuf timeout under msg is deleted, 
                // need to move the current MSG pointer
                uint32_t irqState = ENTER_CRITICAL();
                if(p_msg != NULL) {
                    g_curr_msg = p_msg;
                }
                else {
                    g_curr_msg = oput_msg_list;
                }
                EXIT_CRITICAL(irqState);
            }
        }
    }

    
    return p_buf;
}

/**
 * @brief: output timer
 * @param {type} none
 * @return: none
 */
void oput_msg_tmr(void)
{
    struct pbuf *p_buf;
    struct mac_hdr* p_hdr;
    struct rf_card* p_card;
    
    if (g_curr_msg == NULL) {
        // TY_LOGERROR("no data>>.");
        return;
    }
    
    // the first pbuf in each oput_msg list is the pbuf to be found
    p_buf = send_pbuf_find(g_curr_msg);
    if (p_buf == NULL) {
        // TY_LOGERROR("no data need to send.");
        return;
    }
    
    p_hdr = (struct mac_hdr*)p_buf->payload;

    TY_LOGDEBUG(" seq:0x%02x, oft:0x%02x, opt:0x%x",
                p_hdr->seqnum, p_hdr->offset, p_hdr->options);
    // TY_LOGDEBUG("dest: %x:%x:%x:%x:%x:%x:%x:%x, src: %x:%x:%x:%x:%x:%x:%x:%x, seq:0x%02x, oft:0x%02x, opt:0x%x\n",
    //         (uint8_t)p_hdr->dest.addr[0], (uint8_t)p_hdr->dest.addr[1], (uint8_t)p_hdr->dest.addr[2], (uint8_t)p_hdr->dest.addr[3],
    //         (uint8_t)p_hdr->dest.addr[4], (uint8_t)p_hdr->dest.addr[5], (uint8_t)p_hdr->dest.addr[6], (uint8_t)p_hdr->dest.addr[7],
    //         (uint8_t)p_hdr->src.addr[0], (uint8_t)p_hdr->src.addr[1], (uint8_t)p_hdr->src.addr[2], (uint8_t)p_hdr->src.addr[3],      
    //         (uint8_t)p_hdr->src.addr[4], (uint8_t)p_hdr->src.addr[5], (uint8_t)p_hdr->src.addr[6], (uint8_t)p_hdr->src.addr[7],
    //         p_hdr->seqnum, p_hdr->offset, p_hdr->options);
    
    // get rf card
    p_card = rf_card_get_from_num(p_buf->card_idx - 1);
    if (p_card == NULL) {
        // if no rf card find, set retry_times to max retry attempts, will be delete at next timer loop
        p_buf->send_retry = OUT_MSG_RETRY_ATTEMPTS;
        TY_LOGERROR("no rf card find.");
        return;
    }
    // set len
    p_hdr->sum = 0;
    p_hdr->sum = check_sum((uint8_t *)(void *)p_hdr, p_hdr->len);
    g_curr_msg->seqnum = p_hdr->seqnum;
    // send pbuf out
    p_card->rf_output(p_card, p_buf);

    if (ACK_REQUEST_GET(p_hdr)) {
        // retry send times increase
        p_buf->send_retry++;
    }
    else {
        p_buf->send_retry = OUT_MSG_RETRY_ATTEMPTS;
    }
}

/**
 * @brief: output massage handler
 * @param {in} p: pointer of packet buf
 * @param {in} p_card: pointer of rf card
 * @return: result
 */
int8_t oput_msg_handler(struct pbuf* p_buf, struct rf_card* p_card)
{
    return oput_msg_add_packet_to_list(p_buf);
}