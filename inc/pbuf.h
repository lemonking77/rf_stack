/*
 * @Author: Lux1206
 * @Email: 862215606@qq.com
 * @LastEditors: Please set LastEditors
 * @FileName: &file name&
 * @Description: 
 * @Date: 2020-11-07 17:16:29
 * @LastEditTime: 2021-03-19 10:34:26
 */

#ifndef _PBUF_H__
#define _PBUF_H__


#include <stdio.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
    NO_SEND = 0,
    SEND_SUCESS,
}SEND_T;

typedef struct {
	void* (*malloc_fn)(size_t num);
	void (*free_fn)(void *ptr);
}mem_t;

typedef enum {
  PBUF_MAC = 0 + 24,
  PBUF_RAW = 0
} pbuf_layer;

struct pbuf {
    /* next pbuf in singly linked pbuf chain */
    struct pbuf *next;
    /* pointer to the actual data in the buffer */
    void *payload;
    /* the total length of data contained in the current pbuf and all subsequent pbufs */
    uint16_t tot_len;
    /* length of this buffer */
    uint16_t len;
    /* hight 4 bits are used for sending status,low 4 bits are used for priority */
    uint8_t flag;
    /*
     * the reference count always equals the number of pointers
     * that refer to this pbuf. This can be pointers from an application,
     * the stack itself, or pbuf->next pointers from a chain.
     */
    uint8_t ref;
    /* data management id,use for or sending management */
    uint8_t data_id;
    /* For incoming packets, this contains the input netif's index */
    uint8_t card_idx;
    /* send retry times */
    uint16_t send_retry;
    uint16_t resv; 
};

#define PBUF_STRUCT_SIZE (sizeof(struct pbuf))

#define SEND_STATUS_GET(pbuf) \
    ((pbuf->flag & 0xF0) >> 4)
    
#define SEND_STATUS_SET(pbuf, status) \
    (pbuf->flag |= (status << 4))

#define PBUF_PRIORITY_GET(pbuf) \
    (pbuf->flag & 0x0F)
    
#define PBUF_PRIORITY_SET(pbuf, status) \
    (pbuf->flag |= (status & 0x0F))

void mem_init(mem_t* mem);
void ty_free(void *p);
void* ty_malloc(size_t size);

uint8_t pbuf_free(struct pbuf* p);
struct pbuf* pbuf_alloc(uint8_t offset, uint16_t len);
void pbuf_ref_increase(struct pbuf* p);
uint16_t pbuf_counter(const struct pbuf* p);
void pbuf_cat(struct pbuf* front, struct pbuf* after);
void pbuf_chain(struct pbuf* front, struct pbuf* after);
struct pbuf* pbuf_dechain(struct pbuf *p);
int8_t pbuf_add_header(struct pbuf* p, uint16_t hdr_size_inc);
int8_t pbuf_remove_header(struct pbuf* p, uint16_t hdr_size_dec);
int8_t pbuf_take(struct pbuf *buf, const void *p_data, uint16_t len);
uint16_t pbuf_copy_to_swap(const struct pbuf *buf, void *swap, uint16_t len, uint16_t offset);

#ifdef __cplusplus
}
#endif

#endif  // end _MAC_NET_H__
