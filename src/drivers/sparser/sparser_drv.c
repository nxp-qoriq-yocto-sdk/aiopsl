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

#include "fsl_types.h"
#include <__mem.h>
#include "fsl_stdio.h"
#include "fsl_io.h"
#include "sparser_drv.h"	/* Internal SP API */
#include "fsl_sparser_drv.h"	/* External SP API */

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
/* Macro SP_DRV_RUN_ON_SIMULATOR must be defined if code is run on Simulator.
 * There is a bug in SIM implementation : The Parser can't be stopped.
 *
 * WARNING : Unexpected behavior occurs if the macro remains defined when the
 * code is run on a board.
 */
/*#define SP_DRV_RUN_ON_SIMULATOR*/

/******************************************************************************/
#define SOC_PERIPH_OFF_AIOP_CTLU		0x38000
#define CTLU_PARSER_OFFSET			0

#define PARSER_REGS_ADDR	(AIOP_PERIPHERALS_OFF +			\
				 SOC_PERIPH_OFF_AIOP_TILE +		\
				 SOC_PERIPH_OFF_AIOP_CTLU +		\
				 CTLU_PARSER_OFFSET)

/* Parser enable(d) flag */
#define PARSER_ENABLE			0x01
/* Parser idle flag */
#define PARSER_IDLE			0x01
/* Parser stop retry counter */
#define	PARSER_STOP_RETRY		100

/* Note: Parser register space over loads HXS region of the soft parse
 * addressable space. */
struct parser_regs {
	uint32_t par_cfg;	/* Parser Configuration Register */
	uint32_t par_idle;	/* Parser Idle Register */
	uint32_t par_pclim;	/* Parsing Cycle Limit Register */
	uint32_t par_ip_rev_1;	/* Parser IP Block Revision 1 Reg. */
	uint32_t par_ip_rev_2;	/* Parser IP Block Revision 2 Reg. */
	uint32_t reserved[11];	/* Reserved */
};

/* Maximum number of managed software parsers */
#define PARSER_MAX_SP				32
/*****************************/
/* Reserved software parsers */
/*****************************/
/* External reserved SP index. This position is reserved to keep information
 * about the soft parsers pre-loaded by MC. */
#define PARSER_EXT_SP_IDX			PARSER_MAX_SP
/* Internal reserved SP index. This position is reserved to keep information
 * about the soft parsers pre-loaded by AIOP (SP "library" routines). */
#define PARSER_LIB_SP_IDX			(PARSER_MAX_SP + 1)

/* Parser driver structure */
struct sparser_drv {
	/* Parser Registers */
	struct parser_regs	*regs;
	/* Index of the soft parser to be loaded */
	uint16_t		sp_idx;
	/* Note : The last 2 indexes are reserved to keep information about the
	 * pre-loaded soft parsers. The parameters area of a soft parser must
	 * not overlap others soft parsers parameters area. */
	struct sparser_info	sp_info[PARSER_MAX_SP + 2];
};

/******************************************************************************/
/* SP "library" data */
static struct sparser_info	sp_aiop_lib_info;

/* SP driver data */
static struct sparser_drv	parser_drv;

/******************************************************************************/
static __COLD_CODE int sp_drv_stop_parser(void)
{
	struct parser_regs	*pregs;
	uint32_t		par_cfg, par_idle;
	int			retry;

	pregs = (struct parser_regs *)PARSER_REGS_ADDR;
	retry = PARSER_STOP_RETRY;
	/* Disable parser */
	do {
		par_cfg = ioread32be(&pregs->par_cfg);
		par_cfg &= ~PARSER_ENABLE;
		iowrite32be(par_cfg, &pregs->par_cfg);
		par_idle = ioread32be(&pregs->par_idle);
		retry--;
	} while (!(par_idle & PARSER_IDLE) && retry > 0);
	if (retry <= 0) {
#ifdef SP_DRV_RUN_ON_SIMULATOR
		/* The code above does not work on Simulator. Force return 0 */
		pr_warn("Can't stop parser after %d retries !\n",
			PARSER_STOP_RETRY);
		return 0;
#else
		pr_err("Can't stop parser after %d retries !\n",
		       PARSER_STOP_RETRY);
		return -1;
#endif
	}
	return 0;
}

