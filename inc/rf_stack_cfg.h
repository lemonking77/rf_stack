/*
 * @Author: Lux1206
 * @Email: 862215606@qq.com
 * @LastEditors: Please set LastEditors
 * @FileName: &file name&
 * @Description: 
 * @Date: 2020-11-07 17:16:29
 * @LastEditTime: 2021-03-20 16:13:13
 */
#ifndef _RF_STACK_CFG_H__
#define _RF_STACK_CFG_H__

#include <string.h>
#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef   __PACKED
  #define __PACKED                               __attribute__ ((__packed__))
#endif
#ifndef   __PACKED_STRUCT
  #define __PACKED_STRUCT                        struct __attribute__((packed, aligned(1)))
#endif
#ifndef   __PACKED_UNION
  #define __PACKED_UNION                         union __attribute__((packed, aligned(1)))
#endif


/* Definitions for error constants. */
typedef enum {
    /* No error, everything OK. */
    ERR_OK       = 0,
    /* Out of memory error.     */
    ERR_MEM      = -1,
    /* Buffer error.            */
    ERR_BUF      = -2,
    /* Timeout.                 */
    ERR_TIMEOUT  = -3,
    /* Operation in progress    */
    ERR_INPROGRESS = -4,
    /* Illegal value.           */
    ERR_VAL      = -5,
    /* Operation would block.   */
    ERR_WOULDBLOCK = -6,
    /* Address in use.          */
    ERR_USE      = -7,
    /* Already connecting.      */
    ERR_ALREADY  = -8,
    /* Not connected.           */
    ERR_CONN    = -9,
    /* Low-level rf card error. */
    ERR_RF      = -10,
    /* Illegal parameter.       */
    ERR_PARA    = -11,
    /* Others error.            */
    ERR_OTHER   = -12
} ERR_T;


#define MEM_ALIGNMENT           4
typedef uint32_t                mem_ptr_t;


#ifndef MEM_ALIGN_SIZE
#define MEM_ALIGN_SIZE(size)    (((size) + MEM_ALIGNMENT - 1U) & ~(MEM_ALIGNMENT-1U))
#endif

#ifndef MEM_ALIGN
#define MEM_ALIGN(addr)         ((void *)(((mem_ptr_t)(addr) + MEM_ALIGNMENT - 1) & ~(mem_ptr_t)(MEM_ALIGNMENT-1)))
#endif

// Hardware address length
#define MAX_HWADDR_LEN          8
// Maximum load length
#define MAX_PAYLOAD_LEN         100

// users need to adapt according to the hardware
#ifndef ENTER_CRITICAL
#define ENTER_CRITICAL()        0
#endif 
// users need to adapt according to the hardware
#ifndef EXIT_CRITICAL 
#define EXIT_CRITICAL(pri) 
#endif

#define TY_LOGERROR(fmt, args...) \
    printf("<%s>(%d)-: "fmt"\r\n",__func__ , __LINE__, ##args)

#define TY_LOGDEBUG(fmt, args...) \
    printf("<%s>(%d)-: "fmt"\r\n",__func__ , __LINE__, ##args)


#ifdef __cplusplus
}
#endif

#endif // end _RF_MF_STACK_CFG_H__
