/*
 * Copyright 2014 Freescale Semiconductor, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Freescale Semiconductor nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Freescale Semiconductor BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*******************************************************************************
 *                              - CAUTION -
 *
 * This code must not be distributed till we haven't a clear understanding
 * about what internals of the Parser may be exposed to customers.
 ******************************************************************************/

#include "fsl_types.h"
#include <__mem.h>
#include "fsl_stdio.h"
#include "fsl_io.h"
#include "fsl_sparser_drv.h"
#include "fsl_sparser_disa.h"
#include "fsl_sparser_dump.h"

#include "fsl_dbg.h"
/* If "fsl_dbg.h" is not included ASSERT_COND and pr_err must be redefined as
 * it follows */
#ifndef ASSERT_COND
	#define ASSERT_COND(_cond)
#endif
#ifndef pr_err
	#define pr_err(...)
#endif

/******************************************************************************/
/* Parameters Array size */
#define SP_PA_SIZE		64
/* Number of parser working registers */
#define NUM_WR			2
/* Minimum value of program counter */
#define SP_MIN_PC		0x20
/* DPAA2 G (gosub) bit position */
#define DEST_ADDR_G_BIT		0x8000
/* DPAA2 L (relative addressing) bit position */
#define DEST_ADDR_L_BIT		0x4000
/* DPAA2 sign bit position */
#define DEST_ADDR_SIGN_BIT	0x0400
#if (SP_DPAA_VERSION == 1)
	/* Destination address mask */
	#define DEST_ADDR_MASK		0x3FF
	/* Maximum value of program counter */
	#define SP_MAX_PC		0x3FD
	/* Return to calling HXS address  */
	#define RET_TO_HARD_HXS		0x3FE
	/* End of parsing address  */
	#define END_PARSING		0x3FF
#else
	/* Destination address mask */
	#define DEST_ADDR_MASK		0x7FF
	/* Maximum value of program counter */
	#define SP_MAX_PC		0x7FD
	/* Return to calling HXS address  */
	#define RET_TO_HARD_HXS		0x7FE
	/* End of parsing address  */
	#define END_PARSING		0x7FF
	/* Byte position of FAF Extension into Parse Array */
	#define RA_FAF_EXT_POSITION	18
	/* Byte position of FAF into Parse Array */
	#define RA_FAF_POSITION		20
	/* Number of bytes of FAF into Parse Array */
	#define RA_FAF_BYTES		12
#endif

/* HXS was called */
#define SP_HARD_HXS_CALLED	1
/* Invalid OpCode detected */
#define SP_ERR_INVAL_OPCODE	-1
/* Invalid Destination address detected */
#define SP_ERR_INVAL_DST	-2
/* Invalid Parameter detected */
#define SP_ERR_INVAL_PARAM	-3

/******************************************************************************/
/***************************
/* DPAA1 only instructions *
 ***************************
	1:Confirm_Layer_Mask
	3:OR_IV_LCV
	12:Load_LCV_to_WR
	13:Store_WR_to_LCV
	31:Compare_WR0_to_IV
*/
/***************************
/* DPAA1 only instructions *
 ***************************
	4:Return Sub
	21:Set_Clr_FAF
	25:Jump_FAF
*/
enum opcode_syndrom {
#if (SP_DPAA_VERSION == 1)
	CONFIRM_LAYER_MASK   = 0x0001,		/* 1: */
	OR_IV_LCV            = 0x0003,		/* 3: */
	LOAD_LCV_TO_WR       = 0x0040,		/* 12: */
	STORE_WR_TO_LCV      = 0x0042,		/* 13: */
	COMPARE_WR0_TO_IV    = 0x4000,		/* 31: */
#else
	RETURN_SUB           = 0x0007,		/* 4: */
	SET_CLR_FAF          = 0x0300,		/* 21: */
	JUMP_FAF             = 0x0C00,		/* 25: */
#endif
	JUMP_GOSUB           = 0x1800,		/* 27: */
	NOP                  = 0x0000,		/* 0: */
	ADVANCE_HB_BY_WO     = 0x0002,		/* 1: */
	ZERO_WR              = 0x0004,		/* 2: */
	ONES_CMP_WR1_TO_WR0  = 0x0006,		/* 3: */
	CASE1_DJ_WR_TO_WR    = 0x0008,		/* 5: */
	CASE2_DC_WR_TO_WR    = 0x000C,		/* 6: */
	CASE2_DJ_WR_TO_WR    = 0x0010,		/* 7: */
	CASE3_DC_WR_TO_WR    = 0x0018,		/* 8: */
	CASE3_DJ_WR_TO_WR    = 0x0020,		/* 9: */
	CASE4_DC_WR_TO_WR    = 0x0030,		/* 10: */
	JUMP_PROTOCOL        = 0x0044,		/* 11: */
	ADD_SUB_WR_WR_TO_WR  = 0x0048,		/* 12: */
	ADD_SUB_WR_IV_TO_WR  = 0x0050,		/* 13: */
	BITWISE_WR_WR_TO_WR  = 0x0070,		/* 14: */
	COMPARE_WR0_WR1      = 0x0078,		/* 15: */
	MODIFY_WO_BY_WR      = 0x0080,		/* 16: */
	BITWISE_WR_IV_TO_WR  = 0x00C0,		/* 17: */
	SHIFT_LEFT_WR_BY_SV  = 0x0100,		/* 18: */
	SHIFT_RIGHT_WR_BY_SV = 0x0180,		/* 19: */
	LOAD_BITS_IV_TO_WR   = 0x0200,		/* 20: */
	LOAD_SV_TO_WO        = 0x0600,		/* 22: */
	ADD_SV_TO_WO         = 0x0700,		/* 23: */
	STORE_IV_TO_RA       = 0x0800,		/* 24: */
	LOAD_BYTES_PA_TO_WR  = 0x1000,		/* 26: */
	STORE_WR_TO_RA       = 0x2800,		/* 28: */
	LOAD_BYTES_RA_TO_WR  = 0x3000,		/* 29: */
	LOAD_BITS_FW_TO_WR   = 0x8000,		/* 30: */
};

struct ps_opcodes {
	enum opcode_syndrom	op_syn;
	uint16_t		op_mask;
};

struct ps_opcodes opcodes[] = {
#if (SP_DPAA_VERSION == 1)
	{CONFIRM_LAYER_MASK,   0xffff},
	{OR_IV_LCV,            0xffff},
	{LOAD_LCV_TO_WR,       0xfffe},
	{STORE_WR_TO_LCV,      0xfffe},
	{JUMP_GOSUB,           0xf800},
	{COMPARE_WR0_TO_IV,    0xc000},
#else
	{RETURN_SUB,           0xffff},
	{SET_CLR_FAF,          0xff00},
	{JUMP_FAF,             0xff00},
	{JUMP_GOSUB,           0xfffc},
#endif
	{NOP,                  0xffff},
	{ADVANCE_HB_BY_WO,     0xffff},
	{ZERO_WR,              0xfffe},
	{ONES_CMP_WR1_TO_WR0,  0xffff},
	{CASE1_DJ_WR_TO_WR,    0xfffc},
	{CASE2_DC_WR_TO_WR,    0xfffc},
	{CASE2_DJ_WR_TO_WR,    0xfff8},
	{CASE3_DC_WR_TO_WR,    0xfff8},
	{CASE3_DJ_WR_TO_WR,    0xfff0},
	{CASE4_DC_WR_TO_WR,    0xfff0},
	{JUMP_PROTOCOL,        0xfffc},
	{ADD_SUB_WR_WR_TO_WR,  0xfff8},
	{ADD_SUB_WR_IV_TO_WR,  0xfff0},
	{BITWISE_WR_WR_TO_WR,  0xfff8},
	{COMPARE_WR0_WR1,      0xfff8},
	{MODIFY_WO_BY_WR,      0xfffc},
	{BITWISE_WR_IV_TO_WR,  0xffc0},
	{SHIFT_LEFT_WR_BY_SV,  0xff80},
	{SHIFT_RIGHT_WR_BY_SV, 0xff80},
	{LOAD_BITS_IV_TO_WR,   0xff00},
	{LOAD_SV_TO_WO,        0xff00},
	{ADD_SV_TO_WO,         0xff00},
	{STORE_IV_TO_RA,       0xfc00},
	{LOAD_BYTES_PA_TO_WR,  0xf800},
	{STORE_WR_TO_RA,       0xf800},
	{LOAD_BYTES_RA_TO_WR,  0xf000},
	{LOAD_BITS_FW_TO_WR,   0x8000}
};

/******************************************************************************/
struct soft_parser_sim {
	uint16_t		pc;		/* Program Counter */
	uint16_t		hb;		/* Header Base */
	uint16_t		wo;		/* Window Offset */
	uint64_t		wr[NUM_WR];	/* Working Registers 0, 1 */
	uint8_t			pa[SP_PA_SIZE];	/* SP Parameters Array */
	union {					/* Parse Array */
		uint8_t			ra_arr[128];
		struct sp_parse_array	ra;
	};
	uint8_t			frm[SP_SIM_MAX_FRM_LEN];
						/* Frame (without FCS) */
	uint16_t		frm_len;	/* Frame length */
	uint8_t			sim_enabled;	/* "Light" SIM enable */
	uint16_t		pc_ret;		/* Return PC */
	uint16_t		lim_count;	/* Instructions count limit */
	uint16_t		instr_count;	/* Executed instructions */
	int			sp_status;	/* Status/Error code */
	uint16_t		pc_start;	/* Offset from Parser memory*/
	uint16_t		pc_end;		/* Maximum allowed PC */
	uint32_t		init_status;	/* Simulator initialization */
};

/* Simulator initialization flags */
#define SIM_INITIALIZED		0x00000001
#define SIM_RA_SET		0x00000002
#define SIM_PKT_SET		0x00000004
#define SIM_PA_SET		0x00000008
#define SIM_PCLIM_SET		0x00000010
#define SIM_HB_SET		0x00000020

/******************************************************************************/
static struct soft_parser_sim	sp_sim;

/******************************************************************************/
static uint16_t sp_find_opcode(uint16_t in_opcode)
{
	int		i;

	for (i = 0; i < sizeof(opcodes) / sizeof(struct ps_opcodes); i++) {
		if ((in_opcode & opcodes[i].op_mask) == opcodes[i].op_syn)
			return (uint16_t)opcodes[i].op_syn;
	}
	return (uint16_t)-1;
}

/******************************************************************************/
static void sp_print_opcode_words(uint16_t **sp_code, uint8_t wcount)
{
	uint8_t		i;

	#define MAX_INSTR_LEN	5
	ASSERT_COND(wcount > 0 && wcount < (MAX_INSTR_LEN + 1));

	/* PC */
	fsl_print("\t %03x", sp_sim.pc);
	/* Instructions count limit */
	if (sp_sim.sim_enabled)
		fsl_print("[%d:%d]", sp_sim.instr_count, sp_sim.lim_count);
	fsl_print(": ");
	for (i = 0; i < wcount; i++)
		fsl_print("%04x ",  *(*sp_code + i));
	for ( ; i < MAX_INSTR_LEN; i++)
		/*fsl_print("     ");*/
		fsl_print("____ ");
}

/******************************************************************************/
static void sp_not_implemented(uint16_t **sp_code)
{
	sp_sim.sp_status = SP_ERR_INVAL_OPCODE;
	sp_print_opcode_words(sp_code, 1);
	fsl_print("Unknown OpCode;\n");
}

/******************************************************************************/
static void load_iv(uint8_t *be, uint8_t n, uint16_t **sp_code)
{
	uint8_t		*pb;

	ASSERT_COND(n < 4);
	pb = (uint8_t *)(*sp_code + 1);
	switch (n) {
	default:
	case 0:
		be[6] = pb[0];
		be[7] = pb[1];
		be[0] = 0;
		be[1] = 0;
		be[2] = 0;
		be[3] = 0;
		be[4] = 0;
		be[5] = 0;
		break;
	case 1:
		be[4] = pb[2];
		be[5] = pb[3];
		be[6] = pb[0];
		be[7] = pb[1];
		be[0] = 0;
		be[1] = 0;
		be[2] = 0;
		be[3] = 0;
		break;
	case 2:
		be[2] = pb[4];
		be[3] = pb[5];
		be[4] = pb[2];
		be[5] = pb[3];
		be[6] = pb[0];
		be[7] = pb[1];
		be[0] = 0;
		be[1] = 0;
		break;
	case 3:
		be[0] = pb[6];
		be[1] = pb[7];
		be[2] = pb[4];
		be[3] = pb[5];
		be[4] = pb[2];
		be[5] = pb[3];
		be[6] = pb[0];
		be[7] = pb[1];
		break;
	}
}

/******************************************************************************/
static void sp_print_iv_operands(uint16_t **sp_code, uint8_t n)
{
	uint8_t		i;

	ASSERT_COND(n <= 4);
	for (i = 0; i < n; i++) {
		fsl_print("iv%d:0x%x", 3 - i, *(*sp_code + 1 + i));
		fsl_print((i + 1) == n ? ";\n" : ", ");
	}
}

/******************************************************************************/
static void sp_print_hxs_destination(uint16_t jmp_dest)
{
	if (jmp_dest == 0x00)
		fsl_print(" (%s)", "Ethernet");
	else if (jmp_dest == 0x01)
		fsl_print(" (%s)", "LLC+SNAP");
	else if (jmp_dest == 0x02)
		fsl_print(" (%s)", "VLAN");
	else if (jmp_dest == 0x03)
		fsl_print(" (%s)", "PPPoE+PPP");
	else if (jmp_dest == 0x04)
		fsl_print(" (%s)", "MPLS");
#if (SP_DPAA_VERSION == 1)
	else if (jmp_dest >= 5 && jmp_dest <= 0x0f)
		fsl_print(" (%s)",
			  (jmp_dest == 0x05) ? "IPv4 HXS" :
			  (jmp_dest == 0x06) ? "IPv6 HXS" :
			  (jmp_dest == 0x07) ? "GRE HXS" :
			  (jmp_dest == 0x08) ? "Min Encap HXS" :
			  (jmp_dest == 0x09) ? "Other L3 Shell" :
			  (jmp_dest == 0x0a) ? "TCP" :
			  (jmp_dest == 0x0b) ? "UDP" :
			  (jmp_dest == 0x0c) ? "IPSec Shell" :
			  (jmp_dest == 0x0d) ? "SCTP Shell" :
			  (jmp_dest == 0x0e) ? "DCCP Shell" :
			  "Other L4 Shell");
#else
	else if (jmp_dest >= 5 && jmp_dest <= 0x13)
		fsl_print(" (%s)",
			  (jmp_dest == 0x05) ? "ARP" :
			  (jmp_dest == 0x06) ? "IP" :
			  (jmp_dest == 0x07) ? "IPv4 HXS" :
			  (jmp_dest == 0x08) ? "IPv6 HXS" :
			  (jmp_dest == 0x09) ? "GRE HXS" :
			  (jmp_dest == 0x0a) ? "MinEncap HXS" :
			  (jmp_dest == 0x0b) ? "Other L3 Shell" :
			  (jmp_dest == 0x0c) ? "TCP" :
			  (jmp_dest == 0x0d) ? "UDP" :
			  (jmp_dest == 0x0e) ? "IPSec" :
			  (jmp_dest == 0x0f) ? "SCTP" :
			  (jmp_dest == 0x10) ? "DCCP" :
			  (jmp_dest == 0x11) ? "Other L4 Shell" :
			  (jmp_dest == 0x12) ? "GTP" : "ESP");
	else if (jmp_dest == 0x1e)
		fsl_print(" (%s)", "Other L5+ Shell");
	else if (jmp_dest == 0x1f)
		fsl_print(" (%s)", "Final Shell");
#endif
	else if (jmp_dest == RET_TO_HARD_HXS)
		fsl_print(" (%s)", "Return to hard HXS");
	else if (jmp_dest == END_PARSING)
		fsl_print(" (%s)", "End Parsing");
	else
		fsl_print(" (Unknown HXS : %02x)", jmp_dest);
}

/******************************************************************************/
static void sp_print_dest_operand(uint16_t jmp_dest, uint8_t a, uint8_t g,
				  uint8_t l)
{
	uint8_t		s;

	s = (jmp_dest & DEST_ADDR_SIGN_BIT) ? 1 : 0;
	jmp_dest &= DEST_ADDR_MASK;
	fsl_print("d:");
	if (g)
		fsl_print("G|");
	if (l)
		fsl_print("L|");
	if (a)
		fsl_print("A|");
	if (l && s) {
		fsl_print("S|");
		jmp_dest &= ~DEST_ADDR_SIGN_BIT;
	}
	fsl_print("0x%x", jmp_dest);
	if (!l && (jmp_dest < SP_MIN_PC || jmp_dest == RET_TO_HARD_HXS ||
		   jmp_dest == END_PARSING))
		sp_print_hxs_destination(jmp_dest);
	if (a)
		fsl_print("; hb += wo");
	fsl_print(";\n");
}

/******************************************************************************/
static void sp_print_case_dest(const char *what, uint16_t jmp_dest, uint8_t a,
			       uint8_t g, uint8_t l)
{
	uint8_t		i;

	jmp_dest &= DEST_ADDR_MASK;
	fsl_print("\t           ", sp_sim.pc);
	for (i = 0 ; i < MAX_INSTR_LEN + 1; i++)
		fsl_print("     ");
	fsl_print("%s ", what);
#if 1
	sp_print_dest_operand(jmp_dest, a, g, l);
#else
	fsl_print("%s", g ? "gosub" : "jump");
	if (l)
		fsl_print("_rel");
	if (!l && (jmp_dest < SP_MIN_PC || jmp_dest == RET_TO_HARD_HXS ||
		   jmp_dest == END_PARSING))
		sp_print_hxs_destination(jmp_dest);
	else
		fsl_print(" %04x;", jmp_dest);
	if (a)
		fsl_print("; hb += wo;");
	fsl_print("\n");
#endif
}

/******************************************************************************/
static void sp_print_case_dest_continue(void)
{
	uint8_t		i;

	fsl_print("\t           ", sp_sim.pc);
	for (i = 0 ; i < MAX_INSTR_LEN + 1; i++)
		fsl_print("     ");
	fsl_print("DEFAULT            : CONTINUE (DC);\n");
}

/******************************************************************************/
static int sp_check_destination_address(uint16_t jmp_dest)
{
#if (SP_DPAA_VERSION == 1)
	if (!(jmp_dest & (~DEST_ADDR_MASK)))
		return 0;
#else
	if (!(jmp_dest & (~(DEST_ADDR_G_BIT | DEST_ADDR_L_BIT |
			    DEST_ADDR_SIGN_BIT | DEST_ADDR_MASK))))
		return 0;
#endif
	fsl_print("\t\t ERROR : Invalid destination 0x%04x\n",
		  jmp_dest);
	sp_sim.sp_status = SP_ERR_INVAL_DST;
	return -1;
}

