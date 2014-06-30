/**************************************************************************//**
 Copyright 2013 Freescale Semiconductor, Inc.

 @File          io.h

 @Description   I/O accessors
 *//***************************************************************************/
#ifndef __FSL_IO_H
#define __FSL_IO_H

#include "common/types.h"
#include "fsl_endian.h"

static inline uint8_t ioread8(const volatile uint8_t *addr)
{
	uint8_t ret = *addr;
	core_memory_barrier();
	return ret;
}

static inline uint16_t ioread16(const volatile uint16_t *addr)
{
	uint16_t ret = LOAD_LE16_TO_CPU(addr);
	core_memory_barrier();
	return ret;
}

static inline uint16_t ioread16be(const volatile uint16_t *addr)
{
	uint16_t ret = LOAD_BE16_TO_CPU(addr);
	core_memory_barrier();
	return ret;
}

static inline uint32_t ioread32(const volatile uint32_t *addr)
{

	uint32_t ret = LOAD_LE32_TO_CPU(addr);
	core_memory_barrier();
	return ret;
}

static inline uint32_t ioread32be(const volatile uint32_t *addr)
{
	uint32_t ret = LOAD_BE32_TO_CPU(addr);
	core_memory_barrier();
	return ret;
}

static inline uint64_t ioread64(const volatile uint64_t *addr)
{
	uint64_t ret = LOAD_LE64_TO_CPU(addr);
	core_memory_barrier();
	return ret;
}

static inline uint64_t ioread64be(const volatile uint64_t *addr)
{
	uint64_t ret = LOAD_BE64_TO_CPU(addr);
	core_memory_barrier();
	return ret;
}

static inline void iowrite8(uint8_t val, volatile uint8_t *addr)
{
	*addr = val;
	core_memory_barrier();
}

static inline void iowrite16(uint16_t val, volatile uint16_t *addr)
{
	STORE_CPU_TO_LE16(val, addr);
	core_memory_barrier();
}

static inline void iowrite16be(uint16_t val, volatile uint16_t *addr)
{
	STORE_CPU_TO_BE16(val, addr);
	core_memory_barrier();
}

static inline void iowrite32(uint32_t val, volatile uint32_t *addr)
{
	STORE_CPU_TO_LE32(val, addr);
	core_memory_barrier();
}

static inline void iowrite32be(uint32_t val, volatile uint32_t *addr)
{
	STORE_CPU_TO_BE32(val, addr);
	core_memory_barrier();
}
/*
 * TODO: remove "__attribute__((never_inline))" when CQ ENGR00319764 solved
 */
static inline void iowrite64(uint64_t val, volatile uint64_t *addr) __attribute__((never_inline))
{
	STORE_CPU_TO_LE64(val, addr);
	core_memory_barrier();
}

static inline void iowrite64be(uint64_t val, volatile uint64_t *addr)
{
	STORE_CPU_TO_BE64(val, addr);
	core_memory_barrier();
}

/**************************************************************************//**
 @Function      fsl_os_phys_to_virt

 @Description   Translates a physical address to the matching virtual address.

 @Param[in]     addr - The physical address to translate.

 @Return        Virtual address.
 *//***************************************************************************/
void *fsl_os_phys_to_virt(dma_addr_t addr);

/**************************************************************************//**
 @Function      fsl_os_virt_to_phys

 @Description   Translates a virtual address to the matching physical address.

 @Param[in]     addr - The virtual address to translate.

 @Return        Physical address.
 *//***************************************************************************/
dma_addr_t fsl_os_virt_to_phys(void *addr);

#endif /* __FSL_IO_H */