/******************************************************************************/
static __COLD_CODE void sp_drv_start_parser(void)
{
	struct parser_regs	*pregs;
	uint32_t		par_cfg;

	pregs = (struct parser_regs *)PARSER_REGS_ADDR;
	par_cfg = ioread32be(&pregs->par_cfg);
	par_cfg |= PARSER_ENABLE;
	iowrite32be(par_cfg, &pregs->par_cfg);
}

/******************************************************************************/
static __COLD_CODE int sp_drv_check_sp_info(struct sparser_info *sp,
					    uint16_t sp_idx)
{
	uint32_t	sz, tmp_sz;
	int		i;

	if (sp->pc < PARSER_MIN_PC || sp->pc >= PARSER_MAX_PC) {
		pr_err("Invalid starting PC 0x%x (< 0x%x or >= 0x%x)\n",
		       sp->pc, PARSER_MIN_PC, PARSER_MAX_PC);
		return -1;
	}
	if (!sp->size) {
		pr_err("SP size is 0\n");
		return -1;
	}
	if (sp->size % 4) {
		pr_err("SP size %d is not 4 multiple\n", sp->size);
		return -1;
	}
	if (sp->param_off && !sp->param_size) {
		pr_err("Invalid SP parameters (off = %d size = %d)\n",
		       sp->param_off, sp->param_size);
		return -1;
	}
	sz = (uint32_t)(DIV_CEIL(sp->size, 2) + sp->pc);
	if (sz >= PARSER_MAX_PC) {
		pr_err("SP code exceeds maximum PC (>= 0x%x)\n", PARSER_MAX_PC);
		return -1;
	}
	/* WRIOP parsers. They are managed/loaded by MC. Let the SP of the
	 * SPARSER_MC_AIOP type to be checked against code and parameters
	 * overlapping */
	if (sp->type != SPARSER_AIOP_AIOP && sp->type != SPARSER_MC_AIOP)
		return 0;
	sz -= sp->pc;
	/* Check for code overlapping with the already loaded or with the
	 * pre-loaded SPs */
	for (i = 0; i < PARSER_MAX_SP + 2; i++) {
		if (!parser_drv.sp_info[i].pc)
			continue;
		tmp_sz = (uint32_t)(DIV_CEIL(parser_drv.sp_info[i].size, 2));
		if ((sp->pc >= parser_drv.sp_info[i].pc &&
		     sp->pc <= parser_drv.sp_info[i].pc + tmp_sz - 1) ||
		    (sp->pc + sz - 1 >= parser_drv.sp_info[i].pc &&
		     sp->pc + sz <= parser_drv.sp_info[i].pc + tmp_sz)) {
			pr_err("SP_#%d over SP_#%d: Code overlap\n",
			       sp_idx, i);
			return -1;
		}
	}
	/* Check for parameters overlapping with the already loaded or with the
	 * pre-loaded SPs. Parameters are stored in a Parse Profile. Parameters
	 * in the same profile must not overlap. */
	for (i = 0; sp->param_size && i < PARSER_MAX_SP + 2; i++) {
		/* Check for :
		 *	- not used entry,
		 *	- entry with no parameters
		 *	- entry from other parse profile */
		if (!parser_drv.sp_info[i].pc ||
		    !parser_drv.sp_info[i].param_size ||
		    sp->prpid != parser_drv.sp_info[i].prpid)
			continue;
		if ((sp->param_off >= parser_drv.sp_info[i].param_off &&
		     sp->param_off <= parser_drv.sp_info[i].param_off +
		     parser_drv.sp_info[i].param_size - 1) ||
		    (sp->param_off + sp->param_size - 1 >=
		     parser_drv.sp_info[i].param_off &&
		     sp->param_off + sp->param_size <=
		     parser_drv.sp_info[i].param_off +
		     parser_drv.sp_info[i].param_size)) {
			pr_err("SP_#%d over SP_#%d: Parameters overlap\n",
			       sp_idx, i);
			return -1;
		}
	}
	return 0;
}

/******************************************************************************/
static __COLD_CODE int sp_drv_init(struct sparser_info *sp_ext_info,
				   struct sparser_info *sp_lib_info)
{
	int		i, ret;
	uint32_t	*sp_mem;

