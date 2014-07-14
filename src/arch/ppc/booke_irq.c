/*
 *
 * @File          booke_irq.c
 * 
 * @Description   This is an E200 z490 specific implementation. 
 *                This file contain the PPC general exception 
 *                interrupt routines.
 *
 * @Cautions
 *
 *
 * Author: Danny Shultz.
 *
 */

#include "fsl_core_booke_regs.h"
#include "types.h"
#include "fsl_dbg.h"

#pragma push
#pragma section code_type ".interrupt_vector"
#pragma force_active on
#pragma function_align 256 /* IVPR must be aligned to 256 bytes */

void booke_init_interrupt_vector(void);

/*****************************************************************/
/* routine:       booke_generic_irq_init                         */
/*                                                               */
/* description:                                                  */
/*        Initialization of pointers to the exception handlers.  */
/*                                                               */
/* arguments:                                                    */
/*    None.                                                      */
/*                                                               */
/*****************************************************************/
void booke_generic_irq_init(void)
{
    booke_init_interrupt_vector();
}

/*****************************************************************/
/* routine:       booke_generic_exception_isr                    */
/*                                                               */
/* description:                                                  */
/*    Internal routine, called by the main interrupt handler     */
/*    to indicate the occurrence of an INT.                      */
/*                                                               */
/* arguments:                                                    */
/*    intrEntry (In) - The interrupt handler original offset     */
/*                     from IVPR register.                       */
/*                                                               */
/*****************************************************************/
void booke_generic_exception_isr(uint32_t intr_entry)
{
	switch(intr_entry)
	{
	case(0x00):
		pr_debug("core %d int: CRITICAL\n", core_get_id());
		break;
	case(0x10):
		pr_debug("core %d int: MACHINE_CHECK\n", core_get_id());
		break;
	case(0x20):
		pr_debug("core %d int: DATA_STORAGE\n", core_get_id());
		break;
	case(0x30):
		pr_debug("core %d int: INSTRUCTION_STORAGE\n", core_get_id());
		break;
	case(0x40):
		pr_debug("core %d int: EXTERNAL\n", core_get_id());
		break;
	case(0x50):
		pr_debug("core %d int: ALIGNMENT\n", core_get_id());
		break;
	case(0x60):
		pr_debug("core %d int: PROGRAM\n", core_get_id());
		break;
	case(0x70):
		pr_debug("core %d int: SYSTEM CALL\n", core_get_id());
		break;
	case(0x80):
		pr_debug("core %d int: debug\n", core_get_id());
		break;
	case(0x90):
		pr_debug("core %d int: SPE-floating point data\n", core_get_id());
		break;
	case(0xA0):
		pr_debug("core %d int: SPE-floating point round\n", core_get_id());
		break;
	case(0xB0):
		pr_debug("core %d int: performance monitor\n", core_get_id());
		break;
	case(0xF0): 
		pr_debug("core %d int: CTS interrupt\n", core_get_id());
		break;
	default:
		pr_warn("undefined interrupt #%x\n", intr_entry);
		break;
	}
	
	while(1){}
}

asm static void branch_table(void) {
    nofralloc
    
    /* Critical Input Interrupt (Offset 0x00) */
    li  r3, 0x00
    b  exception_irq
    
    /* Machine Check Interrupt (Offset 0x10) */
    .align 0x10
    li  r3, 0x10
    b  exception_irq
    
    /* Data Storage Interrupt (Offset 0x20) */
    .align 0x10
    li  r3, 0x20
    b  exception_irq

    /* Instruction Storage Interrupt (Offset 0x30) */
    .align 0x10
    li  r3, 0x30
    b  exception_irq

    /* External Input Interrupt (Offset 0x40) */
    .align 0x10
    li  r3, 0x40
    b  exception_irq

    /* Alignment Interrupt (Offset 0x50) */
    .align 0x10
    li  r3, 0x50
    b  exception_irq
    
    /* Program Interrupt (Offset 0x60) */
    .align 0x10
    li  r3, 0x60
    b  exception_irq
    
    /* Performance Monitor Interrupt (Offset 0x70) */
    .align 0x10
    li  r3, 0x70
    b  exception_irq 
    
    /* System Call Interrupt (Offset 0x80) */
    .align 0x10
    li  r3, 0x80
    b  exception_irq 
    
    /* Debug Interrupt (Offset 0x90) */
    .align 0x10
    li  r3, 0x90
    b  exception_irq
    
    /* Embedded Floating-point Data Interrupt (Offset 0xA0) */
    .align 0x10
    li  r3, 0xA0
    b exception_irq
    
    /* Embedded Floating-point Round Interrupt (Offset 0xB0) */
    .align 0x10
    li  r3, 0xB0
    b exception_irq

    /* place holder (Offset 0xC0) */
    .align 0x10
    nop 

    /* place holder (Offset 0xD0) */
    .align 0x10
    nop 

    /* place holder (Offset 0xE0) */
    .align 0x10
    nop 

    /* CTS Task Watchdog Timer Interrupt (Offset 0xF0) */
    .align 0x10
    li  r3, 0xF0
    b exception_irq
    
    /***************************************************/
    /*** generic exception *****************************/
    /***************************************************/
    .align 0x100
exception_irq:
    li       r0, 0x00000000
    /* disable exceptions and interrupts */
    mtspr    DBSR, r0 //TODO maybe save it before clearing it ... ??? not reacting ???
    mtspr    DBCR0, r0
    mtspr    DBCR2, r0
    /* disable debug and interrupts in MSR */
    mtmsr    r0 //TODO not sure about this //TODO disable interrupts //TODO look at: CE, EE, ME, DE, PMM and RI
    isync
    /* update stack overflow detection to 1-task (0x8000) */ 
    se_bgeni r4,16
    mtspr    DAC2,r4
    /* clear stack pointer */
    li       rsp, 0x7ff0 //TODO this is not correct, what if task is not #0 !!!
    /* branch to isr */
    lis      r4,booke_generic_exception_isr@h
    ori      r4,r4,booke_generic_exception_isr@l
    mtlr     r4
    blrl
    se_illegal
}

asm void booke_init_interrupt_vector(void)
{
    lis     r3,branch_table@h
    ori     r3,r3,branch_table@l
    mtspr   IVPR,r3
}

#pragma pop
