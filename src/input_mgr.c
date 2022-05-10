/*
 * @Author: Lux1206
 * @Email: 862215606@qq.com
 * @LastEditors: Please set LastEditors
 * @FileName: &file name&
 * @Description: 
 * @Date: 2020-12-18 10:17:09
 * @LastEditTime: 2021-03-19 10:17:39
 */
#include <string.h>

#include "rf_stack_opt.h"
#include "rf_stack_cfg.h"
#include "radio_card.h"
#include "input_mgr.h"
#include "mac_net.h"
#include "pbuf.h"


// cache 4 packets for each device to receive data De duplication.
#define IPUT_MSG_QUE_MAX_SIZE       (MAC_MAX_DEV_NUM*10)

// maximum aging time of input data.
#define IPUT_MSG_MAX_AGE            (MAC_MAX_RETRIES+1)

/* input remove duplicates queue item state */
typedef enum {
    QUE_ITEM_EMPTY = 0,
    QUE_ITEM_EXISTED
}IPUT_QUE_T;

/* input message remove duplicates structure */
struct iput_msg {
    IPUT_QUE_T state;       // input queue item state
    uint8_t ctime;          // input message cache time(every 10ms)
    uint16_t seqnum;        // input message data packet seqnum
    uint16_t offset;        // input message offset
    struct mac_addr addr;   // input message source address
};

struct iput_msg iput_msg_que[IPUT_MSG_QUE_MAX_SIZE];

/**
 * @brief: clears input massage cache entries.
 * @param {type} none
 * @return: none
 */
void iput_msg_tmr(void)
{
    uint8_t i;
    uint8_t state;

    for (i = 0; i < IPUT_MSG_QUE_MAX_SIZE; i++) {
        
        state = iput_msg_que[i].state;

        if (state != QUE_ITEM_EMPTY) {

            iput_msg_que[i].ctime++;
            if (iput_msg_que[i].ctime >= IPUT_MSG_MAX_AGE) {
                iput_msg_que[i].state = QUE_ITEM_EMPTY;
            }
        }
    }
}

/**
 * @brief: find free items in the iput_msg queue 
 * @param {type} none
 * @return: pointer of iput_msg
 */
static struct iput_msg* iput_msg_find_empty(void)
{
    uint8_t i;
    uint8_t state;

    for (i = 0; i < IPUT_MSG_QUE_MAX_SIZE; i++) {
        state = iput_msg_que[i].state;
        if (state == QUE_ITEM_EMPTY) {
            return &iput_msg_que[i];
        }
    }
    
    return NULL;
}

/**
 * @brief: find items that match addr and seqnum in the iput queue
 * @param {in} addr: mac addr
 * @param {in} seqnum: number of packet
 * @return: pointer of iput_msg
 */
static struct iput_msg* iput_msg_find_match(struct mac_addr *addr,uint16_t seqnum, uint16_t offset)
{
    uint8_t i;
    uint8_t state;

    for (i = 0; i < IPUT_MSG_QUE_MAX_SIZE; i++) {
        state = iput_msg_que[i].state;
        
        if (state != QUE_ITEM_EMPTY &&
            offset == iput_msg_que[i].offset &&
            SEQNUM_CMP(seqnum, iput_msg_que[i].seqnum) &&
            MAC_ADDR_CMP(addr, &iput_msg_que[i].addr)) {
            
            return &iput_msg_que[i];
        }
    }
    
    return NULL;
}

/**
 * @brief: input massage add to iput_msg queue
 * @param {in} p: pointer of packet buf
  * @return: result
 */
int8_t iput_msg_add_to_queue(struct pbuf* p)
{
    uint16_t offset;
    struct mac_hdr* p_hdr;
    struct iput_msg* p_msg;
    
    if (p == NULL) {
        return ERR_PARA;
    }

    p_hdr = (struct mac_hdr*)p->payload;
    offset = HDR_OFFSET_BYTES(p_hdr);
    // check input massage queue,if it already exists, 
    // need not to insert to queue.
    p_msg = iput_msg_find_match(&p_hdr->src, p_hdr->seqnum, offset);
    if (p_msg != NULL) {
        // TY_LOGERROR("input %d seqnum already exist",p_hdr->seqnum);
        return ERR_VAL;
    }

    // find empty queue to add new seqnum info
    p_msg = iput_msg_find_empty();
    if (p_msg == NULL) {
        TY_LOGERROR("input msg queue is full.");
        return ERR_BUF;
    }

    // fill input queue
    p_msg->state = QUE_ITEM_EXISTED;
    p_msg->ctime = 0;
    p_msg->seqnum = p_hdr->seqnum;
    p_msg->offset = HDR_OFFSET_BYTES(p_hdr);
    memcpy(p_msg->addr.addr, p_hdr->src.addr, MAX_HWADDR_LEN);

    TY_LOGDEBUG("input msg queue create ok.");

    return ERR_OK;
}

/**
 * @brief: input massage handler
 * @param {in} p_buf: pointer of packet buf
 * @return: result
 */
int8_t iput_msg_handler(struct pbuf* p_buf)
{
    return iput_msg_add_to_queue(p_buf);
}


#if 0
uint8_t g_input_pbuf_num = 0;
struct pbuf* input_pbuf_list; 

void iput_pbuf_upload_tmr(void)
{
    struct mac_hdr* p_hdr;
    struct rf_card* p_card;
    struct pbuf* p_buf = input_pbuf_list;

    if (p_buf == NULL) {
        return;
    }

    input_pbuf_list = p_buf->next;
    
    p_hdr = (struct mac_hdr*)p_buf->payload;

    p_card = mac_route(&p_hdr->dest);
    if(p_card == NULL) {
        TY_LOGERROR("rf card error.");
        return;
    }

    TY_LOGDEBUG("SEQ %d",p_hdr->seqnum);
    if (p_card->input(p_buf, p_card) != ERR_OK) {
        TY_LOGERROR("up layer handler error");
    }
}

int8_t input_pbuf_add_to_list(struct pbuf* p_buf)
{
    int8_t ret;
    uint8_t pbuf_num;
    
    if (p_buf == NULL) {
        return ERR_PARA;
    }

    ret = iput_msg_add_to_queue(p_buf);
    if (ret != ERR_OK) {
        pbuf_free(p_buf);
        return ERR_OTHER;
    }

    pbuf_num = pbuf_counter(p_buf);
    if (pbuf_num > IPUT_MSG_PBUF_MAX_SIZE || 
        g_input_pbuf_num + pbuf_num > IPUT_MSG_PBUF_MAX_SIZE) {
        pbuf_free(p_buf);
        TY_LOGERROR("too more input pbuf %d",g_input_pbuf_num + pbuf_num);
       return ERR_PARA;
    }

    g_input_pbuf_num += pbuf_num;

    // tail insertion
    if (input_pbuf_list == NULL) {
        input_pbuf_list = p_buf;
        return ERR_OK;
    }
    for (p_buf = input_pbuf_list; p_buf->next != NULL; p_buf = p_buf->next) {
    }
    p_buf->next = p_buf;

    return ERR_OK;
}

#endif