	ret = sp_drv_stop_parser();
	if (ret)
		return ret;
	memset(&parser_drv, 0, sizeof(struct sparser_drv));
	parser_drv.regs = (struct parser_regs *)PARSER_REGS_ADDR;
	if (sp_ext_info) {
		pr_info("Loading SPs pre-loaded by MC at 0x%x\n",
			sp_ext_info->pc);
		ret = sp_drv_check_sp_info(sp_ext_info,
					   (uint16_t)PARSER_EXT_SP_IDX);
		if (ret) {
			sp_drv_start_parser();
			return ret;
		}
		parser_drv.sp_info[PARSER_EXT_SP_IDX].pc = sp_ext_info->pc;
		parser_drv.sp_info[PARSER_EXT_SP_IDX].size = sp_ext_info->size;
		parser_drv.sp_info[PARSER_EXT_SP_IDX].byte_code =
			(uint8_t *)((uint32_t)parser_drv.regs +
				    2 * sp_ext_info->pc);
		parser_drv.sp_info[PARSER_EXT_SP_IDX].param_off =
				sp_ext_info->param_off;
		parser_drv.sp_info[PARSER_EXT_SP_IDX].param_size =
				sp_ext_info->param_size;
		parser_drv.sp_info[PARSER_EXT_SP_IDX].prpid =
				sp_ext_info->prpid;
	}
	if (sp_lib_info) {
		pr_info("Loading SPs \"library\" pre-loaded by AIOP at 0x%x\n",
			sp_lib_info->pc);
		ret = sp_drv_check_sp_info(sp_lib_info,
					   (uint16_t)PARSER_LIB_SP_IDX);
		if (ret) {
			sp_drv_start_parser();
			return ret;
		}
		parser_drv.sp_info[PARSER_LIB_SP_IDX].pc = sp_lib_info->pc;
		parser_drv.sp_info[PARSER_LIB_SP_IDX].size = sp_lib_info->size;
		parser_drv.sp_info[PARSER_LIB_SP_IDX].byte_code =
			(uint8_t *)((uint32_t)parser_drv.regs +
				    2 * sp_lib_info->pc);
		parser_drv.sp_info[PARSER_LIB_SP_IDX].param_off =
				sp_lib_info->param_off;
		parser_drv.sp_info[PARSER_LIB_SP_IDX].param_size =
				sp_lib_info->param_size;
		parser_drv.sp_info[PARSER_LIB_SP_IDX].prpid =
				sp_lib_info->prpid;
	}
	/* Clear Soft Parser instructions area. Skip pre-loaded SPs. Last 4
	 * bytes of the instructions area must be always cleared */
	sp_mem = (uint32_t *)((uint32_t)parser_drv.regs + 2 * PARSER_MIN_PC);
	for (i = PARSER_MIN_PC; i < PARSER_MAX_PC + 3; ) {
		if (sp_ext_info && (uint8_t *)sp_mem ==
		    parser_drv.sp_info[PARSER_EXT_SP_IDX].byte_code) {
			sp_mem += parser_drv.sp_info[PARSER_EXT_SP_IDX].
					size / 4;
			i += parser_drv.sp_info[PARSER_EXT_SP_IDX].size;
			continue;
		}
		if (sp_lib_info && (uint8_t *)sp_mem ==
		    parser_drv.sp_info[PARSER_LIB_SP_IDX].byte_code) {
			sp_mem += parser_drv.sp_info[PARSER_LIB_SP_IDX].
					size / 4;
			i += parser_drv.sp_info[PARSER_LIB_SP_IDX].size;
			continue;
		}
		iowrite32be(0, sp_mem++);
		i += sizeof(uint32_t);
	}
	/* Dump Soft Parser instructions area */
	parser_drv.sp_idx = 0;
	/* Re-enable parser */
	sp_drv_start_parser();
	return 0;
}

/*******************************************************************************
 * SP Driver internal API functions
 ******************************************************************************/
