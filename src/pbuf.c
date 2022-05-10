/*
 * @Author: Lux1206
 * @Email: 862215606@qq.com
 * @LastEditors: Please set LastEditors
 * @FileName: &file name&
 * @Description: 
 * @Date: 2020-11-09 09:57:18
 * @LastEditTime: 2020-12-16 09:32:33
 */
#include <string.h>
#include "rf_stack_cfg.h"
#include "pbuf.h"

static mem_t mem_ctr = {0};

/**
 * @brief: rf stack memery init
 * @param {in} mem: memery structure
 * @return: none
 */
void mem_init(mem_t *mem)
{
    mem_ctr.free_fn = mem->free_fn;
    mem_ctr.malloc_fn = mem->malloc_fn;
}
/**
 * @brief: rf stack free
 * @param {in} p: pointer to address for free
 * @return: none
 */
void ty_free(void *p)
{
    mem_ctr.free_fn(p);
}
/**
 * @brief: rf stack malloc
 * @param {in} size: malloc size
 * @return: none
 */
void* ty_malloc(size_t size)
{
    return mem_ctr.malloc_fn(size);
}

/**
 * @brief: init the pbuf after alloced
 * @param {in} p: pointer of the pbuf
 * @param {in} payload: payload of the pbuf
 * @param {in} tot_len: tot_len of the pbuf chain
 * @param {in} len: len of the pbuf
 * @return: none
 */
static void pbuf_alloced_init(struct pbuf *p, void *payload, 
                              uint16_t tot_len, uint16_t len)
{
    p->next = NULL;
    p->payload = payload;
    p->tot_len = tot_len;
    p->len = len;
    p->ref = 1;
    p->flag = 0;
    p->data_id = 0;
    p->card_idx = 0;
}

/**
 * @brief: pbuf alloce
 * @param {in} offset: the offset of the payload 
 * @param {in} len: len of the pbuf payload
 * @return: pointer of pbuf
 */
struct pbuf* pbuf_alloc(uint8_t offset, uint16_t len)
{
    struct pbuf* p;
    uint16_t payload_len, tot_len;

    payload_len = (uint16_t)MEM_ALIGN_SIZE(offset) + MEM_ALIGN_SIZE(len);
    tot_len = MEM_ALIGN_SIZE(sizeof(struct pbuf)) + payload_len;

    p = (struct pbuf *)ty_malloc(tot_len);
    if (p == NULL) {
        TY_LOGERROR("alloc error.");
        return NULL;
    }
    memset(p, 0, tot_len);

    pbuf_alloced_init(p, MEM_ALIGN((void *)((uint8_t *)p + MEM_ALIGN_SIZE(sizeof(struct pbuf)) + offset)), 
                        len, len);

    return p;
}
/**
 * @brief: free pbuf
 * @param {in} p: pointer of the pbuf
 * @return: count of free pbuf 
 */
uint8_t pbuf_free(struct pbuf* p)
{
    uint8_t ref;
    struct pbuf* q;
    uint8_t count = 0;

    if (p == NULL) {
        TY_LOGERROR("pbuf free params error.");
        return 0;
    }
    
    while(p != NULL) {
        ref = --(p->ref);
        if (ref == 0) {
            q = p->next;
            ty_free(p);
            p = q;
            count++;
        }
        else {
            p = NULL;
        }
    }

    return count;
}

/**
 * @brief: pbuf ref increase
 * @param {in} p: pointer of the pbuf
 * @return: none
 */
void pbuf_ref_increase(struct pbuf* p)
{
    if (p != NULL) {
        p->ref += 1;
    }
}

/**
 * @brief: get the number of pbuf
 * @param {in} p: pointer of the pbuf
 * @return: pbuf number
 */
uint16_t pbuf_counter(const struct pbuf* p)
{
    uint16_t len = 0;

    while (p != NULL) {
        ++len;
        p = p->next;
    }

    return len;
}

/**
 * @brief: connect two pbuf but not increase ref
 * @param {in} front: pointer of the front pbuf
 * @param {in} after: pointer of the after pbuf
 * @return: none
 */
void pbuf_cat(struct pbuf* front, struct pbuf* after)
{
    struct pbuf* p;

    if (front == NULL || after == NULL) {
        TY_LOGERROR("pbuf_connect params error.");
        return;
    }

    for (p = front; p->next != NULL; p = p->next) {
        p->tot_len = p->tot_len + after->tot_len;
    }
    /* add total length of second chain to last pbuf total of first chain */
    p->tot_len = p->tot_len + after->tot_len;
    /* chain last pbuf of head (p) with first of tail (t) */
    p->next = after;
}

/**
 * @brief: connect two pbuf
 * @param {in} front: pointer of the front pbuf
 * @param {in} after: pointer of the after pbuf
 * @return: none
 */
void pbuf_chain(struct pbuf* front, struct pbuf* after)
{
    pbuf_cat(front, after);
    pbuf_ref_increase(after);
}

/**
 * @brief: dechainpbuf
 * @param {in} p: pointer of the pbuf
 * @return: none
 */
struct pbuf* pbuf_dechain(struct pbuf *p)
{
    struct pbuf *q;
    uint8_t tail_gone = 1;

    q = p->next;

