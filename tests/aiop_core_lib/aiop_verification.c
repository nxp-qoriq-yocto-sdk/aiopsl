/**************************************************************************//**
@File		aiop_verification.c

@Description	This file contains the AIOP SW Verification Engine

		Copyright 2013 Freescale Semiconductor, Inc.
*//***************************************************************************/

#include "dplib/fsl_fdma.h"

#include "aiop_verification.h"

void aiop_verification()
{
	/* Presentation Context */
	struct presentation_context *PRC;
	uint32_t asa_seg_addr;	/* ASA Segment Address */
	uint16_t asa_seg_size;  /* ASA Segment Size */
	uint16_t size = 0;	/* ASA incremental read size */
	uint16_t str_size;
	uint32_t opcode;

	/* initialize Additional Dequeue Context */
	PRC = (struct presentation_context *) HWC_PRC_ADDRESS;

	/* Initialize ASA variables */
	asa_seg_addr = (uint32_t)(PRC->asapa_asaps & PRC_ASAPA_MASK);
	/* shift size by 6 since the size is in 64bytes (2^6 = 64) quantities */
	asa_seg_size = (PRC->asapa_asaps & PRC_ASAPS_MASK) << 6;
	
	/* initialize profile sram */
	/* This is a temporary function and has to be used only until 
			 * the ARENA will initialize the profile sram */
	init_profile_sram();

	/* The condition is for back up only.
	In case the ASA was written correctly the Terminate command will
	finish the verification */
	while (size < asa_seg_size) {
		opcode  = *((uint32_t *) asa_seg_addr);

		switch ((opcode & ACCEL_ID_CMD_MASK) >> 16) {

		case FDMA_MODULE:
		{
			str_size = aiop_verification_fdma(asa_seg_addr);
			break;
		}
		case TMAN_MODULE:
		{
			str_size = aiop_verification_tman(asa_seg_addr);
			break;
		}
		case STE_MODULE:
		{
			str_size = aiop_verification_ste(asa_seg_addr);
			break;
		}
		case CDMA_MODULE:
		{
			str_size = aiop_verification_cdma(asa_seg_addr);
			break;
		}
		case CTLU_MODULE:
		{
			str_size = aiop_verification_ctlu(asa_seg_addr);
			break;
		}
		case CTLU_PARSE_CLASSIFY_MODULE:
		{
			str_size = aiop_verification_parser(asa_seg_addr);
			break;
		}
		case HM_MODULE:
		{
			str_size = aiop_verification_hm(asa_seg_addr);
			break;
		}
		case VPOOL_MODULE:
		{
			str_size = verification_virtual_pools(asa_seg_addr);
			break;
		}
		case TERMINATE_FLOW_MODULE:
		default:
		{
			fdma_terminate_task();
			return;
		}
		}

		if (str_size == STR_SIZE_ERR) {
			fdma_terminate_task();
			return;
		}
		asa_seg_addr += str_size;
		size += str_size;
	}

	fdma_terminate_task();

	return;
}