static __COLD_CODE
int sparser_drv_get_preloaded_sp_info(struct sparser_info *sp_info)
{
	if (sp_info->type == SPARSER_MC_AIOP) {
		/* TODO - Implement MC command getting the following hard
		 * coded information */
		sp_info->pc = 0x1E0;
		/* Code loaded by MC is not a 4 multiple in size. Add 1 to the
		 * size to make it a 4 multiple */
		sp_info->size = (0x3FB - 0x1E0 + 1) * sizeof(uint16_t);
		sp_info->byte_code = NULL;
		sp_info->param_off = 0;
		sp_info->param_size = 0;
		sp_info->prpid = 0;
	} else if (sp_info->type == SPARSER_AIOP_AIOP) {
		memcpy(sp_info, &sp_aiop_lib_info, sizeof(struct sparser_info));
	} else {
		pr_err("Unsupported SP type : 0x%x\n", (uint32_t)sp_info->type);
		return -1;
	}
	return 0;
}

/******************************************************************************/
static __COLD_CODE int sparser_drv_send_sp(struct sparser_info *sp_info)
{
	UNUSED(sp_info);
	/* TODO - Implement MC command */
	return 0;
}

/******************************************************************************/
__COLD_CODE int sparser_drv_early_init(void)
{
	int		i, ret;
	uint32_t	*sp_mem, *sp_code, sz;
	uint8_t		sp_aiop_lib_parsers[4] __attribute__((aligned(4))) = {
				0x00, 0x00, 0x00, 0x00
			};
	/* sp_aiop_lib_parsers[] array contains the byte-code of the AIOP_SL
	 * SPs "library".
	 *
	 * The SPs in this library must occupy contiguous space into the
	 * internal memory of the AIOP Parser. Only one SP driver entry is
	 * reserved to keep information about all SPs in the library.
	 *
	 * The SPs in this library are :
	 *	- routine SPs. Sequences of instructions performing a certain
	 *	operation (by example compute the Running Sum). The routines
	 *	may be called by user defined SPs and ends by executing a
	 *	RETURN instruction.
	 *	- standard SPs. May be SPs performing a certain operation on the
	 *	packet (by example :skip a custom header, skip a standard
	 *	header).
	 *
	 * Notes :
	 *	1. Loading address (PC), SPs code size, SPs parameters (if any)
	 *	must be documented when this support is added.
	 *	2. It's not mandatory to populate the SPs library with SPs.
	 */

	/* Fill the sp_aiop_lib_info fields with the needed information */
	sp_aiop_lib_info.pc = 0x000; /* Must set the right value */
	sp_aiop_lib_info.size = 0; /* Must set the right value. The following
				    commented line sets the correct size.
				    Un-comment it when sp_aiop_lib_parsers
				    contains a valid SP. */
	/*sp_aiop_lib_info.size = sizeof(sp_aiop_lib_parsers);*/
	sp_aiop_lib_info.byte_code = &sp_aiop_lib_parsers[0];
	sp_aiop_lib_info.param_off = 0;	/* Must set the right value */
	sp_aiop_lib_info.param_size = 0;
	sp_aiop_lib_info.prpid = 0; /* Must set the right value */
	sp_aiop_lib_info.type = SPARSER_AIOP_AIOP;
	/* Load SPs "library" */
	if (!sp_aiop_lib_info.pc) {
		pr_info("Soft Parser early initialization succeeded\n");
		return 0;
	}
	/* Check SP library parameters */
	if (sp_aiop_lib_info.pc < PARSER_MIN_PC ||
	    sp_aiop_lib_info.pc >= PARSER_MAX_PC) {
		pr_err("Invalid starting PC 0x%x (< 0x%x or >= 0x%x)\n",
		       sp_aiop_lib_info.pc, PARSER_MIN_PC, PARSER_MAX_PC);
		return -1;
	}
	if (sp_aiop_lib_info.size % 4) {
		pr_err("SP size %d is not 4 multiple\n", sp_aiop_lib_info.size);
		return -1;
	}
	if (sp_aiop_lib_info.param_off && !sp_aiop_lib_info.param_size) {
		pr_err("Invalid SP parameters (off = %d size = %d)\n",
		       sp_aiop_lib_info.param_off, sp_aiop_lib_info.param_size);
		return -1;
	}
	sz = (uint32_t)(DIV_CEIL(sp_aiop_lib_info.size, 2) +
				 sp_aiop_lib_info.pc);
	if (sz >= PARSER_MAX_PC) {
		pr_err("SP code exceeds maximum PC (>= 0x%x)\n", PARSER_MAX_PC);
		return -1;
	}
	/* Disable parser */
	ret = sp_drv_stop_parser();
	if (ret)
		return ret;
	/* Load SP library code */
	sp_code = (uint32_t *)sp_aiop_lib_info.byte_code;
	sp_mem = (uint32_t *)(PARSER_REGS_ADDR + 2 * sp_aiop_lib_info.pc);
	for (i = 0; i < DIV_CEIL(sp_aiop_lib_info.size, 4); i++) {
		fsl_print("\t 0x%08x : 0x%08x\n", (uint32_t)sp_mem, *sp_code);
		iowrite32be(*sp_code++, sp_mem++);
	}
	memcpy(&parser_drv.sp_info[PARSER_LIB_SP_IDX], &sp_aiop_lib_info,
	       sizeof(struct sparser_info));
	/* Re-enable parser */
	sp_drv_start_parser();
	pr_info("Soft Parser early initialization succeeded\n");
	return 0;
}