    if (q != NULL) {
        if (q->tot_len != p->tot_len - p->len) {
            return NULL;
        }
        /* enforce invariant if assertion is disabled */
        q->tot_len = (p->tot_len - p->len);
        /* decouple pbuf from remainder */
        p->next = NULL;
        /* total length of pbuf p is its own length only */
        p->tot_len = p->len;

        tail_gone = pbuf_free(q);
        if (tail_gone > 0) {
            
        }
    }

    return ((tail_gone > 0) ? NULL : q);
}

/**
 * @brief: move the pointer to the head
 * @param {in} p: the pointer of pbuf
 * @param {in} hdr_size_inc: last header len
 * @return: result
 */
int8_t pbuf_add_header(struct pbuf* p, uint16_t hdr_size_inc)
{
    uint8_t* payload;
    
    if (p == NULL || hdr_size_inc > 0xfffe) {
        TY_LOGERROR("pbuf_add_header params error.");
        return ERR_PARA;
    }
    if (hdr_size_inc == 0) {
        return ERR_OK;
    }
    
    if ((uint16_t)(hdr_size_inc + p->tot_len) < hdr_size_inc) {
        TY_LOGERROR("pbuf_add_header overflow.");
        return ERR_VAL;
    }

    payload = (uint8_t *)p->payload - hdr_size_inc;

    if (payload < (uint8_t *)p + MEM_ALIGN_SIZE(sizeof(struct pbuf))) {
        TY_LOGERROR("pbuf_add_header take up pbuf header.");
        return ERR_VAL;
    }

    p->payload = (void *)payload;
    p->len += hdr_size_inc;
    p->tot_len += hdr_size_inc;

    return ERR_OK;
}

/**
 * @brief: move the pointer to the payload
 * @param {in} p: the pointer of pbuf
 * @param {in} hdr_size_inc: last header len
 * @return: result
 */
int8_t pbuf_remove_header(struct pbuf* p, uint16_t hdr_size_dec)
{
    uint8_t* payload;

    if (p == NULL || hdr_size_dec > 0xfffe) {
        TY_LOGERROR("pbuf_remove_header params error.");
        return ERR_PARA;
    }
    if (hdr_size_dec == 0) {
        return ERR_OK;
    }

    payload = (uint8_t *)p->payload + hdr_size_dec;

    if (payload > (uint8_t *)p->payload + p->len) {
        TY_LOGERROR("pbuf_remove_header overflow.");
        return ERR_VAL;
    }

    p->payload = (void *)payload;
    p->len -= hdr_size_dec;
    p->tot_len -= hdr_size_dec;

    return ERR_OK;
}

/**
 * @brief: copy application supplied data into a pbuf
 * @param {in} buf: pbuf to fill with data
 * @param {in} p_data: application supplied data buffer
 * @param {in} len: len of application supplied data
 * @return: result
 */
int8_t pbuf_take(struct pbuf *buf, const void *p_data, uint16_t len)
{
    struct pbuf *p;
    uint16_t buf_copy_len;
    uint16_t total_copy_len = len;
    uint16_t copied_total = 0;

    if (buf == NULL || p_data == NULL || buf->tot_len < len) {
        TY_LOGDEBUG("args error.");
        return ERR_PARA;
    }

    /* note some systems use byte copy if p_data or one of the pbuf payload pointers are unaligned. */
    for (p = buf; total_copy_len != 0; p = p->next) {
        buf_copy_len = total_copy_len;

        if (buf_copy_len > p->len) {
            /* this pbuf can not hold all remaining data */
            buf_copy_len = p->len;
        }
        /* copy the necessary parts of the buffer */
        memcpy(p->payload, &((const char *)p_data)[copied_total], buf_copy_len);
        total_copy_len -= buf_copy_len;
        copied_total += buf_copy_len;
    }
    
    return ERR_OK;
}

/**
 * @brief: copy pbuf data to swap buff
 * @param {in} buf: pbuf to fill with data
 * @param {in} swap: data swap buff
 * @param {in} len: len of application supplied data
 * @param {in} offset: pbuf payload offset
 * @return: result
 */
uint16_t pbuf_copy_to_swap(const struct pbuf *buf, void *swap, uint16_t len, uint16_t offset)
{
    const struct pbuf *p_buf;
    uint16_t left = 0;
    uint16_t buf_copy_len;
    uint16_t copied_total = 0;

    if (buf == NULL || swap == NULL || !len) {
        return 0;
    }

    // note some systems use byte copy if swap or one of the pbuf payload pointers are unaligned.
    for (p_buf = buf; len != 0 && p_buf != NULL; p_buf = p_buf->next) {
        if ((offset != 0) && (offset >= p_buf->len)) {
            return 0;
        } 
        else {
            // copy from this buffer. maybe only partially.
            buf_copy_len = p_buf->len - offset;
            if (buf_copy_len > len) {
                buf_copy_len = len;
            }
            // copy the necessary parts of the buffer
            memcpy(&((char *)swap)[left], &((char *)p_buf->payload)[offset], buf_copy_len);
            copied_total += buf_copy_len;
            left += buf_copy_len;
            len -= buf_copy_len;
            offset = 0;
        }
    }

    return copied_total;
}