/******************************************************************************/
static void set_jmp_gosub_destination(uint16_t **sp_code, uint16_t imm16,
				      uint8_t a, uint8_t g, uint8_t l,
				      uint8_t pc_inc)
{
	uint16_t	jmp_pc;

	jmp_pc = imm16 & DEST_ADDR_MASK;
	if (l) {
		/* Relative JMP/GOSUB to the current PC (jmp_dest = 0) are not
		 * allowed */
		if (!jmp_pc) {
			fsl_print("\t\t ERROR : Null offset relative jump\n");
			sp_sim.sp_status = SP_ERR_INVAL_DST;
			return;
		}
		/* Relative addressing */
		if (jmp_pc & DEST_ADDR_SIGN_BIT) {
			/* Negative jump */
			jmp_pc &= ~DEST_ADDR_SIGN_BIT;
			if (jmp_pc > sp_sim.pc) {
				fsl_print("\t\t ERROR : Jump before PC = 0\n");
				sp_sim.sp_status = SP_ERR_INVAL_DST;
				return;
			}
			(*sp_code) -= jmp_pc;
			jmp_pc = sp_sim.pc - jmp_pc;
		} else {
			/* Positive jump */
			if (sp_sim.pc + jmp_pc == RET_TO_HARD_HXS ||
			    sp_sim.pc + jmp_pc == END_PARSING) {
				jmp_pc += sp_sim.pc;
			} else if (sp_sim.pc + jmp_pc <= sp_sim.pc_end) {
				(*sp_code) += jmp_pc;
				jmp_pc += sp_sim.pc;
			} else {
				fsl_print("\t\t ERROR : Jump after 0x%x\n",
					  sp_sim.pc_end);
				sp_sim.sp_status = SP_ERR_INVAL_DST;
				return;
			}
		}
	} else if (jmp_pc != RET_TO_HARD_HXS && jmp_pc != END_PARSING) {
		/* Absolute addressing */
		if (jmp_pc > sp_sim.pc_end) {
			fsl_print("\t\t ERROR : Jump after 0x%x\n",
				  sp_sim.pc_end);
			sp_sim.sp_status = SP_ERR_INVAL_DST;
			return;
		}
		/*if (jmp_pc < sp_sim.pc_start) {
			fsl_print("\t\t ERROR : Jump before %03x\n",
				  sp_sim.pc_start);
			sp_sim.sp_status = SP_ERR_INVAL_DST;
			return;
		}*/
		if (jmp_pc > sp_sim.pc) {
			(*sp_code) += jmp_pc - sp_sim.pc;
		} else if (jmp_pc < sp_sim.pc) {
			(*sp_code) -= sp_sim.pc - jmp_pc;
		} else if (!a) {
			fsl_print("\t\t ERROR : Jump on same PC 0x%x\n",
				  sp_sim.pc);
			sp_sim.sp_status = SP_ERR_INVAL_DST;
			return;
		}
	}
	sp_sim.pc = jmp_pc;
	fsl_print("\t\t Go to PC = 0x%03x", sp_sim.pc);
	if (jmp_pc == 0x00)
		fsl_print(" (%s)", "Ethernet");
	else if (jmp_pc == 0x01)
		fsl_print(" (%s)", "LLC+SNAP");
	else if (jmp_pc == 0x02)
		fsl_print(" (%s)", "VLAN");
	else if (jmp_pc == 0x03)
		fsl_print(" (%s)", "PPPoE+PPP");
	else if (jmp_pc == 0x04)
		fsl_print(" (%s)", "MPLS");
#if (SP_DPAA_VERSION == 1)
	else if (jmp_pc >= 5 && jmp_pc <= 0x0f)
		fsl_print(" (%s)",
			  (jmp_pc == 0x05) ? "IPv4 HXS" :
			  (jmp_pc == 0x06) ? "IPv6 HXS" :
			  (jmp_pc == 0x07) ? "GRE HXS" :
			  (jmp_pc == 0x08) ? "Min Encap HXS" :
			  (jmp_pc == 0x09) ? "Other L3 Shell" :
			  (jmp_pc == 0x0a) ? "TCP" :
			  (jmp_pc == 0x0b) ? "UDP" :
			  (jmp_pc == 0x0c) ? "IPSec Shell" :
			  (jmp_pc == 0x0d) ? "SCTP Shell" :
			  (jmp_pc == 0x0e) ? "DCCP Shell" :
			  "Other L4 Shell");
#else
	else if (jmp_pc >= 5 && jmp_pc <= 0x13)
		fsl_print(" (%s)",
			  (jmp_pc == 0x05) ? "ARP" :
			  (jmp_pc == 0x06) ? "IP" :
			  (jmp_pc == 0x07) ? "IPv4 HXS" :
			  (jmp_pc == 0x08) ? "IPv6 HXS" :
			  (jmp_pc == 0x09) ? "GRE HXS" :
			  (jmp_pc == 0x0a) ? "MinEncap HXS" :
			  (jmp_pc == 0x0b) ? "Other L3 Shell" :
			  (jmp_pc == 0x0c) ? "TCP" :
			  (jmp_pc == 0x0d) ? "UDP" :
			  (jmp_pc == 0x0e) ? "IPSec" :
			  (jmp_pc == 0x0f) ? "SCTP" :
			  (jmp_pc == 0x10) ? "DCCP" :
			  (jmp_pc == 0x11) ? "Other L4 Shell" :
			  (jmp_pc == 0x12) ? "GTP" : "ESP");
	else if (jmp_pc == 0x1e)
		fsl_print(" (%s)", "Other L5+ Shell");
	else if (jmp_pc == 0x1f)
		fsl_print(" (%s)", "Final Shell");
#endif
	else if (jmp_pc == RET_TO_HARD_HXS)
		fsl_print(" (%s)", "Return to hard HXS");
	else if (jmp_pc == END_PARSING)
		fsl_print(" (%s)", "End Parsing");
	fsl_print("\n");
	if (a) {
		int	i;

		/* HB advancement */
		sp_sim.hb += sp_sim.wo;
		sp_sim.wo = 0;
		for (i = 0; i < NUM_WR; i++)
			sp_sim.wr[i] = 0;
		fsl_print("\t\t HB = %d; WO = %d\n",
			  sp_sim.hb, sp_sim.wo);
		for (i = 0; i < NUM_WR; i++)
			fsl_print("\t\t WR%d = 0x%08x-%08x\n", i,
				  (uint32_t)(sp_sim.wr[i] >> 32),
				  (uint32_t)sp_sim.wr[i]);
	}
	if (jmp_pc < SP_MIN_PC) {
		/* Jump to HXS */
#if (SP_DPAA_VERSION == 1)
		if (jmp_pc > 0x0f) {
#else
		if (jmp_pc > 0x13 && jmp_pc != 0x1e && jmp_pc != 0x1f) {
#endif
			fsl_print("\t\t ERROR : Invalid HXS 0x%x\n", jmp_pc);
			sp_sim.sp_status = SP_ERR_INVAL_DST;
			return;
		}
		sp_sim.sp_status = SP_HARD_HXS_CALLED;
		/* Clear return PC */
		sp_sim.pc_ret = 0;
	} else if (jmp_pc == RET_TO_HARD_HXS || jmp_pc == END_PARSING) {
		sp_sim.sp_status = SP_HARD_HXS_CALLED;
	} else if (g) {
		if (sp_sim.pc_ret) {
			fsl_print("\t\t ERROR : Return PC = 0x%x exists !",
				  sp_sim.pc_ret);
			sp_sim.sp_status = SP_ERR_INVAL_DST;
			return;
		}
		/* Store return PC */
		sp_sim.pc_ret = (uint16_t)(sp_sim.pc + pc_inc);
		fsl_print("\t\t Stored Return PC = 0x%x\n", sp_sim.pc_ret);
	}
}

/******************************************************************************/
#if (SP_DPAA_VERSION == 1)
static void sp_dpaa1_compare_wr0_to_iv(uint16_t **sp_code)
{
	uint8_t		c, i;
	uint16_t	opcode, jmp_dest;

	/* 31:Compare_WR0_to_IV
	 *	0 1 2 3    4 5    6 7 8 9 10 11 12 13 14 15
	 *	0 1 c[0:1] i[0:1] Jump Destination[0:9]
	 *
	 *	[16-31] [32-47] [48-63] [64-79]
	 *	IV 3    IV 2    IV 1    IV 0
	 *              i > 0   i > 1   i > 2
	 * Performs one of four types of comparisons (specified by c[0:1]) of
	 * the 64-bit value of WR0 to an Immediate Value. The size of the
	 * Intermediate Value can be 16,32,48 or 64-bits (as specified by
	 * i[0:1]) but is padded to 64 bits before comparison. Jumps to the
	 * specified destination when the comparison is true. The hard coded
	 * jump destinations are specified in the Jump instruction.
	 * Comparisons are:
	 *		0 : WR0 == IV
	 *		1 : WR0 != IV
	 *		2 : WR0 > IV
	 *		3 : WR0 < IV
	 *
	 * DPAA1_CMP_WR0_EQ_IMM16	JmpDest, #iv3
	 * DPAA1_CMP_WR0_NE_IMM16	JmpDest, #iv3
	 * DPAA1_CMP_WR0_GT_IMM16	JmpDest, #iv3
	 * DPAA1_CMP_WR0_LT_IMM16	JmpDest, #iv3
	 *
	 * DPAA1_CMP_WR0_EQ_IMM32	JmpDest, #iv3, #iv2
	 * DPAA1_CMP_WR0_NE_IMM32	JmpDest, #iv3, #iv2
	 * DPAA1_CMP_WR0_GT_IMM32	JmpDest, #iv3, #iv2
	 * DPAA1_CMP_WR0_LT_IMM32	JmpDest, #iv3, #iv2
	 *
	 * DPAA1_CMP_WR0_EQ_IMM48	JmpDest, #iv3, #iv2, #iv1
	 * DPAA1_CMP_WR0_NE_IMM48	JmpDest, #iv3, #iv2, #iv1
	 * DPAA1_CMP_WR0_GT_IMM48	JmpDest, #iv3, #iv2, #iv1
	 * DPAA1_CMP_WR0_LT_IMM48	JmpDest, #iv3, #iv2, #iv1
	 *
	 * DPAA1_CMP_WR0_EQ_IMM64	JmpDest, #iv3, #iv2, #iv1, #iv0
	 * DPAA1_CMP_WR0_NE_IMM64	JmpDest, #iv3, #iv2, #iv1, #iv0
	 * DPAA1_CMP_WR0_GT_IMM64	JmpDest, #iv3, #iv2, #iv1, #iv0
	 * DPAA1_CMP_WR0_LT_IMM64	JmpDest, #iv3, #iv2, #iv1, #iv0
	*/

	opcode = **sp_code;
	jmp_dest = opcode & 0x3ff;
	i = (uint8_t)((opcode >> 10) & 0x3);
	c = (uint8_t)((opcode >> 12) & 0x3);
	sp_print_opcode_words(sp_code, i + 2);
	fsl_print("DPAA1_CMP_WR0_%s_IMM%d d:0x%x, ",
		  ((c == 0) ? "EQ" : (c == 1) ? "NE" : (c == 2) ? "GT" : "LT"),
		  16 * (i + 1), jmp_dest);
	sp_print_iv_operands(sp_code, i + 1);
	if (sp_sim.sim_enabled) {
		uint64_t	val64;
		uint8_t		cond;

		val64 = 0;
		load_iv((uint8_t *)&val64, i, sp_code);

		switch (i) {
		default:
		case 0:
			fsl_print("\t\t IMM16 = 0x%04x\n", (uint16_t)val64);
			break;
		case 1:
			fsl_print("\t\t IMM32 = 0x%04x\n", (uint32_t)val64);
			break;
		case 2:
			fsl_print("\t\t IMM48 = 0x%04x-%08x\n",
				  (uint16_t)(val64 >> 32), (uint32_t)val64);
			break;
		case 3:
			fsl_print("\t\t IMM64 = 0x%08x-%08x\n",
				  (uint32_t)(val64 >> 32), (uint32_t)val64);
			break;
		}
		/* Assume condition not verified */
		cond = 0;
		switch (c) {
		default:
		case 0:
			cond = (sp_sim.wr[0] == val64) ? 1 : 0;
			break;
		case 1:
			cond = (sp_sim.wr[0] != val64) ? 1 : 0;
			break;
		case 2:
			cond = (sp_sim.wr[0] > val64) ? 1 : 0;
			break;
		case 3:
			cond = (sp_sim.wr[0] < val64) ? 1 : 0;
			break;
		}
		fsl_print("\t\t Condition %s is %s\n",
			  (c == 0) ? "EQ" : (c == 1) ? "NE" : (c == 2) ? "GT" :
			  "LT", (cond) ? "verified" : "not verified");
		if (cond) {
			fsl_print("\t\t Branch performed\n");
			set_jmp_gosub_destination(sp_code,
						  jmp_dest, 0, 0, 0, 0);
			return;
		}
		fsl_print("\t\t Continue with the next PC = 0x%x\n",
			  sp_sim.pc + i + 2);
	}
	(*sp_code) += 2;
	sp_sim.pc += 2;
	if (i >= 1) {
		(*sp_code)++;
		sp_sim.pc++;
	}
	if (i >= 2) {
		(*sp_code)++;
		sp_sim.pc++;
	}
	if (i == 3) {
		(*sp_code)++;
		sp_sim.pc++;
	}
	ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
}

/******************************************************************************/
static void sp_dpaa1_confirm_layer_mask(uint16_t **sp_code)
{
	/* 1:Confirm_Layer_Mask
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	 *	0 0 0 0 0 0 0 0 0 0 0  0  0  0  0  1
	 *
	 * This instruction will OR in the current layer's Line-up Enable
	 * Confirmation Mask (see Table 8-293) that was read at the beginning
	 * of the current layer. This instruction should only be used by the
	 * soft parser when the soft parser is finishing the parsing at a
	 * current layer (eg. it should not do it on a 0x3fe return code,
	 * but it should run this instruction on a valid (non-errored) 0x3ff
	 * end parse code or when advancing to other hard parse layers
	 * (0x4,0x8,0xc..etc).
	 *
	 * DPAA1_CONFIRM_LAYER_MASK
	*/

	sp_print_opcode_words(sp_code, 1);
	fsl_print("DPAA1_CONFIRM_LAYER_MASK;\n");
	if (sp_sim.sim_enabled) {
		/* TODO - Simulate Line-up Confirmation */
		fsl_print("\t\t DPAA1 : Implemented as NOP !\n");
	}
	(*sp_code)++;
	sp_sim.pc++;
	ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
}

/******************************************************************************/
static void sp_dpaa1_or_iv_lcv(uint16_t **sp_code)
{
	/* 3:OR_IV_LCV
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15	[16-31]	[32-47]
	 *	0 0 0 0 0 0 0 0 0 0 0  0  0  0  1  1	IV3	IV2
	 *
	 * OR's a 32-bit immediate value {IV2,IV3} with the current value of
	 * the Line-up Confirmation Vector. Effectively confirms classification
	 * searches specified in the immediate value.
	 *
	 * DPAA1_OR_IV_LCV	#iv3, #iv2
	*/

	sp_print_opcode_words(sp_code, 3);
	fsl_print("DPAA1_OR_IV_LCV ");
	sp_print_iv_operands(sp_code, 2);
	if (sp_sim.sim_enabled) {
		uint64_t	val64;

		/* TODO - Simulate LCV */
		val64 = 0;
		load_iv((uint8_t *)&val64, 1, sp_code);
		fsl_print("\t\t IMM32 = 0x%04x\n", (uint32_t)val64);
		fsl_print("\t\t DPAA1 : Implemented as NOP !\n");
	}
	(*sp_code) += 3;
	sp_sim.pc += 3;
	ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
}

/******************************************************************************/
static void sp_dpaa1_load_lcv_to_wr(uint16_t **sp_code)
{
	uint16_t	opcode;
	uint8_t		w;

	/* 12:Load_LCV_to_WR
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	 *	0 0 0 0 0 0 0 0 0 1 0  0  0  0  0  W
	 *
	 * The 32-bit value contained in the Line-up Confirmation Vector is
	 * loaded in the least significant 32b of WR0 or WR1 (selected by W).
	 * The most significant 32b of WR0/1 is zeroed.
	 *
	 * DPAA1_LD_LCV_TO_WR0
	 * DPAA1_LD_LCV_TO_WR1
	*/

	opcode = **sp_code;
	w = (uint8_t)(opcode & 0x1);
	sp_print_opcode_words(sp_code, 1);
	fsl_print("DPAA1_LD_LCV_TO_WR%d;\n", w);
	if (sp_sim.sim_enabled) {
		/* TODO - Simulate LCV */
		fsl_print("\t\t DPAA1 : Implemented as NOP !\n");
	}
	(*sp_code)++;
	sp_sim.pc++;
	ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
}

/******************************************************************************/
static void sp_dpaa1_store_wr_to_lcv(uint16_t **sp_code)
{
	uint16_t	opcode;
	uint8_t		w;

	/* 13:Store_WR_to_LCV
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	 *	0 0 0 0 0 0 0 0 0 1 0  0  0  0  1  W
	 *
	 * The least significant 32b of WR0 or WR1 (selected by W) is stored
	 * into the 32b Line-up Confirmation Vector. The current value stored
	 * in the LCV is overwritten.
	 *
	 * DPAA1_ST_WR0_TO_LCV
	 * DPAA1_ST_WR1_TO_LCV
	*/

	opcode = **sp_code;
	w = (uint8_t)(opcode & 0x1);
	sp_print_opcode_words(sp_code, 1);
	fsl_print("DPAA1_ST_WR%d_TO_LCV;\n", w);
	if (sp_sim.sim_enabled) {
		/* TODO - Simulate LCV */
		fsl_print("\t\t DPAA1 : Implemented as NOP !\n");
	}
	(*sp_code)++;
	sp_sim.pc++;
	ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
}
#endif		/* SP_DPAA_VERSION == 1 */

/******************************************************************************/
#if (SP_DPAA_VERSION == 2)
/******************************************************************************/
static void sp_jump_faf(uint16_t **sp_code)
{
	uint16_t	opcode, jmp_dest;
	uint8_t		a, g, l, j;

	/* 25:Jump_FAF
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	 *	0 0 0 0 1 1 0 0 A j[0:6]
	 *
	 *	[16-31] : G[0]L[1]JumpDest[5:15]
	 *
	 * Execution jumps to the specified instruction if the flag indexed by
	 * j[0:6] in the FAF is set.
	 * Used to :
	 *	- jump to another instruction that is part of the same HXS,
	 *	- jump to a new HXS,
	 *	- return from an HXS,
	 *	- to jump to the end of all parsing.
	 * G: The top most bit of the jump destination is labeled G. When set
	 * the jump is treated as a gosub.
	 * L: The next bit(1) is labeled L. When set the jump destination is
	 * treated as relative otherwise it is absolute.
	 *
	 * Listed below are a set on special addresses that only exist in the
	 * absolute space.
	 * Returning to an HXS is only used by sequences that are extensions
	 * to a Hard HXS. A return to HXS is triggered by jumping to a hard
	 * coded value (0x7FE).
	 *
	 * Ending parsing occurs only if the currently executing soft HXS is
	 * the last step in the parse tree. It is triggered by jumping to a
	 * hard coded value(0x7FF).
	 *
	 * If A is set for any type of jump, then the jump is treated as an
	 * advancement to a new HXS (the HB is updated to HB+WO, WO and WRs
	 * are reset). Note that this action can be done even when the jump is
	 * not advancing to a new HXS (although it typically will not be used
	 * this way). The A bit is don't care when doing a Return or Jump to
	 * Hard HXS since the Hard HXS will do the advancement.
	 *
	 * Attempting to access a value beyond the existing FAF will result in
	 * an instruction error and code termination.
	 *
	 * Absolute address destinations
	 *	00: Ethernet		01: LLC+SNAP
	 *	02: VLAN		03: PPPoE+PPP
	 *	04: MPLS		05: ARP
	 *	06: IP			07: IPv4 HXS
	 *	08: IPv6 HXS		09: GRE HXS
	 *	0A: MinEncap HXS	0B: Other L3 Shell
	 *	0C: TCP			0D: UDP
	 *	0E: IPSec		0F: SCTP
	 *	10: DCCP		11: Other L4 Shell
	 *	12: GTP Note : For proper function the NxtHdr field of the Parse
	 *			Result must be populated with the correct port
	 *			to distinguish between GTPU/C and GTP.
	 *			If UDP/TCP was parsed by HXS this is already
	 *			done.
	 *	13: ESP			1E: Other L5+ Shell
	 *	1F: Final Shell		7FE:Return to Hard HXS
	 *	7FF:End Parsing
	 *
	 * JUMP_FAF		#FAF_bit, A|L|S|JmpDest
	 * GOSUB_FAF		#FAF_bit, A|L|S|JmpDest
	*/

	opcode = **sp_code;
	jmp_dest = *(*sp_code + 1);
	j = (uint8_t)(opcode & 0x7f);
	a = (uint8_t)((opcode >> 7) & 0x1);
	l = (uint8_t)((jmp_dest >> 14) & 0x1);
	g = (uint8_t)((jmp_dest >> 15) & 0x1);
	sp_print_opcode_words(sp_code, 2);
	fsl_print("%s bit:%d, ", (g) ? "GOSUB_FAF" : "JUMP_FAF", j);
	sp_print_dest_operand(jmp_dest, a, g, l);
	if (j >= 8 * (sizeof(sp_sim.ra.pr.frame_attribute_flags_extension) +
		      sizeof(sp_sim.ra.pr.frame_attribute_flags_1) +
		      sizeof(sp_sim.ra.pr.frame_attribute_flags_2) +
		      sizeof(sp_sim.ra.pr.frame_attribute_flags_3))) {
		fsl_print("\t\t ERROR : Invalid parameter FAF_bit = %d !\n", j);
		sp_sim.sp_status = SP_ERR_INVAL_PARAM;
		return;
	}
	if (sp_check_destination_address(jmp_dest))
		return;
	jmp_dest &= DEST_ADDR_MASK;
	if (sp_sim.sim_enabled) {
		uint8_t		byte, mask, byte_pos, bit_pos;

		byte_pos = j / 8;
		if (byte_pos < RA_FAF_BYTES) {
			byte_pos += RA_FAF_POSITION;
		} else {
			/* FAF Extension */
			byte_pos -= RA_FAF_BYTES;
			byte_pos += RA_FAF_EXT_POSITION;
		}
		bit_pos = 7 - (j % 8);
		mask = 1 << bit_pos;
		byte = sp_sim.ra_arr[byte_pos];
		if (!(byte & mask)) {
			/* FAF bit is not set : continue with the next
			 * instruction */
			fsl_print("\t\t RA[%d] = %02x mask = %02x\n", byte_pos,
				  sp_sim.ra_arr[byte_pos], mask);
			fsl_print("\t\t FAF bit #%d is not set\n", j);
			fsl_print("\t\t Continue with next PC = 0x%x\n",
				  sp_sim.pc + 2);
			(*sp_code) += 2;
			sp_sim.pc += 2;
			ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
			return;
		}
		set_jmp_gosub_destination(sp_code, jmp_dest, a, g, l, 2);
	} else {
		(*sp_code) += 2;
		sp_sim.pc += 2;
		ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
	}
}

/******************************************************************************/
static void sp_return_sub(uint16_t **sp_code)
{
	/* 4:Return Sub
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	 *	0 0 0 0 0 0 0 0 0 0 0  0  0  1  1  1
	 *
	 * Returns from the previous Gosub call to the immediate next
	 * instruction location (This distance varies depending on which
	 * instruction issued the gosub. Ex. CASE4_DC_WR_to_WR would move 5
	 * address locations forward).
	 * Currently only a stack depth of one is supported. An Invalid Soft
	 * Parser Instruction will be triggered if a Return is attempted when
	 * there was no previous Gosub
	 *
	 * RETURN_SUB
	*/

	sp_print_opcode_words(sp_code, 1);
	fsl_print("RETURN_SUB;\n");
	if (sp_sim.sim_enabled) {
		if (!sp_sim.pc_ret) {
			fsl_print("\t\t ERROR : No return PC !\n");
			sp_sim.sp_status = SP_ERR_INVAL_DST;
			return;
		}
		ASSERT_COND(sp_sim.pc_ret - sp_sim.pc);
		if (sp_sim.pc_ret > sp_sim.pc)
			(*sp_code) += sp_sim.pc_ret - sp_sim.pc;
		else
			(*sp_code) -= sp_sim.pc - sp_sim.pc_ret;
		fsl_print("\t\t Return to PC = 0x%03x\n", sp_sim.pc_ret);
		sp_sim.pc = sp_sim.pc_ret;
		sp_sim.pc_ret = 0;
	} else {
		(*sp_code)++;
		sp_sim.pc++;
		ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
	}
}

/******************************************************************************/
static void sp_set_clear_faf(uint16_t **sp_code)
{
	uint8_t		c, j;
	uint16_t	opcode;

	/* 21:Set_Clr_FAF
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	 *	0 0 0 0 0 0 1 1 c j[0:6]
	 *
	 * Set (c=1) or clear (c=0) the Frame Attribute Flags at the indicated
	 * index (j[0:6]). Attempting to access a value beyond the existing FAF
	 * will result in an instruction error and code termination.
	 *
	 * SET_FAF_BIT		#FAF_bit
	 * CLEAR_FAF_BIT	#FAF_bit
	*/

	opcode = **sp_code;
	j = (uint8_t)(opcode & 0x7f);
	c = (uint8_t)((opcode >> 7) & 0x1);
	sp_print_opcode_words(sp_code, 1);
	fsl_print("%s_FAF_BIT bit:%d;\n", ((c) ? "SET" : "CLEAR"), j);
	if (j >= 8 * (sizeof(sp_sim.ra.pr.frame_attribute_flags_extension) +
		      sizeof(sp_sim.ra.pr.frame_attribute_flags_1) +
		      sizeof(sp_sim.ra.pr.frame_attribute_flags_2) +
		      sizeof(sp_sim.ra.pr.frame_attribute_flags_3))) {
		fsl_print("\t\t ERROR : Invalid parameter FAF_bit = %d !\n", j);
		sp_sim.sp_status = SP_ERR_INVAL_PARAM;
		return;
	}
	if (sp_sim.sim_enabled) {
		uint8_t		byte, mask, byte_pos, bit_pos;

		byte_pos = j / 8;
		if (byte_pos < RA_FAF_BYTES) {
			byte_pos += RA_FAF_POSITION;
		} else {
			/* FAF Extension */
			byte_pos -= RA_FAF_BYTES;
			byte_pos += RA_FAF_EXT_POSITION;
		}
		bit_pos = 7 - (j % 8);
		mask = 1 << bit_pos;
		byte = sp_sim.ra_arr[byte_pos];
		if (c)
			byte |= mask;
		else
			byte &= ~mask;
		sp_sim.ra_arr[byte_pos] = byte;
		fsl_print("\t\t RA[%d] = %02x mask = %02x\n", byte_pos,
			  sp_sim.ra_arr[byte_pos], mask);
	}
	(*sp_code)++;
	sp_sim.pc++;
	ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
}

#endif	/* SP_DPAA_VERSION == 2 */

/******************************************************************************/
static void sp_nop(uint16_t **sp_code)
{
	/* 0: NOP No operation */
	sp_print_opcode_words(sp_code, 1);
	fsl_print("NOP;\n");
	(*sp_code)++;
	sp_sim.pc++;
	ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
}

/******************************************************************************/
static void sp_jump_gosub(uint16_t **sp_code)
{
	/* 27:Jump/Gosub
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	 *	0 0 0 1 1 0 0 0 0 0 0  0  0  0  0  A
	 *
	 *	[16-31] : G[0]L[1]JumpDest[5:15]
	 *
	 * Execution jumps to the specified instruction.
	 * Used to :
	 *	- jump to another instruction that is part of the same HXS,
	 *	- jump to a new HXS,
	 *	- return from an HXS,
	 *	- jump to the end of all parsing.
	 * G: The top most bit of the jump destination is labeled G. When set
	 * the jump is treated as a gosub.
	 * L: The next bit(1) is labeled L. When set the jump destination is
	 * treated as relative otherwise it is absolute.
	 *
	 * Listed below are a set on special addresses that only exist in the
	 * absolute space( Section 1.5.7.2, "Program Counter").
	 *
	 * Returning to an HXS is only used by sequences that are extensions to
	 * a Hard HXS. A return to HXS is triggered by jumping to a hard coded
	 * value (0x7FE).
	 *
	 * Ending parsing occurs only if the currently executing soft HXS is the
	 * last step in the parse tree. It is triggered by jumping to a hard
	 * coded value(0x7FF).
	 *
	 * If A is set for any type of jump, then the jump is treated as an
	 * advancement to a new HXS (the HB is updated to HB+WO, WO and WRs are
	 * reset). Note that this action can be done even when the jump is not
	 * advancing to a new HXS (although it typically will not be used this
	 * way).
	 * The A bit is don't care when doing a Return or Jump to Hard HXS since
	 * the Hard HXS will do the advancement.
	 *
	 * Absolute address destinations
	 *	00: Ethernet		01: LLC+SNAP
	 *	02: VLAN		03: PPPoE+PPP
	 *	04: MPLS		05: ARP
	 *	06: IP			07: IPv4 HXS
	 *	08: IPv6 HXS		09: GRE HXS
	 *	0A: MinEncap HXS	0B: Other L3 Shell
	 *	0C: TCP			0D: UDP
	 *	0E: IPSec		0F: SCTP
	 *	10: DCCP		11: Other L4 Shell
	 *	12: GTP Note : For proper function the NxtHdr field of the Parse
	 *			Result must be populated with the correct port
	 *			to distinguish between GTPU/C and GTP.
	 *			If UDP/TCP was parsed by HXS this is already
	 *			done.
	 *	13: ESP			1E: Other L5+ Shell
	 *	1F: Final Shell		7FE:Return to Hard HXS
	 *	7FF:End Parsing
	 *
	 * ***************
	 * Gosub feature:
	 * ***************
	 * Each call of this instruction overwrites a "return index" with the
	 * current index+1. If the matching Return Sub command is encountered
	 * it will return to this captured index otherwise this instruction
	 * just acts as an ordinary jump.
	 *
	 * JUMP		A|L|S|JmpDest
	 * GOSUB	A|G|L|S|JmpDest
	*/
	/* 28:Jump
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	 *	0 0 0 1 1 A Jump Destination[0:9]
	 *
	 * Execution jumps to the specified instruction.
	 * Used to jump to :
	 *	- another instruction that is part of the same HXS,
	 *	- a new HXS,
	 *	- return from an HXS,
	 *	- the end of all parsing.
	 * When jumping to a soft HXS, the jump destination value is the actual
	 * instruction address of the first instruction of a soft HXS.
	 *
	 * When jumping to a hard HXS, the jump destination is the hard coded
	 * value for the desired hard HXS.
	 *
	 * Returning to an HXS is only used by sequences that are extensions to
	 * a Hard HXS. A return to HXS is triggered by jumping to a hard coded
	 * value (0x3FE).
	 *
	 * Ending parsing occurs only if the currently executing soft HXS is the
	 * last step in the parse tree. It is triggered by jumping to a hard
	 * coded value.
	 *
	 * If A is set for any type of jump, then the jump is treated as an
	 * advancement to a new HXS (the HB is updated to HB+WO, WO and WRs are
	 * reset). Note that this action can be done even when the jump is not
	 * advancing to a new HXS (although it typically will not be used this
	 * way).
	 * The A bit is don't care when doing a Return or Jump to Hard HXS since
	 * the Hard HXS will do the advancement.
	 *
	 *	00: Ethernet			01: LLC+SNAP
	 *	02: VLAN			03: PPPoE+PPP
	 *	04: MPLS			05: IPv4 HXS
	 *	06: IPv6 HXS			07: GRE HXS
	 *	08: MinEncap HXS		09: Other L3 Shell
	 *	0A: TCP				0B: UDP
	 *	0C: IPSec Shell			0D: SCTP Shell
	 *	0E: DCCP Shell			0F: Other L4 Shell
	 *	3FE:Return to Hard HXS		3FF:End Parsing
	 *
	 * DPAA1_JUMP	A|JmpDest
	*/

	uint16_t	opcode, jmp_dest;
	uint8_t		a;

#if (SP_DPAA_VERSION == 1)
	opcode = **sp_code;
	jmp_dest = opcode & 0x3ff;
	a = (uint8_t)((opcode >> 10) & 0x01);
	sp_print_opcode_words(sp_code, 1);
	fsl_print("DPAA1_JUMP ");
	sp_print_dest_operand(jmp_dest, a, 0, 0);
	if (sp_check_destination_address(jmp_dest))
		return;
	if (sp_sim.sim_enabled) {
		set_jmp_gosub_destination(sp_code, jmp_dest, a, 0, 0, 0);
	} else {
		(*sp_code)++;
		sp_sim.pc++;
		ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
	}
#else
	uint8_t		g, l;

	opcode = **sp_code;
	jmp_dest = *(*sp_code + 1);
	a = (uint8_t)(opcode & 0x1);
	l = (uint8_t)((jmp_dest >> 14) & 0x1);
	g = (uint8_t)((jmp_dest >> 15) & 0x1);
	sp_print_opcode_words(sp_code, 2);
	fsl_print("%s", (g) ? "GOSUB " : "JUMP ");
	sp_print_dest_operand(jmp_dest, a, g, l);
	if (sp_check_destination_address(jmp_dest))
		return;
	jmp_dest &= DEST_ADDR_MASK;
	if (sp_sim.sim_enabled) {
		set_jmp_gosub_destination(sp_code, jmp_dest, a, g, l, 2);
	} else {
		(*sp_code) += 2;
		sp_sim.pc += 2;
		ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
	}
#endif
}

/******************************************************************************/
static void sp_load_bits_fw_to_wr(uint16_t **sp_code)
{
	uint8_t		m, n, w, s;
	uint16_t	opcode;

	/* 30:Load_Bits_FW_to_WR(m-n,m) :
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	 *	1 S m[0:6]        n[0:5]           W
	 *
	 * Load n+1 bits into the WR 0 or WR1 (as specified by W) from the bit
	 * positions m-n to m of Frame Window. (It is required that m-n >= 0.)
	 * The shift option (specified by S=1) causes the current value of the
	 * target WR to be shifted up by n+1 bits and reloaded. Note that in
	 * the case where m and n are integer multiples of 8, this instruction
	 * performs byte movements.
	 *
	 * LD_FW_TO_WR0		#from_position, #bits
	 * LDS_FW_TO_WR0	#from_position, #bits
	 * LD_FW_TO_WR1		#from_position, #bits
	 * LDS_FW_TO_WR1	#from_position, #bits
	*/
	opcode = **sp_code;
	m = (uint8_t)((opcode >> 7) & 0x7f);
	n = (uint8_t)((opcode >> 1) & 0x3f);
	if (m < n) {
		fsl_print("\t\t ERROR : Invalid parameter m(%d) < n(%d) !\n",
			  m, n);
		sp_sim.sp_status = SP_ERR_INVAL_PARAM;
		return;
	}
	s = (uint8_t)((opcode >> 14) & 0x1);
	w = (uint8_t)(opcode & 0x1);
	sp_print_opcode_words(sp_code, 1);
	if (s)
		fsl_print("LDS_FW_TO_WR%d from_bit:%d, bits:%d;\n",
			  w, m - n, n + 1);
	else
		fsl_print("LD_FW_TO_WR%d from_bit:%d, bits:%d;\n",
			  w, m - n, n + 1);
	if (sp_sim.sim_enabled) {
		uint8_t		*fw_first_byte;
		uint8_t		extracted_bytes[9] = {0, 0, 0, 0,
							0, 0, 0, 0, 0};
		int		bits, i, j, k, l;
		uint64_t	val64, mask;

		ASSERT_COND(sp_sim.hb + sp_sim.wo < SP_SIM_MAX_FRM_LEN);
		fw_first_byte = &sp_sim.frm[sp_sim.hb + sp_sim.wo];
		bits = (m + 1) % 8;
		i = (m - n) / 8;
		j = (m + 1) / 8 + (bits != 0);
		for (l = 0, k = j - 1; k >= i; k--, l++)
			extracted_bytes[8 - l] = fw_first_byte[k];
		val64 = *(uint64_t *)&extracted_bytes[1];
		if (bits) {
			uint64_t	swap_val64;

			swap_val64 = LLLDW_SWAP(0, &val64);
			swap_val64 >>= (8 - bits);
			val64 = LLLDW_SWAP(0, &swap_val64);
		}
		mask = (uint64_t)(~0ll) >> 64 - (n + 1);
		if (s) {
			uint64_t	tmp;

			tmp = (LLLDW_SWAP(0, sp_sim.wr[w])) << n + 1;
			sp_sim.wr[w] = LLLDW_SWAP(0, &tmp);
			if (n + 1 >= 64)
				sp_sim.wr[w] = 0;
			sp_sim.wr[w] |= val64 & mask;
		} else {
			sp_sim.wr[w] = val64 & mask;
		}
		fsl_print("\t\t WR%d = 0x%08x-%08x from PKT[%d:%d]\n",
			  w,
			  (uint32_t)(sp_sim.wr[w] >> 32),
			  (uint32_t)sp_sim.wr[w],
			  sp_sim.hb + sp_sim.wo + (m - n) / 8,
			  sp_sim.hb + sp_sim.wo +
			  (m + 1) / 8 + (bits != 0) - 1);
	}
	(*sp_code)++;
	sp_sim.pc++;
	ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
}

/******************************************************************************/
static void sp_load_bytes_pa_to_wr(uint16_t **sp_code)
{
	uint8_t		j, k, w, s;
	uint16_t	opcode;

	/* 26:Load_Bytes_PA_to_WR :
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	 *	0 0 0 1 0 S j[0:5]        k[0:2]   W
	 *
	 * Load the k+1 bytes from byte positions j-k to j of the Soft
	 * Examination Parameter Array into WR0 or WR1 (as specified by W).
	 * Since loads to the working registers are always 64-bits, the bytes
	 * to the left of the k+1 bytes are zeroed in the selected WR unless
	 * the shift option is specified. If the shift option is specified
	 * (S=1), the current value of the selected WR is shifted left by k+1
	 * bytes and reloaded into that position to the left of the newly
	 * loaded bytes. (Logically it appears as if the k+1 bytes have been
	 * shifted into or concatenated with the existing value.)
	 *
	 * LD_PA_TO_WR0		#from_position, #bytes
	 * LDS_PA_TO_WR0	#from_position, #bytes
	 * LD_PA_TO_WR1		#from_position, #bytes
	 * LDS_PA_TO_WR1	#from_position, #bytes
	*/
	opcode = **sp_code;
	j = (uint8_t)((opcode >> 4) & 0x3f);
	k = (uint8_t)((opcode >> 1) & 0x7);
	if (j < k) {
		fsl_print("\t\t ERROR : Invalid parameter j(%d) < k(%d) !\n",
			  j, k);
		sp_sim.sp_status = SP_ERR_INVAL_PARAM;
		return;
	}
	s = (uint8_t)((opcode >> 10) & 0x1);
	w = (uint8_t)(opcode & 0x1);
	sp_print_opcode_words(sp_code, 1);
	if (s)
		fsl_print("LDS_PA_TO_WR%d from_byte:%d, bytes:%d;\n",
			  w, j - k, k + 1);
	else
		fsl_print("LD_PA_TO_WR%d from_byte:%d, bytes:%d;\n",
			  w, j - k, k + 1);
	if (sp_sim.sim_enabled) {
		uint8_t		extracted_bytes[8] = {0, 0, 0, 0, 0, 0, 0, 0};
		int		i;
		uint64_t	val64, mask;

		ASSERT_COND(sp_sim.hb + sp_sim.wo < SP_SIM_MAX_FRM_LEN);
		for (i = 0; i < k + 1; i++)
			extracted_bytes[7 - i] = sp_sim.pa[j - i];
		val64 = *(uint64_t *)&extracted_bytes[0];
		mask = (uint64_t)(~0ll) >> 64 - 8 * (k + 1);
		if (s) {
			uint64_t	tmp;

			tmp = (LLLDW_SWAP(0, sp_sim.wr[w])) << 8 * (k + 1);
			sp_sim.wr[w] = (LLLDW_SWAP(0, &tmp)) & mask;
			if (8 * (k + 1) >= 64)
				sp_sim.wr[w] = 0;
			sp_sim.wr[w] |= val64 & mask;
		} else {
			sp_sim.wr[w] = val64 & mask;
		}
		fsl_print("\t\t WR%d = 0x%08x-%08x from PA[%d:%d]\n",
			  w,
			  (uint32_t)(sp_sim.wr[w] >> 32),
			  (uint32_t)sp_sim.wr[w],
			  j - k, j);
	}
	(*sp_code)++;
	sp_sim.pc++;
	ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
}

/******************************************************************************/
static void sp_cmp_wr_jump(uint16_t **sp_code)
{
	uint8_t		c, g, l;
	uint16_t	opcode, jmp_dest;

	/* 15:Compare Working Regs WR0 <?> WR1 :
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15	16-31
	 *	0 0 0 0 0 0 0 0 0 1 1 1 1       c[0:2]	G[0]L[1]JumpDest[5:15]
	 *
	 * When c[0:2] = 0..5, a conditional jump may be performed based on the
	 * comparison of the 64-bit value contained in WR0 to the 64 bit value
	 * in WR1:
	 *	0 : if (WR0 = WR1) then jump to destination[5:15]
	 *	1 : if (WR0 != WR1) then jump to destination[5:15]
	 *	2 : if (WR0 > WR1) then jump to destination[5:15]
	 *	3 : if (WR0 < WR1) then jump to destination[5:15]
	 *	4 : if (WR0 >= WR1) then jump to destination[5:15]
	 *	5 : if (WR0 <= WR1) then jump to destination[5:15]
	 * If c[0:2] is assigned to 6 or 7, then no jump is performed and the
	 * next instruction is executed.
	 * The hard coded jump destinations are specified in the Jump
	 * instruction.
	 * G: The top most bit of the jump destination is labeled G. When set
	 * the jump is treated as a gosub.
	 * L: The next bit(1) is labeled L. When set the jump destination is
	 * treated as relative otherwise it is absolute.
	 *
	 * CMP_WR0_EQ_WR1		G|L|S|JmpDest
	 * CMP_WR0_NE_WR1		G|L|S|JmpDest
	 * CMP_WR0_GT_WR1		G|L|S|JmpDest
	 * CMP_WR0_LT_WR1		G|L|S|JmpDest
	 * CMP_WR0_GE_WR1		G|L|S|JmpDest
	 * CMP_WR0_LE_WR1		G|L|S|JmpDest
	 * CMP_WR0_XX_WR1		G|L|S|JmpDest
	 *
	 * Notes :
	 *	1.  The G(gosub), L(relative addressing), and S(sign bit) flags
	 *	may be used only on the DPAA2 version
	 *	2. JmpDest is on 11 bits (10:0) on DPAA2 and on 10 bits (9:0)
	 *	on DPAA2 */
	opcode = **sp_code;
	jmp_dest = *(*sp_code + 1);
	c = (uint8_t)(opcode & 0x7);
#if (SP_DPAA_VERSION == 1)
	l = 0;
	g = 0;
#else
	l = (uint8_t)((jmp_dest >> 14) & 0x1);
	g = (uint8_t)((jmp_dest >> 15) & 0x1);
#endif
	sp_print_opcode_words(sp_code, 2);
	fsl_print("CMP_WR0_%s_WR1 ", (c == 0) ? "EQ" : (c == 1) ? "NE" :
		  (c == 2) ? "GT" : (c == 3) ? "LT" : (c == 4) ? "GE" :
		  (c == 5) ? "LE" : "XX");
	sp_print_dest_operand(jmp_dest, 0, g, l);
	if (sp_check_destination_address(jmp_dest))
		return;
	jmp_dest &= DEST_ADDR_MASK;
	if (sp_sim.sim_enabled) {
		uint8_t		cond = 0;

		switch (c) {
		case 0:
			cond = (sp_sim.wr[0] == sp_sim.wr[0]) ? 1 : 0;
			break;
		case 1:
			cond = (sp_sim.wr[0] != sp_sim.wr[0]) ? 1 : 0;
			break;
		case 2:
			cond = (sp_sim.wr[0] > sp_sim.wr[0]) ? 1 : 0;
			break;
		case 3:
			cond = (sp_sim.wr[0] < sp_sim.wr[0]) ? 1 : 0;
			break;
		case 4:
			cond = (sp_sim.wr[0] >= sp_sim.wr[0]) ? 1 : 0;
			break;
		case 5:
			cond = (sp_sim.wr[0] <= sp_sim.wr[0]) ? 1 : 0;
			break;
		default:
			break;
		}
		fsl_print("\t\t WR0 = 0x%08x-%08x\n",
			  (uint32_t)(sp_sim.wr[0] >> 32),
			  (uint32_t)sp_sim.wr[0]);
		fsl_print("\t\t WR1 = 0x%08x-%08x\n",
			  (uint32_t)(sp_sim.wr[1] >> 32),
			  (uint32_t)sp_sim.wr[1]);
		fsl_print("\t\t Condition %s is %s\n",
			  (c == 0) ? "EQ" : (c == 1) ? "NE" : (c == 2) ? "GT" :
			  (c == 3) ? "LT" : (c == 4) ? "GE" : (c == 5) ? "LE" :
			  "XX", (cond) ? "verified" : "not verified");
		if (cond) {
			fsl_print("\t\t Branch performed\n");
			set_jmp_gosub_destination(sp_code,
						  jmp_dest, 0, g, l, 2);
			return;
		}
		fsl_print("\t\t Continue with the next PC = 0x%x\n",
			  sp_sim.pc + 2);
	}
	(*sp_code) += 2;
	sp_sim.pc += 2;
	ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
}

/******************************************************************************/
static void sp_load_bytes_ra_to_wr(uint16_t **sp_code)
{
	uint8_t		j, k, w, s;
	uint16_t	opcode;

	/* 29:Load_Bytes_RA_to_WR :
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	 *	0 0 1 1 S j[0:6]          k[0:2]   W
	 *
	 * Load the k+1 bytes from byte positions j-k to j of the Parse Array
	 * into WR0 or WR1 (as specified by W). Since loads to the working
	 * registers are always 64-bits, the bytes to the left of the k+1 bytes
	 * are zeroed in the selected WR unless the shift option is specified.
	 * If the shift option is specified (S=1), the current value of the
	 * selected WR is shifted left by k+1 bytes and reloaded into that
	 * position to the left of the newly loaded bytes. (Logically it appears
	 * as if the k+1 bytes have been shifted into or concatenated with the
	 * existing value.)
	 *
	 * LD_RA_TO_WR0		#from_position, #bytes
	 * LDS_RA_TO_WR0	#from_position, #bytes
	 * LD_RA_TO_WR1		#from_position, #bytes
	 * LDS_RA_TO_WR1	#from_position, #bytes
	*/
	opcode = **sp_code;
	j = (uint8_t)((opcode >> 4) & 0x3f);
	k = (uint8_t)((opcode >> 1) & 0x7);
	if (j < k) {
		fsl_print("\t\t ERROR : Invalid parameter j(%d) < k(%d) !\n",
			  j, k);
		sp_sim.sp_status = SP_ERR_INVAL_PARAM;
		return;
	}
	s = (uint8_t)((opcode >> 11) & 0x1);
	w = (uint8_t)(opcode & 0x1);
	sp_print_opcode_words(sp_code, 1);
	if (s)
		fsl_print("LDS_RA_TO_WR%d from_byte: %d, bytes:%d;\n",
			  w, j - k, k + 1);
	else
		fsl_print("LD_RA_TO_WR%d from_byte:%d, bytes:%d;\n",
			  w, j - k, k + 1);
	if (sp_sim.sim_enabled) {
		uint8_t		extracted_bytes[8] = {0, 0, 0, 0, 0, 0, 0, 0};
		int		i;
		uint64_t	val64, mask;

		ASSERT_COND(sp_sim.hb + sp_sim.wo < SP_SIM_MAX_FRM_LEN);
		for (i = 0; i < k + 1; i++)
			extracted_bytes[7 - i] = sp_sim.ra_arr[j - i];
		val64 = *(uint64_t *)&extracted_bytes[0];
		mask = (uint64_t)(~0ll) >> 64 - 8 * (k + 1);
		if (s) {
			uint64_t	tmp;

			tmp = (LLLDW_SWAP(0, sp_sim.wr[w])) << 8 * (k + 1);
			sp_sim.wr[w] = (LLLDW_SWAP(0, &tmp)) & mask;
			if (8 * (k + 1) >= 64)
				sp_sim.wr[w] = 0;
			sp_sim.wr[w] |= val64 & mask;
		} else {
			sp_sim.wr[w] = val64 & mask;
		}
		fsl_print("\t\t WR%d = 0x%08x-%08x from RA[%d:%d]\n",
			  w,
			  (uint32_t)(sp_sim.wr[w] >> 32),
			  (uint32_t)sp_sim.wr[w],
			  j - k, j);
	}
	(*sp_code)++;
	sp_sim.pc++;
	ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
}

/******************************************************************************/
static void sp_advance_hb_by_wo(uint16_t **sp_code)
{
	/* 1:Advance_HB_by_WO
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	 *	0 0 0 0 0 0 0 0 0 0 0  0  0  0  1  0
	 *
	 * Add the value of Window Offset to Header Base & reset the value of
	 * WO to zero. Used within an HXS to advance to HB for the following
	 * HXS. It is appropriate to apply when WO is pointing to the first
	 * byte that the next HXS is to examine.
	 *
	 * ADVANCE_HB_BY_WO
	*/
	sp_print_opcode_words(sp_code, 1);
	fsl_print("ADVANCE_HB_BY_WO;\n");
	if (sp_sim.sim_enabled) {
		sp_sim.hb += sp_sim.wo;
		sp_sim.wo = 0;
		fsl_print("\t\t HB = %d; WO = %d\n", sp_sim.hb, sp_sim.wo);
	}
	(*sp_code)++;
	sp_sim.pc++;
	ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
}

/******************************************************************************/
static void sp_modify_wo_by_wr(uint16_t **sp_code)
{
	uint8_t		a, w;
	uint16_t	opcode;

	/* 16:Modify WO by WR
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	 *	0 0 0 0 0 0 0 0 1 0 0  0  0  0  A  W
	 *
	 * Either loads (if A=0) the least significant 8b of WR0/1 (depending
	 * on W) into the Window Offset register, or adds in the least
	 * significant 8b of WR0/1 to the current Window Offset value (if A=1)
	 *
	 * LD_WR0_TO_WO
	 * LD_WR1_TO_WO
	 * ADD_WR0_TO_WO
	 * ADD_WR1_TO_WO
	*/
	opcode = **sp_code;
	w = (uint8_t)(opcode & 0x1);
	a = (uint8_t)((opcode >> 1) & 0x1);
	sp_print_opcode_words(sp_code, 1);
	if (a)
		fsl_print("ADD_WR%d_TO_WO;\n", w);
	else
		fsl_print("LD_WR%d_TO_WO;\n", w);
	if (sp_sim.sim_enabled) {
		if (a)
			sp_sim.wo += (uint8_t)sp_sim.wr[w];
		else
			sp_sim.wo = (uint8_t)sp_sim.wr[w];
		fsl_print("\t\t WO = %d\n", sp_sim.wo);
	}
	(*sp_code)++;
	sp_sim.pc++;
	ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
}

/******************************************************************************/
static void sp_add_sub_wr_iv_to_wr(uint16_t **sp_code)
{
	uint8_t		s, v, o, w;
	uint16_t	opcode;

	/* 13:AddSub_WR_IV_to_WR
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15	16-31	[32-47]
	 *	0 0 0 0 0 0 0 0 0 1 0  1  S  V  O  W	IV 3	[IV 2]
	 *
	 * The least significant 32b of WR0 (W=0) or WR1 (W=1) is the first
	 * operand and 16b (S=0) or 32b (S=1) of immediate data is the second
	 * operand in a 32b addition (O=0) or a 32b subtraction (O=1) operation.
	 * The 32b result is stored in the least significant 32b of WR0 (V=0) or
	 * WR1 (V=1). No carries or underflow bits are captured. The most
	 * significant 32b of WR0 and WR1 are not affected by this instruction
	 * (they hold their value).
	 *
	 * ADD32_WR0_IMM16_TO_WR0	#iv3
	 * ADD32_WR0_IMM32_TO_WR0	#iv3, #iv2
	 * ADD32_WR0_IMM16_TO_WR1	#iv3
	 * ADD32_WR0_IMM32_TO_WR1	#iv3, #iv2
	 * ADD32_WR1_IMM16_TO_WR1	#iv3
	 * ADD32_WR1_IMM32_TO_WR1	#iv3, #iv2
	 * ADD32_WR1_IMM16_TO_WR0	#iv3
	 * ADD32_WR1_IMM32_TO_WR0	#iv3, #iv2
	 * SUB32_WR0_IMM16_TO_WR0	#iv3
	 * SUB32_WR0_IMM32_TO_WR0	#iv3, #iv2
	 * SUB32_WR0_IMM16_TO_WR1	#iv3
	 * SUB32_WR0_IMM32_TO_WR1	#iv3, #iv2
	 * SUB32_WR1_IMM16_TO_WR1	#iv3
	 * SUB32_WR1_IMM32_TO_WR1	#iv3, #iv2
	 * SUB32_WR1_IMM16_TO_WR0	#iv3
	 * SUB32_WR1_IMM32_TO_WR0	#iv3, #iv2
	*/
	opcode = **sp_code;
	w = (uint8_t)(opcode & 0x1);
	o = (uint8_t)((opcode >> 1) & 0x1);
	v = (uint8_t)((opcode >> 2) & 0x1);
	s = (uint8_t)((opcode >> 3) & 0x1);
	sp_print_opcode_words(sp_code, s + 2);
	if (o)
		fsl_print("SUB32_WR%d_IMM%d_TO_WR%d ", w, 16 * (s + 1), v);
	else
		fsl_print("ADD32_WR%d_IMM%d_TO_WR%d ", w, 16 * (s + 1), v);
	sp_print_iv_operands(sp_code, s + 1);
	if (sp_sim.sim_enabled) {
		uint32_t	wsrc32, wdst32, imm32;
		uint64_t	val64;

		val64 = 0;
		load_iv((uint8_t *)&val64, s, sp_code);
		wsrc32 = (uint32_t)sp_sim.wr[w];
		wdst32 = (uint32_t)sp_sim.wr[v];
		imm32 = (uint32_t)val64;
		if (o)
			wdst32 = wsrc32 - imm32;
		else
			wdst32 = wsrc32 + imm32;
		sp_sim.wr[v] = (sp_sim.wr[v] & 0xffffffff00000000UL) | wdst32;
		fsl_print("\t\t WR%d   = 0x%08x-%08x\n", v,
			  (uint32_t)(sp_sim.wr[v] >> 32),
			  (uint32_t)sp_sim.wr[v]);
	}
	(*sp_code) += 2;
	sp_sim.pc += 2;
	if (s) {
		(*sp_code)++;
		sp_sim.pc++;
	}
	ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
}

/******************************************************************************/
static void sp_store_wr_to_ra(uint16_t **sp_code)
{
	uint8_t		s, t, w;
	uint16_t	opcode;

	/* 28:Store_WR_to_RA
	 *	0 1 2 3 4 5 6 7  8 9 10 11 12 13 14 15
	 *	0 0 1 0 1 s[0:2] t[0:6]             W
	 *
	 * Stores the s+1 leftmost bytes (least significant) of WR0 or WR1 (as
	 * specified by W) into the Parse Array at byte positions t-s to t.
	 *
	 * ST_WR0_TO_RA	#to_position, #bytes
	 * ST_WR1_TO_RA	#to_position, #bytes
	 *
	*/
	opcode = **sp_code;
	t = (uint8_t)((opcode >> 1) & 0x3f);
	s = (uint8_t)((opcode >> 8) & 0x7);
	if (t < s) {
		fsl_print("\t\t ERROR : Invalid parameter t(%d) < s(%d) !\n",
			  t, s);
		sp_sim.sp_status = SP_ERR_INVAL_PARAM;
		return;
	}
	w = (uint8_t)(opcode & 0x1);
	sp_print_opcode_words(sp_code, 1);
	fsl_print("ST_WR%d_TO_RA to_byte:%d, bytes:%d;\n", w, t - s, s + 1);
	if (sp_sim.sim_enabled) {
		uint8_t		*pb, i;

		pb = (uint8_t *)&sp_sim.wr[w];
		pb += 7 - s;
		fsl_print("\t\t RA[%d:%d] = ", t - s, t);
		for (i = 0; i < s + 1; i++) {
			sp_sim.ra_arr[t - s + i] = *pb++;
			fsl_print("%02x ", sp_sim.ra_arr[t - s + i]);
		}
		fsl_print("\n");
	}
	(*sp_code)++;
	sp_sim.pc++;
	ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
}

/******************************************************************************/
static void sp_add_sub_wr_to_wr(uint16_t **sp_code)
{
	uint8_t		v, o, l;
	uint16_t	opcode;

	/* 12:AddSub_WR_WR_to_WR
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	 *	0 0 0 0 0 0 0 0 0 1 0  0  1  V  O  L
	 *
	 * The least significant 32b of WR0 (L=0) or WR1 (L=1) is the first
	 * operand and the least significant 32b of WR1 or WR0 is the second
	 * operand in a 32b addition (O=0) or a 32b subtraction (O=1) operation.
	 * The 32b result is stored in the least significant 32b of WR0 (V=0) or
	 * WR1 (V=1). No carries or underflow bits are captured. The most
	 * significant 32b of WR0 and WR1 are not affected by this instruction
	 * (they hold their value).
	 *
	 * ADD32_WR0_TO_WR0
	 * ADD32_WR0_TO_WR1
	 * ADD32_WR1_TO_WR1
	 * ADD32_WR1_TO_WR0
	 * SUB32_WR0_FROM_WR0
	 * SUB32_WR0_FROM_WR1
	 * SUB32_WR1_FROM_WR1
	 * SUB32_WR1_FROM_WR0
	*/
	opcode = **sp_code;
	l = (uint8_t)(opcode & 0x1);
	o = (uint8_t)((opcode >> 1) & 0x1);
	v = (uint8_t)((opcode >> 2) & 0x1);
	sp_print_opcode_words(sp_code, 1);
	if (o)
		fsl_print("SUB32_WR%d_FROM_WR%d;\n", l, v);
	else
		fsl_print("ADD32_WR%d_TO_WR%d;\n", l, v);
	if (sp_sim.sim_enabled) {
		uint32_t	wsrc32, wdst32;

		wsrc32 = (uint32_t)sp_sim.wr[l];
		wdst32 = (uint32_t)sp_sim.wr[v];
		if (o)
			wdst32 -= wsrc32;
		else
			wdst32 += wsrc32;
		sp_sim.wr[v] = (sp_sim.wr[v] & 0xffffffff00000000UL) | wdst32;
		fsl_print("\t\t WR%d   = 0x%08x-%08x\n", v,
			  (uint32_t)(sp_sim.wr[v] >> 32),
			  (uint32_t)sp_sim.wr[v]);
	}
	(*sp_code)++;
	sp_sim.pc++;
	ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
}

/******************************************************************************/
static void sp_jump_protocol(uint16_t **sp_code)
{
	uint8_t		p;
	uint16_t	opcode;

	/* 11:Jump_Protocol
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	 *	0 0 0 0 0 0 0 0 0 1 0  0  0  1  P[0:1]
	 *
	 * Execution jumps to the HXS decoded in the NxtHdr field of the Parse
	 * Array. Used to jump to the next HXS based on a protocol code.
	 * The jump is treated as an advancement to a new HXS (the HB is
	 * updated to HB+WO, WO and WRs are reset).
	 *
	 * If the P code is zero, NxtHdr is evaluated against the following
	 * EtherTypes:
	 *		-0x05DC of less: jump to LLC-SNAP
	 *		-0x0800: jump to IPv4
	 *		-0x0806: jump to ARP
	 *		-0x86dd: jump to IPv6
	 *		-0x8847, 0x8848: jump to MPLS
	 *		-0x8100, 0x88A8,ConfigTPID1,ConfigTPID2: jump to VLAN
	 *		-0x8864: jump to PPPoE+PPP
	 *		-unknown: jump to Other L3 Shell
	 *
	 * If the P code is one, NxtHdr is evaluated against the following IP
	 * protocol codes:
	 *		-4: jump to IPv4
	 *		-6: jump to TCP
	 *		-17: jump to UDP
	 *		-33: jump to DCCP
	 *		-41: jump to IPv6
	 *		-47: jump to GRE
	 *	l	-50,51: jump to IPSec
	 *		-55: jump to MinEncap
	 *		-132: jump to SCTP
	 *		-unknown: jump to Other L4 Shell
	 *
	 * If the P code is two, NxtHdr is evaluated against the following
	 * TCP/UDP ports:
	 *		-2123: jump to GTP(GTP-C)
	 *		-2152: jump to GTP(GTP-U)
	 *		-3386: jump to GTP(GTP)
	 *		-4500: jump to ESP
	 *		-4789: jump to VXLAN
	 *		-unknown: jump to Other L5+ Shell
	 *
	 * P code of three is invalid. Defaults to 0.
	 *
	 * JUMP_TO_L2_NEXT_HEADER
	 * JUMP_TO_L3_NEXT_HEADER
	 * JUMP_TO_L4_NEXT_HEADER
	 *
	*/

	opcode = **sp_code;
	p = (uint8_t)(opcode & 0x3);
	if (p == 3)
		p = 0;
	sp_print_opcode_words(sp_code, 1);
	fsl_print("JUMP_TO_L%d_NEXT_HEADER;\n", p + 2);
	if (sp_sim.sim_enabled) {
		uint16_t	nxt_hdr;
		int		i;

		sp_sim.hb += sp_sim.wo;
		sp_sim.wo = 0;
		for (i = 0; i < NUM_WR; i++)
			sp_sim.wr[i] = 0;
		fsl_print("\t\t HB = %d; WO = %d\n", sp_sim.hb, sp_sim.wo);
		for (i = 0; i < NUM_WR; i++)
			fsl_print("\t\t WR%d = 0x%08x-%08x\n", i,
				  (uint32_t)(sp_sim.wr[i] >> 32),
				  (uint32_t)sp_sim.wr[i]);
		nxt_hdr = sp_sim.ra.pr.nxt_hdr;
		fsl_print("\t\t Jump to HXS protocol : ");
		if (p == 0) {
			if (nxt_hdr <= 0x05DC)
				fsl_print("LLC-SNAP\n");
			else if (nxt_hdr == 0x0800)
				fsl_print("IPv4\n");
			else if (nxt_hdr == 0x0806)
				fsl_print("ARP\n");
			else if (nxt_hdr == 0x86DD)
				fsl_print("IPv6\n");
			else if (nxt_hdr == 0x8847 || nxt_hdr == 0x8848)
				fsl_print("MPLS\n");
			else if (nxt_hdr == 0x8864)
				fsl_print("PPPoE+PPP\n");
			else
				fsl_print("Other L3 Shell\n");
		} else if (p == 1) {
			if (nxt_hdr == 4)
				fsl_print("IPv4\n");
			else if (nxt_hdr == 6)
				fsl_print("TCP\n");
			else if (nxt_hdr == 17)
				fsl_print("UDP\n");
			else if (nxt_hdr == 33)
				fsl_print("DCCP\n");
			else if (nxt_hdr == 41)
				fsl_print("IPv6\n");
			else if (nxt_hdr == 47)
				fsl_print("GRE\n");
			else if (nxt_hdr == 50 || nxt_hdr == 51)
				fsl_print("IPSec\n");
			else if (nxt_hdr == 55)
				fsl_print("MinEncap\n");
			else if (nxt_hdr == 132)
				fsl_print("SCTP\n");
			else
				fsl_print("Other L4 Shell\n");
		} else {
			if (nxt_hdr == 2123)
				fsl_print("GTP-C\n");
			else if (nxt_hdr == 2152)
				fsl_print("GTP-U\n");
			else if (nxt_hdr == 3386)
				fsl_print("GTP\n");
			else if (nxt_hdr == 4500)
				fsl_print("ESP\n");
			else if (nxt_hdr == 4789)
				fsl_print("VXLAN\n");
			else
				fsl_print("Other L5 Shell\n");
		}
		sp_sim.sp_status = SP_HARD_HXS_CALLED;
	} else {
		(*sp_code)++;
		sp_sim.pc++;
		ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
	}
}

/******************************************************************************/
static void sp_bitwise_wr_iv_to_wr(uint16_t **sp_code)
{
	uint8_t		v, i, f, w;
	uint16_t	opcode;

	/* 17:Bitwise_WR_IV_to_WR
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12  13 14  15
	 *	0 0 0 0 0 0 0 0 1 1 V  i[0:1] f[0:1] W
	 *
	 *      [16-31] [32-47] [48-63] [64-79]
	 *      IV3	IV2	IV1	IV 0
	 *
	 * The 64-bit value contained in WR0 or WR1 (selected by W) is combined
	 * with bitwise function with an immediate value of 16,32,48 or 64 bits
	 * (left padded with zeros to 64 bits when necessary). The 64-bit result
	 * is assigned to the WR indicated by V (0 or 1). The unpadded size of
	 * the immediate value is specified by i[0:1] (values 0,1,2,3 => 16,32,
	 * 48,64 bits). The bitwise function is specified by f[0:1]
	 * (values 0,1,2 => OR,AND,XOR).
	 *
	 * OR_WR0_IMM16_TO_WR0		#iv3
	 * OR_WR0_IMM16_TO_WR1		#iv3
	 * OR_WR1_IMM16_TO_WR1		#iv3
	 * OR_WR1_IMM16_TO_WR0		#iv3
	 *
	 * OR_WR0_IMM32_TO_WR0		#iv3, #iv2
	 * OR_WR0_IMM32_TO_WR1		#iv3, #iv2
	 * OR_WR1_IMM32_TO_WR1		#iv3, #iv2
	 * OR_WR1_IMM32_TO_WR0		#iv3, #iv2
	 *
	 * OR_WR0_IMM48_TO_WR0		#iv3, #iv2, #iv1
	 * OR_WR0_IMM48_TO_WR1		#iv3, #iv2, #iv1
	 * OR_WR1_IMM48_TO_WR1		#iv3, #iv2, #iv1
	 * OR_WR1_IMM48_TO_WR0		#iv3, #iv2, #iv1
	 *
	 * OR_WR0_IMM64_TO_WR0		#iv3, #iv2, #iv1, #iv0
	 * OR_WR0_IMM64_TO_WR1		#iv3, #iv2, #iv1, #iv0
	 * OR_WR1_IMM64_TO_WR1		#iv3, #iv2, #iv1, #iv0
	 * OR_WR1_IMM64_TO_WR0		#iv3, #iv2, #iv1, #iv0
	 *
	 * Same sets for AND, XOR and XX (Not defined) operators.
	 *
	*/

	opcode = **sp_code;
	f = (uint8_t)((opcode >> 1) & 0x3);
	w = (uint8_t)(opcode & 0x1);
	i = (uint8_t)((opcode >> 3) & 0x3);
	v = (uint8_t)((opcode >> 5) & 0x1);
	sp_print_opcode_words(sp_code, i + 2);
	fsl_print("%s_WR%d_IMM%d_TO_WR%d ",
		  (f == 0) ? "OR" : (f == 1) ? "AND" : (f == 2) ? "XOR" : "XX",
		  w, 16 * (i + 1), v);
	sp_print_iv_operands(sp_code, i + 1);
	if (sp_sim.sim_enabled) {
		uint64_t	val64;

		val64 = 0;
		load_iv((uint8_t *)&val64, i, sp_code);

		switch (i) {
		default:
		case 0:
			fsl_print("\t\t IMM16 = 0x%04x\n", (uint16_t)val64);
			break;
		case 1:
			fsl_print("\t\t IMM32 = 0x%08x\n", (uint32_t)val64);
			break;
		case 2:
			fsl_print("\t\t IMM48 = 0x%04x-%08x\n",
				  (uint16_t)(val64 >> 32), (uint32_t)val64);
			break;
		case 3:
			fsl_print("\t\t IMM64 = 0x%08x-%08x\n",
				  (uint32_t)(val64 >> 32), (uint32_t)val64);
			break;
		}
		if (f == 0)
			sp_sim.wr[v] = sp_sim.wr[w] | val64;
		else if (f == 1)
			sp_sim.wr[v] = sp_sim.wr[w] & val64;
		else if (f == 2)
			sp_sim.wr[v] = sp_sim.wr[w] ^ val64;
		else
			sp_sim.wr[v] = 0;
		fsl_print("\t\t WR%d = 0x%08x-%08x\n", v,
			  (uint32_t)(sp_sim.wr[v] >> 32),
			  (uint32_t)sp_sim.wr[v]);
	}
	(*sp_code) += 2;
	sp_sim.pc += 2;
	if (i >= 1) {
		(*sp_code)++;
		sp_sim.pc++;
	}
	if (i >= 2) {
		(*sp_code)++;
		sp_sim.pc++;
	}
	if (i == 3) {
		(*sp_code)++;
		sp_sim.pc++;
	}
	ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
}

/******************************************************************************/
static void sp_bitwise_wr_wr_to_wr(uint16_t **sp_code)
{
	uint8_t		f, w;
	uint16_t	opcode;

	/* 14:Bitwise_WR_WR_to_WR
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14  15
	 *	0 0 0 0 0 0 0 0 0 1 1  1  0  f[0:1] W
	 *
	 * The 64-bit values contained in WR0 & WR1 are combined with bitwise
	 * operation and assigned to the WR indicated by W (0 or 1).
	 * The bitwise operation is defined by f[0:1] as follows:
	 *	0 : bitwise OR
	 *	1 : bitwise AND
	 *	2 : bitwise XOR
	 *	3 : ---Not Defined--- (WR0/1 is assigned to 0 as a result)
	 *
	 * OR_WR0_WR1_TO_WR0
	 * OR_WR0_WR1_TO_WR1
	 * AND_WR0_WR1_TO_WR0
	 * AND_WR0_WR1_TO_WR1
	 * XOR_WR0_WR1_TO_WR0
	 * XOR_WR0_WR1_TO_WR1
	 * XX_WR0_WR1_TO_WR0
	 * XX_WR0_WR1_TO_WR1
	*/

	opcode = **sp_code;
	f = (uint8_t)((opcode >> 1) & 0x3);
	w = (uint8_t)(opcode & 0x1);
	sp_print_opcode_words(sp_code, 1);
	fsl_print("%s_WR0_WR1_TO_WR%d;\n",
		  (f == 0) ? "OR" : (f == 1) ? "AND" :
		  (f == 2) ? "XOR" : "XX", w);
	if (sp_sim.sim_enabled) {
		if (f == 0)
			sp_sim.wr[w] = sp_sim.wr[0] | sp_sim.wr[1];
		else if (f == 1)
			sp_sim.wr[w] = sp_sim.wr[0] & sp_sim.wr[1];
		else if (f == 2)
			sp_sim.wr[w] = sp_sim.wr[0] ^ sp_sim.wr[1];
		else
			sp_sim.wr[w] = 0;
		fsl_print("\t\t WR%d = 0x%08x-%08x\n", w,
			  (uint32_t)(sp_sim.wr[w] >> 32),
			  (uint32_t)sp_sim.wr[w]);
	}
	(*sp_code)++;
	sp_sim.pc++;
	ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
}

/******************************************************************************/
static void sp_store_iv_to_ra(uint16_t **sp_code)
{
	uint8_t		s, t;
	uint16_t	opcode;

	/* 24:Store_IV_to_RA
	 *	0 1 2 3 4 5 6 7 8  9 10 11 12 13 14 15
	 *	0 0 0 0 1 0 s[0:2] t[0:6]
	 *
	 *      [16-31] [32-47] [48-63] [64-79]
	 *      IV3	IV2	IV1	IV 0
	 *              s > 1   s > 3   s > 5
	 *
	 * Stores s+1 bytes from an immediate value into the Parse Array at
	 * byte positions t-s to t.
	 *
	 * ST_IMM_BYTES_TO_RA	#to_position, #bytes, #iv3[,#iv2][,#iv1][,#iv0]
	 *
	*/

	opcode = **sp_code;
	t = (uint8_t)(opcode & 0x7f);
	s = (uint8_t)((opcode >> 7) & 0x7);
	if (t < s) {
		fsl_print("\t\t ERROR : Invalid parameter t(%d) < s(%d) !\n",
			  t, s);
		sp_sim.sp_status = SP_ERR_INVAL_PARAM;
		return;
	}
	sp_print_opcode_words(sp_code, s / 2 + 2);
	fsl_print("ST_IMM_BYTES_TO_RA to_byte:%d, bytes:%d ", t - s, s + 1);
	sp_print_iv_operands(sp_code, s / 2 + 1);
	if (sp_sim.sim_enabled) {
		uint64_t	val64;
		uint8_t		i, *pb;

		val64 = 0;
		load_iv((uint8_t *)&val64, s / 2, sp_code);
		pb = (uint8_t *)&val64;
		pb += 8 - (s + 1);
		fsl_print("\t\t RA[%d:%d] = ", t - s, t);
		for (i = 0; i < s + 1; i++, pb++) {
			fsl_print("%02x ", *pb);
			sp_sim.ra_arr[t - s + i] = *pb;
		}
		fsl_print("\n");
	}
	(*sp_code) += 2;
	sp_sim.pc += 2;
	if (s / 2 >= 1) {
		(*sp_code)++;
		sp_sim.pc++;
	}
	if (s / 2 >= 2) {
		(*sp_code)++;
		sp_sim.pc++;
	}
	if (s / 2 == 3) {
		(*sp_code)++;
		sp_sim.pc++;
	}
	ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
}

/******************************************************************************/
static void sp_load_bits_iv_to_wr(uint16_t **sp_code)
{
	uint8_t		s, n, w;
	uint16_t	opcode;

	/* 20:Load_Bits_IV_to_WR
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	 *	0 0 0 0 0 0 1 0 S n[0:5]            W
	 *
	 *      [16-31] [32-47] [48-63] [64-79]
	 *      IV3	IV2	IV1	IV 0
	 *              n > 15  n > 31  n > 47
	 *
	 * Stores the n+1 bits least significant bits of an immediate value of
	 * 16,32,48 or 64 bits to the n+1 least significant bits of WR0 or WR1
	 * (as selected by W). Since loads to the working registers are always
	 * 64-bits, the bits to the left of the n+1 bits are zeroed in the
	 * selected WR unless the shift option is specified. If the shift option
	 * is specified (S=1), the current value of the selected WR is shifted
	 * left by n+1 bits and reloaded into that position to the left of the
	 * newly loaded bits. (Logically it appears as if the n+1 bits have
	 * been shifted into or concatenated with the existing value.)
	 *
	 * LD_IMM16_BITS_TO_WR0		#bits, #iv3
	 * LDS_IMM16_BITS_TO_WR0	#bits, #iv3
	 * LD_IMM16_BITS_TO_WR1		#bits, #iv3
	 * LDS_IMM16_BITS_TO_WR1	#bits, #iv3
	 *
	 * LD_IMM32_BITS_TO_WR0		#bits, #iv3, #iv2
	 * LDS_IMM32_BITS_TO_WR0	#bits, #iv3, #iv2
	 * LD_IMM32_BITS_TO_WR1		#bits, #iv3, #iv2
	 * LDS_IMM32_BITS_TO_WR1	#bits, #iv3, #iv2
	 *
	 * LD_IMM48_BITS_TO_WR0		#bits, #iv3, #iv2, #iv1
	 * LDS_IMM48_BITS_TO_WR0	#bits, #iv3, #iv2, #iv1
	 * LD_IMM48_BITS_TO_WR1		#bits, #iv3, #iv2, #iv1
	 * LDS_IMM48_BITS_TO_WR1	#bits, #iv3, #iv2, #iv1
	 *
	 * LD_IMM64_BITS_TO_WR0		#bits, #iv3, #iv2, #iv1, #iv0
	 * LDS_IMM64_BITS_TO_WR0	#bits, #iv3, #iv2, #iv1, #iv0
	 * LD_IMM64_BITS_TO_WR1		#bits, #iv3, #iv2, #iv1, #iv0
	 * LDS_IMM64_BITS_TO_WR1	#bits, #iv3, #iv2, #iv1, #iv0
	 *
	*/

	opcode = **sp_code;
	w = (uint8_t)(opcode & 0x1);
	n = (uint8_t)((opcode >> 1) & 0x3f);
	s = (uint8_t)((opcode >> 7) & 0x1);
	sp_print_opcode_words(sp_code, n / 16 + 2);
	if (s)
		fsl_print("LDS_IMM_BITS_TO_WR%d bits:%d ", w, n + 1);
	else
		fsl_print("LD_IMM_BITS_TO_WR%d bits:%d ", w, n + 1);
	sp_print_iv_operands(sp_code, n / 16 + 1);
	if (sp_sim.sim_enabled) {
		uint64_t	val64, mask;

		val64 = 0;
		load_iv((uint8_t *)&val64, n / 16, sp_code);
		mask = (uint64_t)(~0ll) >> 64 - (n + 1);
		if (s) {
			uint64_t	tmp;

			tmp = (LLLDW_SWAP(0, sp_sim.wr[w])) << n + 1;
			sp_sim.wr[w] = LLLDW_SWAP(0, &tmp);
			if (n + 1 >= 64)
				sp_sim.wr[w] = 0;
			sp_sim.wr[w] |= val64 & mask;
		} else {
			sp_sim.wr[w] = val64 & mask;
		}
		fsl_print("\t\t WR%d = 0x%08x-%08x from IV = ",
			  w,
			  (uint32_t)(sp_sim.wr[w] >> 32),
			  (uint32_t)sp_sim.wr[w]);
		switch (n / 16) {
		default:
		case 0:
			fsl_print("0x%04x\n", (uint16_t)val64);
			break;
		case 1:
			fsl_print("0x%08x\n", (uint32_t)val64);
			break;
		case 2:
			fsl_print("0x%04x-%08x\n",
				  (uint16_t)(val64 >> 32), (uint32_t)val64);
			break;
		case 3:
			fsl_print("0x%08x-%08x\n",
				  (uint32_t)(val64 >> 32), (uint32_t)val64);
			break;
		}
	}
	(*sp_code) += 2;
	sp_sim.pc += 2;
	if (n / 16 >= 1) {
		(*sp_code)++;
		sp_sim.pc++;
	}
	if (n / 16 >= 2) {
		(*sp_code)++;
		sp_sim.pc++;
	}
	if (n / 16 == 3) {
		(*sp_code)++;
		sp_sim.pc++;
	}
	ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
}

/******************************************************************************/
static void sp_zero_wr(uint16_t **sp_code)
{
	uint8_t		w;
	uint16_t	opcode;

	/* 2:Zero_WR
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	 *	0 0 0 0 0 0 0 0 0 0 0  0  0  1  0  W
	 *
	 * Zeros (clears) Working Register 0 or 1
	 *
	 * ZERO_WR0
	 * ZERO_WR1
	*/

	opcode = **sp_code;
	w = (uint8_t)(opcode & 0x1);
	sp_print_opcode_words(sp_code, 1);
	fsl_print("ZERO_WR%d;\n", w);
	if (sp_sim.sim_enabled) {
		sp_sim.wr[w] = 0;
		fsl_print("\t\t WR%d = 0x%08x-%08x\n", w,
			  (uint32_t)(sp_sim.wr[w] >> 32),
			  (uint32_t)sp_sim.wr[w]);
	}
	(*sp_code)++;
	sp_sim.pc++;
	ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
}

/******************************************************************************/
static void sp_add_sv_to_wo(uint16_t **sp_code)
{
	uint16_t	opcode;
	uint8_t		v;

	/* 23:Add_SV_to_WO
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	 *	0 0 0 0 0 1 1 1 v[0:7]
	 *
	 * Adds a specified value (v[0:7]) to the Window Offset register.
	 * Effectively moves the window to the right.
	 *
	 * ADD_SV_TO_WO		#short_val
	*/

	opcode = **sp_code;
	v = (uint8_t)(opcode & 0xff);
	sp_print_opcode_words(sp_code, 1);
	fsl_print("ADD_SV_TO_WO sv:%d;\n", v);
	if (sp_sim.sim_enabled) {
		sp_sim.wo += v;
		fsl_print("\t\t WO = %d\n", sp_sim.wo);
	}
	(*sp_code)++;
	sp_sim.pc++;
	ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
}

/******************************************************************************/
static void sp_load_sv_to_wo(uint16_t **sp_code)
{
	uint8_t		v;
	uint16_t	opcode;

	/* 22:Load_SV_to_WO
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	 *	0 0 0 0 0 1 1 0 v[0:7]
	 *
	 * Load a specified value (s[0:7]) into the Window Offset register.
	 *
	 * LD_SV_TO_WO		#short_val
	*/

	opcode = **sp_code;
	v = (uint8_t)(opcode & 0xff);
	sp_print_opcode_words(sp_code, 1);
	fsl_print("LD_SV_TO_WO sv:%d;\n", v);
	if (sp_sim.sim_enabled) {
		sp_sim.wo = v;
		fsl_print("\t\t WO = %d\n", sp_sim.wo);
	}
	(*sp_code)++;
	sp_sim.pc++;
	ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
}

/******************************************************************************/
static void sp_ones_comp_wr1_to_wr0(uint16_t **sp_code)
{
	/* 3:Ones_Comp_WR1_to_WR0
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	 *	0 0 0 0 0 0 0 0 0 0 0  0  0  1  1  0
	 *
	 * Used to 1's complement checksums (that is, the verify checksum
	 * algorithm used by TCP,UDP,IP).
	 * Performs 16-bit 1's complement addition of the four 16-bit values
	 * within WR1 and the least significant 16-bits of WR0 and places the
	 * result in the least significant 16-bits of WR0 (same position).
	 *
	 * WR0(6,7) <= WR0(6,7)+WR1(0,1)+WR1(2,3)+WR1(4,5)+WR1(6,7)+carry
	 *
	 * To verify a checksum over frame or header data, load up to 8 bytes
	 * into WR1 (1 cycle) then execute this instruction (1 cycle) and
	 * repeat as necessary. Once the entire header or frame has been
	 * included, the result in WR0(6,7) should = 0xFFFF if the checksum is
	 * valid. (This comparison can be accomplished with a Compare_WR_to_IV
	 * instruction).
	 *
	 * ONES_CMP_WR1_TO_WR0
	*/

	sp_print_opcode_words(sp_code, 1);
	fsl_print("ONES_CMP_WR1_TO_WR0;\n");

	if (sp_sim.sim_enabled) {
		uint32_t	sum;
		uint16_t	*pw16;

		sum = (uint16_t)sp_sim.wr[0];
		pw16 = (uint16_t *)&sp_sim.wr[1];

		sum += *pw16++;
		sum += *pw16++;
		sum += *pw16++;
		sum += *pw16;
		while (sum >> 16)
			sum = (sum & 0xffff) + (sum >> 16);
		sp_sim.wr[0] &= 0xffffffffffff0000ll;
		sp_sim.wr[0] |= (uint16_t)sum;

		fsl_print("\t\t WR0 = 0x%08x-%08x\n",
			  (uint32_t)(sp_sim.wr[0] >> 32),
			  (uint32_t)sp_sim.wr[0]);
	}
	(*sp_code)++;
	sp_sim.pc++;
	ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
}

/******************************************************************************/
static void sp_shift_left_wr_by_sv(uint16_t **sp_code)
{
	uint16_t	opcode;
	uint8_t		n, w;

	/* 18:Shift_Left_WR_by_SV
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	 *	0 0 0 0 0 0 0 1 0 n[0:5]           W
	 *
	 * Shifts the Working Register (specified by W) to the left by 1 to 64
	 * bits (as specified by n[0:5]+1). Zeros are shifted into the 64-n
	 * least significant bits.
	 *
	 * SHL_WR0_BY_SV	#short_val
	 * SHL_WR1_BY_SV	#short_val
	*/

	opcode = **sp_code;
	w = (uint8_t)(opcode & 0x1);
	n = (uint8_t)((opcode >> 1) & 0x3f);
	sp_print_opcode_words(sp_code, 1);
	fsl_print("SHL_WR%d_BY_SV sv:%d;\n", w, n + 1);
	if (sp_sim.sim_enabled) {
		sp_sim.wr[w] = sp_sim.wr[w] << (n + 1);
		fsl_print("\t\t WR%d = 0x%08x-%08x\n", w,
			  (uint32_t)(sp_sim.wr[w] >> 32),
			  (uint32_t)sp_sim.wr[w]);
	}
	(*sp_code)++;
	sp_sim.pc++;
	ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
}

/******************************************************************************/
static void sp_shift_right_wr_by_sv(uint16_t **sp_code)
{
	uint16_t	opcode;
	uint8_t		n, w;

	/* 19:Shift_Right_WR_by_SV
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	 *	0 0 0 0 0 0 0 1 1 n[0:5]           W
	 *
	 * Shifts the Working Register (specified by W) to the right by 1 to 64
	 * bits (as specified by n[0:5]+1). Zeros are shifted into the 64-n most
	 * significant bits.
	 *
	 * SHR_WR0_BY_SV	#short_val
	 * SHR_WR1_BY_SV	#short_val
	*/

	opcode = **sp_code;
	w = (uint8_t)(opcode & 0x1);
	n = (uint8_t)((opcode >> 1) & 0x3f);
	sp_print_opcode_words(sp_code, 1);
	fsl_print("SHR_WR%d_BY_SV sv:%d;\n", w, n + 1);
	if (sp_sim.sim_enabled) {
		sp_sim.wr[w] = sp_sim.wr[w] >> (n + 1);
		fsl_print("\t\t WR%d = 0x%08x-%08x\n", w,
			  (uint32_t)(sp_sim.wr[w] >> 32),
			  (uint32_t)sp_sim.wr[w]);
	}
	(*sp_code)++;
	sp_sim.pc++;
	ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
}

/******************************************************************************/
static void sp_case1_dj_wr_to_wr(uint16_t **sp_code)
{
	uint16_t	opcode, jmp_dest3, jmp_dest2;
	uint8_t		a2, a3, g3, l3, g2, l2;

	/* 5: CASE1_DJ_WR_to_WR
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	 *	0 0 0 0 0 0 0 0 0 0 0  0  1  0  A2 A3
	 *
	 *	[16-31] : G[0]L[1]JumpDest3[5:15]
	 *	[32-47] : G[0]L[1]JumpDest2[5:15]
	 *
	 * Case structure equality comparison least significant 16-bits of WR0
	 * to the rightmost 16-bit values in WR1. Jumps to the destination
	 * corresponding to the first match. If no match default is to jump.
	 *
	 * If A3 or A2 is set then the respective jumps are treated as an
	 * advancement to a new hard or soft HXS (the HB is updated to HB+WO,
	 * WO and WRs are reset).
	 *
	 * The hard coded jump destinations are specified in the Jump
	 * instruction.
	 *
	 * G: The top most bit of the jump destination is labeled G. When set
	 * the jump is treated as a gosub.
	 * L: The next bit(1) is labeled L. When set the jump destination is
	 * treated as relative otherwise it is absolute.
	 *	case of (WR0(48..63) = ) then
	 *		= WR1(48..63):	jump to destination_3
	 *		default :	jump to destination_2
	 *	endcase
	 *
	 * CASE1_DJ_WR_to_WR	G|L|S|JmpDest3, G|L|S|JmpDest2
	 *
	 * Notes :
	 *	1.  The G(gosub), L(relative addressing), and S(sign bit) flags
	 *	may be used only on the DPAA2 version
	 *	2. JmpDest is on 11 bits (10:0) on DPAA2 and on 10 bits (9:0)
	 *	on DPAA2 */

	opcode = **sp_code;
	jmp_dest3 = *(*sp_code + 1);
	jmp_dest2 = *(*sp_code + 2);
	a3 = (uint8_t)(opcode & 0x1);
	a2 = (uint8_t)((opcode >> 1) & 0x1);
#if (SP_DPAA_VERSION == 1)
	l3 = 0;
	g3 = 0;
	l2 = 0;
	g2 = 0;
#else
	l3 = (uint8_t)((jmp_dest3 >> 14) & 0x1);
	g3 = (uint8_t)((jmp_dest3 >> 15) & 0x1);
	l2 = (uint8_t)((jmp_dest2 >> 14) & 0x1);
	g2 = (uint8_t)((jmp_dest2 >> 15) & 0x1);
#endif
	sp_print_opcode_words(sp_code, 3);
	fsl_print("CASE1_DJ_WR_to_WR\n");
	sp_print_case_dest("WR0[w0] == WR1[w0] :", jmp_dest3, a3, g3, l3);
	sp_print_case_dest("DEFAULT JUMP (DJ)  :", jmp_dest2, a2, g2, l2);
	if (sp_check_destination_address(jmp_dest3) ||
	    sp_check_destination_address(jmp_dest2))
		return;
	jmp_dest3 &= DEST_ADDR_MASK;
	jmp_dest2 &= DEST_ADDR_MASK;
	if (sp_sim.sim_enabled) {
		if ((uint16_t)sp_sim.wr[0] == (uint16_t)sp_sim.wr[1]) {
			fsl_print("\t\t WR0[48:63](%04x) == WR1[48:63](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)sp_sim.wr[1]);
			set_jmp_gosub_destination(sp_code, jmp_dest3,
						  a3, g3, l3, 3);
		} else {
			fsl_print("\t\t WR0[48:63](%04x) != WR1[48:63](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)sp_sim.wr[1]);
			set_jmp_gosub_destination(sp_code, jmp_dest2,
						  a2, g2, l2, 3);
		}
	} else {
		(*sp_code) += 3;
		sp_sim.pc += 3;
		ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
	}
}

/******************************************************************************/
static void sp_case2_dc_wr_to_wr(uint16_t **sp_code)
{
	uint16_t	opcode, jmp_dest3, jmp_dest2;
	uint8_t		a2, a3, g3, l3, g2, l2;

	/* 6:CASE2_DC_WR_to_WR
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	 *	0 0 0 0 0 0 0 0 0 0 0  0  1  1  A2 A3
	 *
	 *	[16-31] : G[0]L[1]JumpDest3[5:15]
	 *	[32-47] : G[0]L[1]JumpDest2[5:15]
	 *
	 * Case structure equality comparison least significant 16-bits of WR0
	 * to each of the two right most 16-bit values in WR1. Jumps to the
	 * destination corresponding to the first match. If no match default is
	 * to continue. If A3 or A2 is set then the respective jumps are treated
	 * as an advancement to a new HXS (the HB is updated to HB+WO, WO and
	 * WRs are reset). The hard coded jump destinations are specified in the
	 * Jump instruction.
	 * G: The top most bit of the jump destination is labeled G. When set
	 * the jump is treated as a gosub.
	 * L: The next bit(1) is labeled L. When set the jump destination is
	 * treated as relative otherwise it is absolute.
	 *	case of (WR0(48..63) = ) then
	 *		= WR1(48..63) : jump to destination_3
	 *		= WR1(32..47) : jump to destination_2
	 *		default : continue
	 *	endcase
	 *
	 * CASE2_DC_WR_to_WR	G|L|S|JmpDest3, G|L|S|JmpDest2
	 *
	 * Notes :
	 *	1.  The G(gosub), L(relative addressing), and S(sign bit) flags
	 *	may be used only on the DPAA2 version
	 *	2. JmpDest is on 11 bits (10:0) on DPAA2 and on 10 bits (9:0)
	 *	on DPAA2 */

	opcode = **sp_code;
	jmp_dest3 = *(*sp_code + 1);
	jmp_dest2 = *(*sp_code + 2);
	a3 = (uint8_t)(opcode & 0x1);
	a2 = (uint8_t)((opcode >> 1) & 0x1);
#if (SP_DPAA_VERSION == 1)
	l3 = 0;
	g3 = 0;
	l2 = 0;
	g2 = 0;
#else
	l3 = (uint8_t)((jmp_dest3 >> 14) & 0x1);
	g3 = (uint8_t)((jmp_dest3 >> 15) & 0x1);
	l2 = (uint8_t)((jmp_dest2 >> 14) & 0x1);
	g2 = (uint8_t)((jmp_dest2 >> 15) & 0x1);
#endif
	sp_print_opcode_words(sp_code, 3);
	fsl_print("CASE2_DC_WR_to_WR\n");
	sp_print_case_dest("WR0[w0] == WR1[w0] :", jmp_dest3, a3, g3, l3);
	sp_print_case_dest("WR0[w0] == WR1[w1] :", jmp_dest2, a2, g2, l2);
	sp_print_case_dest_continue();
	if (sp_check_destination_address(jmp_dest3) ||
	    sp_check_destination_address(jmp_dest2))
		return;
	jmp_dest3 &= DEST_ADDR_MASK;
	jmp_dest2 &= DEST_ADDR_MASK;
	if (sp_sim.sim_enabled) {
		if ((uint16_t)sp_sim.wr[0] == (uint16_t)sp_sim.wr[1]) {
			fsl_print("\t\t WR0[48:63](%04x) == WR1[48:63](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)sp_sim.wr[1]);
			set_jmp_gosub_destination(sp_code, jmp_dest3,
						  a3, g3, l3, 3);
		} else if ((uint16_t)sp_sim.wr[0] ==
			   (uint16_t)(sp_sim.wr[1] >> 16)) {
			fsl_print("\t\t WR0[48:63](%04x) == WR1[32:47](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)(sp_sim.wr[1] >> 16));
			set_jmp_gosub_destination(sp_code, jmp_dest2,
						  a2, g2, l2, 3);
		} else {
			(*sp_code) += 3;
			sp_sim.pc += 3;
			ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
			fsl_print("\t\t WR0[48:63](%04x) != WR1[48:63](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)sp_sim.wr[1]);
			fsl_print("\t\t WR0[48:63](%04x) != WR1[32:47](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)(sp_sim.wr[1] >> 16));
			fsl_print("\t\t Continue with the next PC = 0x%03x\n",
				  sp_sim.pc);
		}
	} else {
		(*sp_code) += 3;
		sp_sim.pc += 3;
		ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
	}
}

/******************************************************************************/
static void sp_case2_dj_wr_to_wr(uint16_t **sp_code)
{
	uint16_t	opcode, jmp_dest3, jmp_dest2, jmp_dest1;
	uint8_t		a3, a2, a1, g3, g2, g1, l3, l2, l1;

	/* 7:CASE2_DJ_WR_to_WR
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	 *	0 0 0 0 0 0 0 0 0 0 0  1  0  A1 A2 A3
	 *
	 *	[16-31] : G[0]L[1]JumpDest3[5:15]
	 *	[32-47] : G[0]L[1]JumpDest2[5:15]
	 *	[48-63] : G[0]L[1]JumpDest1[5:15]
	 *
	 * Case structure equality comparison least significant 16-bits of WR0
	 * to each of the two right most 16-bit values in WR1. Jumps to the
	 * destination corresponding to the first match. If no match default is
	 * to jump. If A3, A2 or A1 is set then the corresponding jumps are
	 * treated as an advancement to a new HXS (the HB is updated to HB+WO,
	 * WO and WRs are reset). The hard coded jump destinations are specified
	 * in the Jump instruction.
	 *
	 * G: The top most bit of the jump destination is labeled G. When set
	 * the jump is treated as a gosub.
	 * L: The next bit(1) is labeled L. When set the jump destination is
	 * treated as relative otherwise it is absolute.
	 *	case of (WR0(48..63) = ) then
	 *		= WR1(48..63):	jump to destination_3
	 *		= WR1(32..47):	jump to destination_2
	 *		default :	jump to destination_1
	 *	endcase
	 *
	 * CASE2_DJ_WR_to_WR	G|L|S|JmpDest3, G|L|S|JmpDest2,
	 *			G|L|S|JmpDest1
	 *
	 * Notes :
	 *	1.  The G(gosub), L(relative addressing), and S(sign bit) flags
	 *	may be used only on the DPAA2 version
	 *	2. JmpDest is on 11 bits (10:0) on DPAA2 and on 10 bits (9:0)
	 *	on DPAA2 */

	opcode = **sp_code;
	jmp_dest3 = *(*sp_code + 1);
	jmp_dest2 = *(*sp_code + 2);
	jmp_dest1 = *(*sp_code + 3);
	a3 = (uint8_t)(opcode & 0x1);
	a2 = (uint8_t)((opcode >> 1) & 0x1);
	a1 = (uint8_t)((opcode >> 2) & 0x1);
#if (SP_DPAA_VERSION == 1)
	l3 = 0;
	g3 = 0;
	l2 = 0;
	g2 = 0;
	l1 = 0;
	g1 = 0;
#else
	l3 = (uint8_t)((jmp_dest3 >> 14) & 0x1);
	g3 = (uint8_t)((jmp_dest3 >> 15) & 0x1);
	l2 = (uint8_t)((jmp_dest2 >> 14) & 0x1);
	g2 = (uint8_t)((jmp_dest2 >> 15) & 0x1);
	l1 = (uint8_t)((jmp_dest1 >> 14) & 0x1);
	g1 = (uint8_t)((jmp_dest1 >> 15) & 0x1);
#endif
	sp_print_opcode_words(sp_code, 4);
	fsl_print("CASE2_DJ_WR_to_WR\n");
	sp_print_case_dest("WR0[w0] == WR1[w0] :", jmp_dest3, a3, g3, l3);
	sp_print_case_dest("WR0[w0] == WR1[w1] :", jmp_dest2, a2, g2, l2);
	sp_print_case_dest("DEFAULT JUMP (DJ)  :", jmp_dest1, a1, g1, l1);
	if (sp_check_destination_address(jmp_dest3) ||
	    sp_check_destination_address(jmp_dest2) ||
	    sp_check_destination_address(jmp_dest1))
		return;
	jmp_dest3 &= DEST_ADDR_MASK;
	jmp_dest2 &= DEST_ADDR_MASK;
	jmp_dest1 &= DEST_ADDR_MASK;
	if (sp_sim.sim_enabled) {
		if ((uint16_t)sp_sim.wr[0] == (uint16_t)sp_sim.wr[1]) {
			fsl_print("\t\t WR0[48:63](%04x) == WR1[48:63](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)sp_sim.wr[1]);
			set_jmp_gosub_destination(sp_code, jmp_dest3,
						  a3, g3, l3, 4);
		} else if ((uint16_t)sp_sim.wr[0] ==
			   (uint16_t)(sp_sim.wr[1] >> 16)) {
			fsl_print("\t\t WR0[48:63](%04x) == WR1[32:47](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)(sp_sim.wr[1] >> 16));
			set_jmp_gosub_destination(sp_code, jmp_dest2,
						  a2, g2, l2, 4);
		} else {
			fsl_print("\t\t WR0[48:63](%04x) != WR1[48:63](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)sp_sim.wr[1]);
			fsl_print("\t\t WR0[48:63](%04x) != WR1[32:47](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)(sp_sim.wr[1] >> 16));
			set_jmp_gosub_destination(sp_code, jmp_dest1,
						  a1, g1, l1, 4);
		}
	} else {
		(*sp_code) += 4;
		sp_sim.pc += 4;
		ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
	}
}

/******************************************************************************/
static void sp_case3_dc_wr_to_wr(uint16_t **sp_code)
{
	uint16_t	opcode, jmp_dest3, jmp_dest2, jmp_dest1;
	uint8_t		a3, a2, a1, g3, g2, g1, l3, l2, l1;

	/* 8:CASE3_DC_WR_to_WR
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	 *	0 0 0 0 0 0 0 0 0 0 0  1  1  A1 A2 A3
	 *
	 *	[16-31] : G[0]L[1]JumpDest3[5:15]
	 *	[32-47] : G[0]L[1]JumpDest2[5:15]
	 *	[48-63] : G[0]L[1]JumpDest1[5:15]
	 *
	 * Case structure equality comparison least significant 16-bits of WR0
	 * to each of the three right most 16-bit values in WR1. Jumps to the
	 * destination corresponding to the first match. If no match default is
	 * to continue. If A3,A2 or A1 is set then the corresponding jumps are
	 * treated as an advancement to a new HXS (the HB is updated to HB+WO,
	 * WO and WRs are reset). The hard coded jump destinations are specified
	 * in the Jump instruction.
	 *
	 * G: The top most bit of the jump destination is labeled G. When set
	 * the jump is treated as a gosub.
	 * L: The next bit(1) is labeled L. When set the jump destination is
	 * treated as relative otherwise it is absolute.
	 *	case of (WR0(48..63) = ) then
	 *		= WR1(48..63):	jump to destination_3
	 *		= WR1(32..47):	jump to destination_2
	 *		= WR1(16..31):	jump to destination_1
	 *		default :	continue;
	 *	endcase
	 *
	 * CASE3_DC_WR_to_WR	G|L|S|JmpDest3, G|L|S|JmpDest2,
	 *			G|L|S|JmpDest1
	 *
	 * Notes :
	 *	1.  The G(gosub), L(relative addressing), and S(sign bit) flags
	 *	may be used only on the DPAA2 version
	 *	2. JmpDest is on 11 bits (10:0) on DPAA2 and on 10 bits (9:0)
	 *	on DPAA2 */

	opcode = **sp_code;
	jmp_dest3 = *(*sp_code + 1);
	jmp_dest2 = *(*sp_code + 2);
	jmp_dest1 = *(*sp_code + 3);
	a3 = (uint8_t)(opcode & 0x1);
	a2 = (uint8_t)((opcode >> 1) & 0x1);
	a1 = (uint8_t)((opcode >> 2) & 0x1);
#if (SP_DPAA_VERSION == 1)
	l3 = 0;
	g3 = 0;
	l2 = 0;
	g2 = 0;
	l1 = 0;
	g1 = 0;
#else
	l3 = (uint8_t)((jmp_dest3 >> 14) & 0x1);
	g3 = (uint8_t)((jmp_dest3 >> 15) & 0x1);
	l2 = (uint8_t)((jmp_dest2 >> 14) & 0x1);
	g2 = (uint8_t)((jmp_dest2 >> 15) & 0x1);
	l1 = (uint8_t)((jmp_dest1 >> 14) & 0x1);
	g1 = (uint8_t)((jmp_dest1 >> 15) & 0x1);
#endif
	sp_print_opcode_words(sp_code, 4);
	fsl_print("CASE3_DC_WR_to_WR\n");
	sp_print_case_dest("WR0[w0] == WR1[w0] :", jmp_dest3, a3, g3, l3);
	sp_print_case_dest("WR0[w0] == WR1[w1] :", jmp_dest2, a2, g2, l2);
	sp_print_case_dest("WR0[w0] == WR1[w2] :", jmp_dest1, a1, g1, l1);
	sp_print_case_dest_continue();
	if (sp_check_destination_address(jmp_dest3) ||
	    sp_check_destination_address(jmp_dest2) ||
	    sp_check_destination_address(jmp_dest1))
		return;
	jmp_dest3 &= DEST_ADDR_MASK;
	jmp_dest2 &= DEST_ADDR_MASK;
	jmp_dest1 &= DEST_ADDR_MASK;
	if (sp_sim.sim_enabled) {
		if ((uint16_t)sp_sim.wr[0] == (uint16_t)sp_sim.wr[1]) {
			fsl_print("\t\t WR0[48:63](%04x) == WR1[48:63](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)sp_sim.wr[1]);
			set_jmp_gosub_destination(sp_code, jmp_dest3,
						  a3, g3, l3, 4);
		} else if ((uint16_t)sp_sim.wr[0] ==
			   (uint16_t)(sp_sim.wr[1] >> 16)) {
			fsl_print("\t\t WR0[48:63](%04x) == WR1[32:47](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)(sp_sim.wr[1] >> 16));
			set_jmp_gosub_destination(sp_code, jmp_dest2,
						  a2, g2, l2, 4);
		} else if ((uint16_t)sp_sim.wr[0] ==
			   (uint16_t)(sp_sim.wr[1] >> 32)) {
			fsl_print("\t\t WR0[48:63](%04x) == WR1[16:31](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)(sp_sim.wr[1] >> 32));
			set_jmp_gosub_destination(sp_code, jmp_dest2,
						  a1, g1, l1, 4);
		} else {
			fsl_print("\t\t WR0[48:63](%04x) != WR1[48:63](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)sp_sim.wr[1]);
			fsl_print("\t\t WR0[48:63](%04x) != WR1[32:47](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)(sp_sim.wr[1] >> 16));
			fsl_print("\t\t WR0[48:63](%04x) != WR1[16:31](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)(sp_sim.wr[1] >> 32));
			(*sp_code) += 4;
			sp_sim.pc += 4;
			ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
			fsl_print("\t\t Continue with the next PC = 0x%03x\n",
				  sp_sim.pc);
		}
	} else {
		(*sp_code) += 4;
		sp_sim.pc += 4;
		ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
	}
}

/******************************************************************************/
static void sp_case3_dj_wr_to_wr(uint16_t **sp_code)
{
	uint16_t	opcode, jmp_dest3, jmp_dest2, jmp_dest1, jmp_dest0;
	uint8_t		a3, a2, a1, a0, g3, g2, g1, g0, l3, l2, l1, l0;

	/* 9:CASE3_DJ_WR_to_WR
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	 *	0 0 0 0 0 0 0 0 0 0 1  0  A0 A1 A2 A3
	 *
	 *	[16-31] : G[0]L[1]JumpDest3[5:15]
	 *	[32-47] : G[0]L[1]JumpDest2[5:15]
	 *	[48-63] : G[0]L[1]JumpDest1[5:15]
	 *	[64-79] : G[0]L[1]JumpDest0[5:15]
	 *
	 * Case structure equality comparison least significant 16-bits of WR0
	 * to each of the three right most 16-bit values in WR1. Jumps to the
	 * destination corresponding to the first match. If no match default is
	 * to jump. If A3,A2,A1 or A0 is set then the corresponding jumps are
	 * treated as an advancement to a new HXS (the HB is updated to HB+WO,
	 * WO and WRs are reset). The hard coded jump destinations are specified
	 * in the Jump instruction.
	 *
	 * G: The top most bit of the jump destination is labeled G. When set
	 * the jump is treated as a gosub.
	 * L: The next bit(1) is labeled L. When set the jump destination is
	 * treated as relative otherwise it is absolute.
	 *	case of (WR0(48..63) = ) then
	 *		= WR1(48..63):	jump to destination_3
	 *		= WR1(32..47):	jump to destination_2
	 *		= WR1(16..31):	jump to destination_1
	 *		default :	jump to destination_0
	 *	endcase
	 *
	 * CASE3_DJ_WR_to_WR	G|L|S|JmpDest3, G|L|S|JmpDest2,
	 *			G|L|S|JmpDest1, G|L|S|JmpDest0
	 *
	 * Notes :
	 *	1.  The G(gosub), L(relative addressing), and S(sign bit) flags
	 *	may be used only on the DPAA2 version
	 *	2. JmpDest is on 11 bits (10:0) on DPAA2 and on 10 bits (9:0)
	 *	on DPAA2 */

	opcode = **sp_code;
	jmp_dest3 = *(*sp_code + 1);
	jmp_dest2 = *(*sp_code + 2);
	jmp_dest1 = *(*sp_code + 3);
	jmp_dest0 = *(*sp_code + 4);
	a3 = (uint8_t)(opcode & 0x1);
	a2 = (uint8_t)((opcode >> 1) & 0x1);
	a1 = (uint8_t)((opcode >> 2) & 0x1);
	a0 = (uint8_t)((opcode >> 3) & 0x1);
#if (SP_DPAA_VERSION == 1)
	l3 = 0;
	g3 = 0;
	l2 = 0;
	g2 = 0;
	l1 = 0;
	g1 = 0;
	l0 = 0;
	g0 = 0;
#else
	l3 = (uint8_t)((jmp_dest3 >> 14) & 0x1);
	g3 = (uint8_t)((jmp_dest3 >> 15) & 0x1);
	l2 = (uint8_t)((jmp_dest2 >> 14) & 0x1);
	g2 = (uint8_t)((jmp_dest2 >> 15) & 0x1);
	l1 = (uint8_t)((jmp_dest1 >> 14) & 0x1);
	g1 = (uint8_t)((jmp_dest1 >> 15) & 0x1);
	l0 = (uint8_t)((jmp_dest0 >> 14) & 0x1);
	g0 = (uint8_t)((jmp_dest0 >> 15) & 0x1);
#endif
	sp_print_opcode_words(sp_code, 5);
	fsl_print("CASE3_DJ_WR_to_WR\n");
	sp_print_case_dest("WR0[w0] == WR1[w0] :", jmp_dest3, a3, g3, l3);
	sp_print_case_dest("WR0[w0] == WR1[w1] :", jmp_dest2, a2, g2, l2);
	sp_print_case_dest("WR0[w0] == WR1[w2] :", jmp_dest1, a1, g1, l1);
	sp_print_case_dest("DEFAULT JUMP (DJ)  :", jmp_dest0, a0, g0, l0);
	if (sp_check_destination_address(jmp_dest3) ||
	    sp_check_destination_address(jmp_dest2) ||
	    sp_check_destination_address(jmp_dest1) ||
	    sp_check_destination_address(jmp_dest0))
		return;
	jmp_dest3 &= DEST_ADDR_MASK;
	jmp_dest2 &= DEST_ADDR_MASK;
	jmp_dest1 &= DEST_ADDR_MASK;
	jmp_dest0 &= DEST_ADDR_MASK;
	if (sp_sim.sim_enabled) {
		if ((uint16_t)sp_sim.wr[0] == (uint16_t)sp_sim.wr[1]) {
			fsl_print("\t\t WR0[48:63](%04x) == WR1[48:63](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)sp_sim.wr[1]);
			set_jmp_gosub_destination(sp_code, jmp_dest3,
						  a3, g3, l3, 5);
		} else if ((uint16_t)sp_sim.wr[0] ==
			   (uint16_t)(sp_sim.wr[1] >> 16)) {
			fsl_print("\t\t WR0[48:63](%04x) == WR1[32:47](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)(sp_sim.wr[1] >> 16));
			set_jmp_gosub_destination(sp_code, jmp_dest2,
						  a2, g2, l2, 5);
		} else if ((uint16_t)sp_sim.wr[0] ==
			   (uint16_t)(sp_sim.wr[1] >> 32)) {
			fsl_print("\t\t WR0[48:63](%04x) == WR1[16:31](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)(sp_sim.wr[1] >> 32));
			set_jmp_gosub_destination(sp_code, jmp_dest1,
						  a1, g1, l1, 5);
		} else {
			fsl_print("\t\t WR0[48:63](%04x) != WR1[48:63](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)sp_sim.wr[1]);
			fsl_print("\t\t WR0[48:63](%04x) != WR1[32:47](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)(sp_sim.wr[1] >> 16));
			fsl_print("\t\t WR0[48:63](%04x) != WR1[16:31](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)(sp_sim.wr[1] >> 32));
			set_jmp_gosub_destination(sp_code, jmp_dest0,
						  a0, g0, l0, 5);
		}
	} else {
		(*sp_code) += 5;
		sp_sim.pc += 5;
		ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
	}
}

/******************************************************************************/
static void sp_case4_dc_wr_to_wr(uint16_t **sp_code)
{
	uint16_t	opcode, jmp_dest3, jmp_dest2, jmp_dest1, jmp_dest0;
	uint8_t		a3, a2, a1, a0, g3, g2, g1, g0, l3, l2, l1, l0;

	/* 10:CASE4_DC_WR_to_WR
	 *	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	 *	0 0 0 0 0 0 0 0 0 0 1  1  A0 A1 A2 A3
	 *
	 *	[16-31] : G[0]L[1]JumpDest3[5:15]
	 *	[32-47] : G[0]L[1]JumpDest2[5:15]
	 *	[48-63] : G[0]L[1]JumpDest1[5:15]
	 *	[64-79] : G[0]L[1]JumpDest0[5:15]
	 *
	 * Case structure equality comparison least significant 16-bits of WR0
	 * to each of the four right most 16-bit values in WR1. Jumps to the
	 * destination corresponding to the first match. If no match default is
	 * to continue. If A3,A2,A1 or A0 is set then the corresponding jumps
	 * are treated as an advancement to a new HXS (the HB is updated to
	 * HB+WO, WO and WRs are reset). The hard coded jump destinations are
	 * specified in the Jump instruction.
	 *
	 * G: The top most bit of the jump destination is labeled G. When set
	 * the jump is treated as a gosub.
	 * L: The next bit(1) is labeled L. When set the jump destination is
	 * treated as relative otherwise it is absolute.
	 *	case of (WR0(48..63) = ) then
	 *		= WR1(48..63):	jump to destination_3
	 *		= WR1(32..47):	jump to destination_2
	 *		= WR1(16..31):	jump to destination_1
	 *		= WR1(0..15):	jump to destination_0
	 *		default :	continue
	 *	endcase
	 *
	 * CASE4_DC_WR_to_WR	G|L|S|JmpDest3, G|L|S|JmpDest2,
	 *			G|L|S|JmpDest1, G|L|S|JmpDest0
	 *
	 * Notes :
	 *	1.  The G(gosub), L(relative addressing), and S(sign bit) flags
	 *	may be used only on the DPAA2 version
	 *	2. JmpDest is on 11 bits (10:0) on DPAA2 and on 10 bits (9:0)
	 *	on DPAA2 */

	opcode = **sp_code;
	jmp_dest3 = *(*sp_code + 1);
	jmp_dest2 = *(*sp_code + 2);
	jmp_dest1 = *(*sp_code + 3);
	jmp_dest0 = *(*sp_code + 4);
	a3 = (uint8_t)(opcode & 0x1);
	a2 = (uint8_t)((opcode >> 1) & 0x1);
	a1 = (uint8_t)((opcode >> 2) & 0x1);
	a0 = (uint8_t)((opcode >> 3) & 0x1);
#if (SP_DPAA_VERSION == 1)
	l3 = 0;
	g3 = 0;
	l2 = 0;
	g2 = 0;
	l1 = 0;
	g1 = 0;
	l0 = 0;
	g0 = 0;
#else
	l3 = (uint8_t)((jmp_dest3 >> 14) & 0x1);
	g3 = (uint8_t)((jmp_dest3 >> 15) & 0x1);
	l2 = (uint8_t)((jmp_dest2 >> 14) & 0x1);
	g2 = (uint8_t)((jmp_dest2 >> 15) & 0x1);
	l1 = (uint8_t)((jmp_dest1 >> 14) & 0x1);
	g1 = (uint8_t)((jmp_dest1 >> 15) & 0x1);
	l0 = (uint8_t)((jmp_dest0 >> 14) & 0x1);
	g0 = (uint8_t)((jmp_dest0 >> 15) & 0x1);
#endif
	sp_print_opcode_words(sp_code, 5);
	fsl_print("CASE4_DC_WR_to_WR\n");
	sp_print_case_dest("WR0[w0] == WR1[w0] :", jmp_dest3, a3, g3, l3);
	sp_print_case_dest("WR0[w0] == WR1[w1] :", jmp_dest2, a2, g2, l2);
	sp_print_case_dest("WR0[w0] == WR1[w2] :", jmp_dest1, a1, g1, l1);
	sp_print_case_dest("WR0[w0] == WR1[w3] :", jmp_dest0, a0, g0, l0);
	sp_print_case_dest_continue();
	if (sp_check_destination_address(jmp_dest3) ||
	    sp_check_destination_address(jmp_dest2) ||
	    sp_check_destination_address(jmp_dest1) ||
	    sp_check_destination_address(jmp_dest0))
		return;
	jmp_dest3 &= DEST_ADDR_MASK;
	jmp_dest2 &= DEST_ADDR_MASK;
	jmp_dest1 &= DEST_ADDR_MASK;
	jmp_dest0 &= DEST_ADDR_MASK;
	if (sp_sim.sim_enabled) {
		if ((uint16_t)sp_sim.wr[0] == (uint16_t)sp_sim.wr[1]) {
			fsl_print("\t\t WR0[48:63](%04x) == WR1[48:63](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)sp_sim.wr[1]);
			set_jmp_gosub_destination(sp_code, jmp_dest3,
						  a3, g3, l3, 5);
		} else if ((uint16_t)sp_sim.wr[0] ==
			   (uint16_t)(sp_sim.wr[1] >> 16)) {
			fsl_print("\t\t WR0[48:63](%04x) == WR1[32:47](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)(sp_sim.wr[1] >> 16));
			set_jmp_gosub_destination(sp_code, jmp_dest2,
						  a2, g2, l2, 5);
		} else if ((uint16_t)sp_sim.wr[0] ==
			   (uint16_t)(sp_sim.wr[1] >> 32)) {
			fsl_print("\t\t WR0[48:63](%04x) == WR1[16:31](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)(sp_sim.wr[1] >> 32));
			set_jmp_gosub_destination(sp_code, jmp_dest1,
						  a1, g1, l1, 5);
		} else if ((uint16_t)sp_sim.wr[0] ==
			   (uint16_t)(sp_sim.wr[1] >> 48)) {
			fsl_print("\t\t WR0[48:63](%04x) == WR1[0:15](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)(sp_sim.wr[1] >> 48));
			set_jmp_gosub_destination(sp_code, jmp_dest0,
						  a0, g0, l0, 5);
		} else {
			fsl_print("\t\t WR0[48:63](%04x) != WR1[48:63](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)sp_sim.wr[1]);
			fsl_print("\t\t WR0[48:63](%04x) != WR1[32:47](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)(sp_sim.wr[1] >> 16));
			fsl_print("\t\t WR0[48:63](%04x) != WR1[16:31](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)(sp_sim.wr[1] >> 32));
			fsl_print("\t\t WR0[48:63](%04x) != WR1[0:15](%04x)\n",
				  (uint16_t)sp_sim.wr[0],
				  (uint16_t)(sp_sim.wr[1] >> 48));
			(*sp_code) += 5;
			sp_sim.pc += 5;
			ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
			fsl_print("\t\t Continue with the next PC = 0x%03x\n",
				  sp_sim.pc);
		}
	} else {
		(*sp_code) += 5;
		sp_sim.pc += 5;
		ASSERT_COND(sp_sim.pc <= sp_sim.pc_end);
	}
}

/******************************************************************************/
static void sp_dump_input(void)
{
	int		i;
	uint8_t		*pb;

	#define dump_struct_bytes(_what, _len)				\
	do {								\
		fsl_print("    %s Length = %d\n\t ", _what, _len);	\
		for (i = 0; i < _len; i++) {				\
			fsl_print("%02x ", *pb++);			\
			if (((i + 1) % 16) == 0)			\
				fsl_print("\n\t ");			\
		}							\
		fsl_print("\n");					\
		if (_len % 16)						\
			fsl_print("\n");				\
	} while (0)

	pb = &sp_sim.pa[0];
	dump_struct_bytes("Parameters Array (PA)", SP_PA_SIZE);
	pb = (uint8_t *)&sp_sim.ra;
	dump_struct_bytes("Parse Array (RA)", sizeof(struct sp_parse_array));
	pb = (uint8_t *)&sp_sim.frm;
	dump_struct_bytes("Packet (PKT)", sp_sim.frm_len);
}

/******************************************************************************/
static int sp_disassm_sim(uint8_t *byte_code, int sp_size)
{
	int		i;
	uint16_t	*sp_code, opcode;

	sp_code = (uint16_t *)byte_code;
	fsl_print("\n DPAA_%d - Parser (byte_code = 0x%08x size = %d)\n",
		  SP_DPAA_VERSION, (uint32_t)byte_code, sp_size);
	ASSERT_COND(SP_DPAA_VERSION == 2 || SP_DPAA_VERSION == 1);
	if (sp_sim.sim_enabled) {
		fsl_print(" Soft Parser SIM is RUNNING\n");
		if (sp_sim.init_status & SIM_RA_SET)
			fsl_print(" Parse Array :     Set\n");
		else
			fsl_print(" Parse Array :     Default used.\n");
		if (sp_sim.init_status & SIM_PA_SET)
			fsl_print(" Parameter Array : Set\n");
		else
			fsl_print(" Parameter Array : Default used.\n");
		if (sp_sim.init_status & SIM_HB_SET)
			fsl_print(" Header Base :     Set\n");
		else
			fsl_print(" Header Base :     Default used.\n");
		if (sp_sim.init_status & SIM_PCLIM_SET)
			fsl_print(" PC limit :        Set\n");
		else
			fsl_print(" PC limit :        Default used.\n");
		fsl_print("    Program Counter (PC) = 0x%03x\n", sp_sim.pc);
		fsl_print("    PC limit counter     = %d\n", sp_sim.lim_count);
		fsl_print("    Header Base (HB)     = %d\n", sp_sim.hb);
		fsl_print("    Window Offset (WO)   = %d\n", sp_sim.wo);
		for (i = 0; i < NUM_WR; i++)
			fsl_print("    Working Register WR%d = 0x%08x-%08x\n",
				  i, (uint32_t)(sp_sim.wr[i] >> 32),
				  (uint32_t)sp_sim.wr[i]);
		/* Dump Parse Array, Parameter Array, Packet to be parsed */
		sp_dump_input();
	} else {
		fsl_print("    Program Counter (PC) = 0x%03x\n", sp_sim.pc);
	}
	/* Run disassembler-simulator */
	do {
		opcode = sp_find_opcode(*sp_code);
		switch (opcode) {
#if (SP_DPAA_VERSION == 1)
		case CONFIRM_LAYER_MASK:
			sp_dpaa1_confirm_layer_mask(&sp_code);
			break;
		case OR_IV_LCV:
			sp_dpaa1_or_iv_lcv(&sp_code);
			break;
		case LOAD_LCV_TO_WR:
			sp_dpaa1_load_lcv_to_wr(&sp_code);
			break;
		case STORE_WR_TO_LCV:
			sp_dpaa1_store_wr_to_lcv(&sp_code);
			break;
		case COMPARE_WR0_TO_IV:
			sp_dpaa1_compare_wr0_to_iv(&sp_code);
			break;
#else
		case RETURN_SUB:
			sp_return_sub(&sp_code);
			break;
		case SET_CLR_FAF:
			sp_set_clear_faf(&sp_code);
			break;
		case JUMP_FAF:
			sp_jump_faf(&sp_code);
			break;
#endif
		case JUMP_GOSUB:
			sp_jump_gosub(&sp_code);
			break;
		case NOP:
			sp_nop(&sp_code);
			break;
		case ADVANCE_HB_BY_WO:
			sp_advance_hb_by_wo(&sp_code);
			break;
		case ZERO_WR:
			sp_zero_wr(&sp_code);
			break;
		case ONES_CMP_WR1_TO_WR0:
			sp_ones_comp_wr1_to_wr0(&sp_code);
			break;
		case CASE1_DJ_WR_TO_WR:
			sp_case1_dj_wr_to_wr(&sp_code);
			break;
		case CASE2_DC_WR_TO_WR:
			sp_case2_dc_wr_to_wr(&sp_code);
			break;
		case CASE2_DJ_WR_TO_WR:
			sp_case2_dj_wr_to_wr(&sp_code);
			break;
		case CASE3_DC_WR_TO_WR:
			sp_case3_dc_wr_to_wr(&sp_code);
			break;
		case CASE3_DJ_WR_TO_WR:
			sp_case3_dj_wr_to_wr(&sp_code);
			break;
		case CASE4_DC_WR_TO_WR:
			sp_case4_dc_wr_to_wr(&sp_code);
			break;
		case JUMP_PROTOCOL:
			sp_jump_protocol(&sp_code);
			break;
		case ADD_SUB_WR_WR_TO_WR:
			sp_add_sub_wr_to_wr(&sp_code);
			break;
		case ADD_SUB_WR_IV_TO_WR:
			sp_add_sub_wr_iv_to_wr(&sp_code);
			break;
		case BITWISE_WR_WR_TO_WR:
			sp_bitwise_wr_wr_to_wr(&sp_code);
			break;
		case COMPARE_WR0_WR1:
			sp_cmp_wr_jump(&sp_code);
			break;
		case MODIFY_WO_BY_WR:
			sp_modify_wo_by_wr(&sp_code);
			break;
		case BITWISE_WR_IV_TO_WR:
			sp_bitwise_wr_iv_to_wr(&sp_code);
			break;
		case SHIFT_LEFT_WR_BY_SV:
			sp_shift_left_wr_by_sv(&sp_code);
			break;
		case SHIFT_RIGHT_WR_BY_SV:
			sp_shift_right_wr_by_sv(&sp_code);
			break;
		case LOAD_BITS_IV_TO_WR:
			sp_load_bits_iv_to_wr(&sp_code);
			break;
		case LOAD_SV_TO_WO:
			sp_load_sv_to_wo(&sp_code);
			break;
		case ADD_SV_TO_WO:
			sp_add_sv_to_wo(&sp_code);
			break;
		case STORE_IV_TO_RA:
			sp_store_iv_to_ra(&sp_code);
			break;
		case LOAD_BYTES_PA_TO_WR:
			sp_load_bytes_pa_to_wr(&sp_code);
			break;
		case STORE_WR_TO_RA:
			sp_store_wr_to_ra(&sp_code);
			break;
		case LOAD_BYTES_RA_TO_WR:
			sp_load_bytes_ra_to_wr(&sp_code);
			break;
		case LOAD_BITS_FW_TO_WR:
			sp_load_bits_fw_to_wr(&sp_code);
			break;
		default:
			sp_not_implemented(&sp_code);
			break;
		}
		switch (sp_sim.sp_status) {
		default:
		case 0:
			break;
		case SP_ERR_INVAL_OPCODE:
			fsl_print("\t Invalid OpCode detected !\n");
			fsl_print("ERROR : Disassembler%sStopped\n\n",
				  (sp_sim.sim_enabled ? " and SIM " : " "));
			return -1;
		case SP_ERR_INVAL_DST:
			fsl_print("ERROR : Disassembler%sStopped\n\n",
				  (sp_sim.sim_enabled ? " and SIM " : " "));
			return -1;
		case SP_ERR_INVAL_PARAM:
			fsl_print("ERROR : Disassembler%sStopped\n\n",
				  (sp_sim.sim_enabled ? " and SIM " : " "));
			return -1;
		case RET_TO_HARD_HXS:
			fsl_print("\t Return to hard HXS\n");
			fsl_print("SUCCESS : Disassembler%sStopped\n\n",
				  (sp_sim.sim_enabled ? " and SIM " : " "));
			return 0;
		case END_PARSING:
			fsl_print("\t Parsing End\n");
			fsl_print("SUCCESS : Disassembler%sStopped\n\n",
				  (sp_sim.sim_enabled ? " and SIM " : " "));
			return 0;
		case SP_HARD_HXS_CALLED:
			fsl_print("\t Hard HXS called\n");
			fsl_print("SUCCESS : Disassembler%sStopped\n\n",
				  (sp_sim.sim_enabled ? " and SIM " : " "));
			return 0;
		}
		if (!sp_sim.sim_enabled)
			continue;
		/* Increment executed instructions counter */
		sp_sim.instr_count++;
		/* If initially lim_count is 0, the limit count check is
		 * disabled */
		if (!sp_sim.lim_count)
			continue;
		/* Decrement limit instructions counter */
		sp_sim.lim_count--;
		if (!sp_sim.lim_count) {
			fsl_print("\t Instructions count limit exceeded !\n");
			fsl_print("ERROR : Disassembler and SIM Stopped\n\n");
			return -1;
		}
	} while (sp_sim.pc < sp_sim.pc_end);

	return 0;
}

/******************************************************************************/
int sparser_disa(uint16_t pc, uint8_t *byte_code, int sp_size)
{
	int	ret;

	/* Disable SIM */
	sp_sim.sim_enabled = 0;
	if (pc < SP_MIN_PC || pc >= SP_MAX_PC) {
		pr_err("0x%x : Invalid starting PC (< 0x%x or >= 0x%x)\n",
		       pc, SP_MIN_PC, SP_MAX_PC);
		return -1;
	}
	/* Set starting PC */
	sp_sim.pc_start = pc;
	/* Set pc_end to size of the byte_code */
	sp_sim.pc_end = (uint16_t)DIV_CEIL(sp_size, 2) + sp_sim.pc_start;
	if (sp_sim.pc_end >= SP_MAX_PC) {
		pr_err("Invalid ending PC (>= 0x%x)\n", SP_MAX_PC);
		return -1;
	}
	sp_sim.pc = sp_sim.pc_start;
	/* Start disassembler/simulator */
	ret = sp_disassm_sim(byte_code, sp_size);
	return ret;
}

/******************************************************************************/
void sparser_sim_init(void)
{
	int	i;

	memset(&sp_sim, 0, sizeof(struct soft_parser_sim));
	/* Initialize Parse Array */
	memset(&sp_sim.ra_arr, 0, sizeof(sp_sim.ra_arr));
	sp_sim.ra.pr.shim_offset_1 = 0xff;
	sp_sim.ra.pr.shim_offset_2 = 0xff;
#ifndef LS2085A_REV1
	sp_sim.ra.pr.ip_1_pid_offset = 0xff;
#else
	sp_sim.ra.pr.ip_pid_offset = 0xff;
#endif
	sp_sim.ra.pr.eth_offset = 0xff;
	sp_sim.ra.pr.llc_snap_offset = 0xff;
	sp_sim.ra.pr.vlan_tci1_offset = 0xff;
	sp_sim.ra.pr.vlan_tcin_offset = 0xff;
	sp_sim.ra.pr.last_etype_offset = 0xff;
	sp_sim.ra.pr.pppoe_offset = 0xff;
	sp_sim.ra.pr.mpls_offset_1 = 0xff;
	sp_sim.ra.pr.mpls_offset_n = 0xff;
#ifndef LS2085A_REV1
	sp_sim.ra.pr.l3_offset = 0xff;
#else
	sp_sim.ra.pr.ip1_or_arp_offset = 0xff;
#endif
	sp_sim.ra.pr.ipn_or_minencap_offset = 0xff;
	sp_sim.ra.pr.gre_offset = 0xff;
	sp_sim.ra.pr.l4_offset = 0xff;
#ifndef LS2085A_REV1
	sp_sim.ra.pr.l5_offset = 0xff;
#else
	sp_sim.ra.pr.gtp_esp_ipsec_offset = 0xff;
#endif
	sp_sim.ra.pr.routing_hdr_offset1 = 0xff;
	sp_sim.ra.pr.routing_hdr_offset2 = 0xff;
	sp_sim.ra.pr.nxt_hdr_offset = 0xff;
	sp_sim.ra.pr.ipv6_frag_offset = 0xff;
#ifndef LS2085A_REV1
	sp_sim.ra.pr.nxt_hdr_before_ipv6_frag_ext = 0xff;
	sp_sim.ra.pr.ip_n_pid_offset = 0xff;
#endif
	/* Initialize SP parameters */
	for (i = 0; i < SP_PA_SIZE; i++)
		sp_sim.pa[i] = 0;
	sp_sim.init_status = SIM_INITIALIZED;
}

/******************************************************************************/
int sparser_sim_init_parse_array(struct sp_parse_array *ra)
{
	if (!(sp_sim.init_status & SIM_INITIALIZED)) {
		pr_err("Simulator is not initialized\n");
		return -1;
	}
	memset(ra, 0, sizeof(struct sp_parse_array));
	ra->pr.shim_offset_1 = 0xff;
	ra->pr.shim_offset_2 = 0xff;
#ifndef LS2085A_REV1
	ra->pr.ip_1_pid_offset = 0xff;
#else
	ra->pr.ip_pid_offset = 0xff;
#endif
	ra->pr.eth_offset = 0xff;
	ra->pr.llc_snap_offset = 0xff;
	ra->pr.vlan_tci1_offset = 0xff;
	ra->pr.vlan_tcin_offset = 0xff;
	ra->pr.last_etype_offset = 0xff;
	ra->pr.pppoe_offset = 0xff;
	ra->pr.mpls_offset_1 = 0xff;
	ra->pr.mpls_offset_n = 0xff;
#ifndef LS2085A_REV1
	ra->pr.l3_offset = 0xff;
#else
	ra->pr.ip1_or_arp_offset = 0xff;
#endif
	ra->pr.ipn_or_minencap_offset = 0xff;
	ra->pr.gre_offset = 0xff;
	ra->pr.l4_offset = 0xff;
#ifndef LS2085A_REV1
	ra->pr.l5_offset = 0xff;
#else
	ra->pr.gtp_esp_ipsec_offset = 0xff;
#endif
	ra->pr.routing_hdr_offset1 = 0xff;
	ra->pr.routing_hdr_offset2 = 0xff;
	ra->pr.nxt_hdr_offset = 0xff;
	ra->pr.ipv6_frag_offset = 0xff;
#ifndef LS2085A_REV1
	ra->pr.nxt_hdr_before_ipv6_frag_ext = 0xff;
	ra->pr.ip_n_pid_offset = 0xff;
#endif
	return 0;
}

/******************************************************************************/
int sparser_sim_set_parse_array(struct sp_parse_array *ra)
{
	if (!(sp_sim.init_status & SIM_INITIALIZED)) {
		pr_err("Simulator is not initialized\n");
		return -1;
	}
	memcpy(&sp_sim.ra, ra, sizeof(struct sp_parse_array));
	sp_sim.init_status |= SIM_RA_SET;
	return 0;
}

/******************************************************************************/
int sparser_sim_set_parameter_array(uint8_t *pa, uint8_t offset, uint8_t size)
{
	if (!(sp_sim.init_status & SIM_INITIALIZED)) {
		pr_err("Simulator is not initialized\n");
		return -1;
	}
	if (offset + size > SP_PA_SIZE) {
		pr_err("Invalid parameters (offset + size > %d)\n", SP_PA_SIZE);
		return -1;
	}
	ASSERT_COND(pa);
	memcpy(&sp_sim.pa[offset], pa, size);
	sp_sim.init_status |= SIM_PA_SET;
	return 0;
}

/******************************************************************************/
int sparser_sim_set_parsed_pkt(uint8_t *prs_pkt, uint16_t pkt_size)
{
	if (!(sp_sim.init_status & SIM_INITIALIZED)) {
		pr_err("Simulator is not initialized\n");
		return -1;
	}
	if (!pkt_size || pkt_size > SP_SIM_MAX_FRM_LEN) {
		pr_err("%d : Invalid parsed packet length\n", pkt_size);
		return -1;
	}
	memcpy(&sp_sim.frm, prs_pkt, pkt_size);
	sp_sim.frm_len = pkt_size;
	sp_sim.init_status |= SIM_PKT_SET;
	return 0;
}

/******************************************************************************/
int sparser_sim_set_header_base(uint16_t hb)
{
	if (!(sp_sim.init_status & SIM_INITIALIZED)) {
		pr_err("Simulator is not initialized\n");
		return -1;
	}
	sp_sim.hb = hb;
	sp_sim.init_status |= SIM_HB_SET;
	return 0;
}

/******************************************************************************/
int sparser_sim_set_pc_limit(uint16_t pc_limit)
{
	if (!(sp_sim.init_status & SIM_INITIALIZED)) {
		pr_err("Simulator is not initialized\n");
		return -1;
	}
	if (pc_limit > SP_SIM_MAX_CYCLE_LIMIT) {
		pr_err("%d : Invalid Program Counter limit\n", pc_limit);
		return -1;
	}
	sp_sim.lim_count = pc_limit;
	sp_sim.init_status |= SIM_PCLIM_SET;
	return 0;
}

/******************************************************************************/
int sparser_sim(uint16_t pc, uint8_t *byte_code, int sp_size)
{
	int	ret;

	if (!(sp_sim.init_status & SIM_INITIALIZED)) {
		pr_err("Simulator is not initialized\n");
		return -1;
	}
	if (!(sp_sim.init_status & SIM_PKT_SET)) {
		pr_err("Parsed packet not set\n");
		return -1;
	}
	if (pc < SP_MIN_PC || pc >= SP_MAX_PC) {
		pr_err("0x%x : Invalid starting PC (< 0x%x or >= 0x%x)\n",
		       pc, SP_MIN_PC, SP_MAX_PC);
		return -1;
	}
	/* Set starting PC */
	sp_sim.pc_start = pc;
	sp_sim.pc = sp_sim.pc_start;
	/* Set pc_end to size of the byte_code */
	sp_sim.pc_end = (uint16_t)DIV_CEIL(sp_size, 2) + sp_sim.pc_start;
	if (sp_sim.pc_end >= SP_MAX_PC) {
		pr_err("Invalid ending PC (>= 0x%x)\n", SP_MAX_PC);
		return -1;
	}
	sp_sim.sim_enabled = 1;	/* Enable SIM */
	sp_sim.wo = 0;		/* Window Offset */
	sp_sim.wr[0] = 0;	/* Clear WR0 */
	sp_sim.wr[1] = 0;	/* Clear WR1 */
	sp_sim.sp_status = 0;	/* Clear status/error */
	sp_sim.pc_ret = 0;	/* Return PC */
	sp_sim.instr_count = 1;	/* Executed instructions count */
	/* Start disassembler/simulator */
	ret = sp_disassm_sim(byte_code, sp_size);
	return ret;
}

/******************************************************************************/
void sparser_sim_parse_error_print(void)
{
	sparser_parse_error_print(&sp_sim.ra.pr);
}

/******************************************************************************/
void sparser_sim_frame_attributes_dump(void)
{
	sparser_frame_attributes_dump(&sp_sim.ra.pr);
}

/******************************************************************************/
void sparser_sim_parse_result_dump(void)
{
	sparser_parse_result_dump(&sp_sim.ra.pr);
}