/******************************************************************************/
__COLD_CODE int sparser_drv_init(void)
{
	int			ret;
	struct sparser_info	sp_ext_info, sp_lib_info;

	memset(&parser_drv, 0, sizeof(struct sparser_drv));
	parser_drv.regs = (struct parser_regs *)PARSER_REGS_ADDR;
	/* Get information about the SP pre-loaded by MC in the internal
	 * memory of the AIOP Parser */
	memset(&sp_ext_info, 0, sizeof(struct sparser_info));
	sp_ext_info.type = SPARSER_MC_AIOP;
	ret = sparser_drv_get_preloaded_sp_info(&sp_ext_info);
	if (ret)
		return ret;
	/* Get information about the SP pre-loaded by AIOP in the internal
	 * memory of the AIOP Parser */
	memset(&sp_lib_info, 0, sizeof(struct sparser_info));
	sp_lib_info.type = SPARSER_AIOP_AIOP;
	ret = sparser_drv_get_preloaded_sp_info(&sp_lib_info);
	if (ret)
		return ret;
	ret = sp_drv_init(sp_ext_info.size ? &sp_ext_info : NULL,
			  sp_lib_info.size ? &sp_lib_info : NULL);
	if (ret)
		pr_err("Soft Parser initialization failed\n");
	else
		pr_info("Soft Parser initialization succeeded\n");
	return ret;
}

/*******************************************************************************
 * SP Driver external API functions
 ******************************************************************************/
__COLD_CODE int sparser_drv_load_parser(struct sparser_info *sp)
{
	int		i, ret;
	uint32_t	*sp_mem, *sp_code;

	ASSERT_COND(sp);
	pr_info("Loading application defined SP at 0x%x\n", sp->pc);
	if (sp->type == SPARSER_AIOP_AIOP &&
	    parser_drv.sp_idx == PARSER_MAX_SP) {
		pr_err("%d : Too many AIOP soft parsers.\n", PARSER_MAX_SP);
		return -1;
	}
	ret = sp_drv_check_sp_info(sp, parser_drv.sp_idx);
	if (ret)
		return ret;
	ASSERT_COND(sp->byte_code);
	if (((uint32_t)sp->byte_code) & 0x03) {
		pr_err("0x%x : Code must be aligned on a 4 bytes boundary\n",
		       (uint32_t)sp->byte_code);
		return -1;
	}
	if (sp->type == SPARSER_MC_AIOP) {
		pr_err("SPARSER_MC_AIOP : Invalid SP type.\n");
		return -1;
	}
	/* SP is loaded/managed by MC. Send it to MC. */
	if (sp->type != SPARSER_AIOP_AIOP) {
		pr_info("Sending SP to MC\n");
		ret = sparser_drv_send_sp(sp);
		return ret;
	}
	/* Disable parser */
	ret = sp_drv_stop_parser();
	if (ret)
		return ret;
	/* Load SP code */
	sp_code = (uint32_t *)sp->byte_code;
	sp_mem = (uint32_t *)((uint32_t)parser_drv.regs + 2 * sp->pc);
	for (i = 0; i < DIV_CEIL(sp->size, 4); i++)
		iowrite32be(*sp_code++, sp_mem++);
	parser_drv.sp_info[parser_drv.sp_idx].pc = sp->pc;
	parser_drv.sp_info[parser_drv.sp_idx].size = sp->size;
	parser_drv.sp_info[parser_drv.sp_idx].byte_code =
			(uint8_t *)((uint32_t)parser_drv.regs + 2 * sp->pc);
	parser_drv.sp_info[parser_drv.sp_idx].param_off = sp->param_off;
	parser_drv.sp_info[parser_drv.sp_idx].param_size = sp->param_size;
	parser_drv.sp_info[parser_drv.sp_idx].prpid = sp->prpid;
	parser_drv.sp_idx++;
	/* Re-enable parser */
	sp_drv_start_parser();
	return 0;
}

