 /**************************************************************************//**
 Copyright 2013 Freescale Semiconductor, Inc.

 @File          types_mw.h

 @Description   TODO
*//***************************************************************************/
#ifndef __TYPES_MW_H
#define __TYPES_MW_H


#include <stdint.h>
#include <stddef.h>

//#define __inline__      inline
#define _prepacked
#define _packed

/* temporary, due to include issues */
typedef uint32_t uintptr_t;
typedef int32_t intptr_t;

typedef uint64_t            dma_addr_t;


#ifndef NULL
#define NULL ((0L))
#endif /* NULL */


/** Task global variables area */
#define __TASK __declspec(section ".tdata")

/** Shared-SRAM global variables */
#define __SHRAM __declspec(section ".sdata")

/** Task global variable definition */
#define __HOT_CODE __declspec(section ".text")


#endif /* __TYPES_MW_H */