/******************************************************************************/
__COLD_CODE uint32_t sparser_drv_get_pclim(void)
{
	struct parser_regs		*pregs;

	pregs = (struct parser_regs *)PARSER_REGS_ADDR;
	return ioread32be(&pregs->par_pclim);
}

/******************************************************************************/
__COLD_CODE void sparser_drv_set_pclim(uint32_t limit)
{
	struct parser_regs		*pregs;

	pregs = (struct parser_regs *)PARSER_REGS_ADDR;
	if (limit > PARSER_CYCLE_LIMIT_MAX) {
		pr_info("Limit adjusted to maximum %d\n",
			PARSER_CYCLE_LIMIT_MAX);
		limit = PARSER_CYCLE_LIMIT_MAX;
	}
	if (!limit)
		pr_warn("Parser cycles limit check is disabled\n");
	iowrite32be(limit, &pregs->par_pclim);
}

#ifdef SL_DEBUG
/******************************************************************************/
__COLD_CODE void sparser_drv_memory_dump(uint16_t from_pc, uint16_t to_pc)
{
	int		i;
	uint32_t	*pmem, val32;
	uint16_t	tmp_from_pc;

	if (from_pc < PARSER_MIN_PC) {
		pr_info("0x%x from PC adjusted to 0x%x\n", from_pc,
			PARSER_MIN_PC);
		from_pc = PARSER_MIN_PC;
	}
	if (to_pc > PARSER_MAX_PC + 3) {
		pr_info("0x%x to PC adjusted to 0x%x\n", to_pc,
			PARSER_MAX_PC + 3);
		to_pc = PARSER_MAX_PC + 3;
	}
	if (from_pc >= to_pc) {
		pr_info("From PC : 0x%x to PC >= To PC : to 0x%x\n", from_pc,
			to_pc);
		return;
	}
	tmp_from_pc = from_pc - (from_pc % 8);
	pmem = (uint32_t *)(PARSER_REGS_ADDR + 2 * tmp_from_pc);
	fsl_print("Dump of Parser memory (at 0x%08x) PC[0x%x:0x%x]\n",
		  (uint32_t)pmem, from_pc, to_pc - 1);
	fsl_print("\t 0x%x : ", from_pc - (from_pc % 8));
	for (i = tmp_from_pc; i < to_pc; ) {
		val32 = ioread32be(pmem++);
		if (i < from_pc)
			fsl_print("     ");
		else
			fsl_print("%04x ", (uint16_t)(val32 >> 16));
		i++;
		if (i == to_pc) {
			fsl_print("\n");
			break;
		}
		if (i < from_pc)
			fsl_print("     ");
		else
			fsl_print("%04x ", (uint16_t)val32);
		i++;
		if (i == to_pc) {
			fsl_print("\n");
			break;
		}
		if ((i % 8) == 0)
			fsl_print("\n\t 0x%x : ", i);
	}
	fsl_print("\n");
}

/******************************************************************************/
__COLD_CODE void sparser_drv_regs_dump(void)
{
	struct parser_regs		*pregs;

	pregs = (struct parser_regs *)PARSER_REGS_ADDR;
	fsl_print("Dump of Parser registers (at 0x%08x)\n", (uint32_t)pregs);
	fsl_print("\t PAR_CFG      = 0x%08x\n", ioread32be(&pregs->par_cfg));
	fsl_print("\t PAR_IDLE     = 0x%08x\n", ioread32be(&pregs->par_idle));
	fsl_print("\t PAR_PCLIM    = 0x%08x\n", ioread32be(&pregs->par_pclim));
	fsl_print("\t PAR_IP_REV_1 = 0x%08x\n",
		  ioread32be(&pregs->par_ip_rev_1));
	fsl_print("\t PAR_IP_REV_2 = 0x%08x\n",
		  ioread32be(&pregs->par_ip_rev_2));
}
#endif	/* SL_DEBUG */
