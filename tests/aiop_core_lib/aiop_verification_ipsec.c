/**************************************************************************//**
@File		aiop_verification_ipsec.c

@Description	This file contains the AIOP IPsec SW Verification Commands

		Copyright 2014 Freescale Semiconductor, Inc.
*//***************************************************************************/

//#define IPSEC_OVERRIDE_RTA

#include "aiop_verification.h"
#include "aiop_verification_ipsec.h"
#include "ipsec.h"

#include "rta.h"

#ifndef IPSEC_OVERRIDE_RTA
//#include "protoshared.h"
//#include "desc/ipsec.h"

//#endif
//#include "sec_app.h"
//#include "ipsec_test_vector.h"
#endif

__VERIF_GLOBAL uint64_t sa_desc_handle[32]; /* Global in Shared RAM */
__VERIF_GLOBAL uint64_t verif_instance_handle[32]; /* Global in Shared RAM */

extern __TASK struct aiop_default_task_params default_task_params;

extern __SHRAM uint64_t ipsec_debug_buf_addr; /* Global in Shared RAM */
extern __SHRAM uint32_t ipsec_debug_buf_size; /* Global in Shared RAM */
extern __SHRAM uint32_t ipsec_debug_buf_offset; /* Global in Shared RAM */

uint16_t  aiop_verification_ipsec(uint32_t data_addr)
{
	uint16_t str_size = STR_SIZE_ERR;
	uint32_t opcode;
	uint64_t ws_sa_desc_handle; /* Temporary Workspace place holder*/
	uint64_t ws_verif_instance_handle; /* Temporary Workspace place holder*/

	opcode  = *((uint32_t *) data_addr);

	switch (opcode) {
//	case IPSEC_INIT_CMD:
//	{
//		struct ipsec_init_command *str =
//			(struct ipsec_init_command *)data_addr;
//		
//		str->status = ipsec_init(
//				str->max_sa_no);
//		*((int32_t *)(str->status_addr)) = str->status;
//		str->prc = *((struct presentation_context *) HWC_PRC_ADDRESS);
//		str_size = (uint16_t)sizeof(struct ipsec_init_command);
//		break;
//	}
	
	case IPSEC_CREATE_INSTANCE_CMD:
	{
		struct ipsec_create_instance_command *str =
			(struct ipsec_create_instance_command *)data_addr;
		
		ipsec_debug_buf_addr = NULL;
		ipsec_debug_buf_size = 0;
		ipsec_debug_buf_offset = 0;
		
		str->status = ipsec_create_instance(
				str->committed_sa_num,
				str->max_sa_num,
				str->instance_flags,
				str->tmi_id,
				&ws_verif_instance_handle
			);
		
		verif_instance_handle[str->instance_id] = ws_verif_instance_handle;
		str->instance_addr = ws_verif_instance_handle;
		
		*((int32_t *)(str->status_addr)) = str->status;
		str->prc = *((struct presentation_context *) HWC_PRC_ADDRESS);
		str_size = (uint16_t)sizeof(struct ipsec_create_instance_command);
		break;
	}

	case IPSEC_DELETE_INSTANCE_CMD:
	{
		struct ipsec_delete_instance_command *str =
			(struct ipsec_delete_instance_command *)data_addr;
		
		str->status = ipsec_delete_instance(
				verif_instance_handle[str->instance_id]
				);
				
		*((int32_t *)(str->status_addr)) = str->status;
		str->prc = *((struct presentation_context *) HWC_PRC_ADDRESS);
		str_size = (uint16_t)sizeof(struct ipsec_delete_instance_command);
		break;
	}

	case IPSEC_ADD_SA_DESCRIPTOR_CMD:
	{
		struct ipsec_add_sa_descriptor_command *str =
			(struct ipsec_add_sa_descriptor_command *)data_addr;
		
		/* Pointer to outer IP header within the command string */
		if (str->params.direction == IPSEC_DIRECTION_OUTBOUND) {
			str->params.encparams.outer_hdr = (uint32_t *)str->outer_ip_header;
		}
		
		
		str->status = ipsec_add_sa_descriptor(
				&(str->params),
				verif_instance_handle[str->instance_id],
				&ws_sa_desc_handle
				);

		sa_desc_handle[str->sa_desc_id] = ws_sa_desc_handle;
		
		str->descriptor_addr = ws_sa_desc_handle;
		
		*((int32_t *)(str->status_addr)) = str->status;
		str->prc = *((struct presentation_context *) HWC_PRC_ADDRESS);
		str_size = (uint16_t)sizeof(struct ipsec_add_sa_descriptor_command);
		
		break;
	}
	
	case IPSEC_DEL_SA_DESCRIPTOR_CMD:
	{
		struct ipsec_del_sa_descriptor_command *str =
			(struct ipsec_del_sa_descriptor_command *)data_addr;
		
		str->status = ipsec_del_sa_descriptor(
				sa_desc_handle[str->sa_desc_id]
				);
		
		*((int32_t *)(str->status_addr)) = str->status;
		str->prc = *((struct presentation_context *) HWC_PRC_ADDRESS);
		str_size = (uint16_t)sizeof(struct ipsec_del_sa_descriptor_command);
		
		break;
	}
	
	case IPSEC_GET_LIFETIME_STATS_CMD:
	{
		struct ipsec_get_lifetime_stats_command *str =
			(struct ipsec_get_lifetime_stats_command *)data_addr;
		
		str->status = ipsec_get_lifetime_stats(
				//*((uint64_t *)(str->ipsec_handle_ptr)),
				sa_desc_handle[str->sa_desc_id],
				&(str-> kilobytes),
				&(str-> packets),
				&(str-> sec)
				);
		
		*((int32_t *)(str->status_addr)) = str->status;
		str->prc = *((struct presentation_context *) HWC_PRC_ADDRESS);
		str_size = (uint16_t)sizeof(struct ipsec_get_lifetime_stats_command);
		
		break;
	}
	
	case IPSEC_DECR_LIFETIME_COUNTERS_CMD:
	{
		struct ipsec_decr_lifetime_counters_command *str =
			(struct ipsec_decr_lifetime_counters_command *)data_addr;
		
		str->status = ipsec_decr_lifetime_counters(
				sa_desc_handle[str->sa_desc_id],
				str->kilobytes_decr_val,
				str->packets_decr_val
				);
		
		*((int32_t *)(str->status_addr)) = str->status;
		str->prc = *((struct presentation_context *) HWC_PRC_ADDRESS);
		str_size = (uint16_t)sizeof(struct ipsec_decr_lifetime_counters_command);
		
		break;
	}
	
	case IPSEC_GET_SEQ_NUM_CMD:
	{
		struct ipsec_get_seq_num_command *str =
			(struct ipsec_get_seq_num_command *)data_addr;
		
		str->status = ipsec_get_seq_num(
				//*((uint64_t *)(str->ipsec_handle_ptr)),
				sa_desc_handle[str->sa_desc_id],
				&(str->sequence_number),
				&(str->extended_sequence_number),
				&(str->anti_replay_bitmap[4])
				);
		
		*((int32_t *)(str->status_addr)) = str->status;
		str->prc = *((struct presentation_context *) HWC_PRC_ADDRESS);
		str_size = (uint16_t)sizeof(struct ipsec_get_seq_num_command);
		
		break;
	}
	
	
	case IPSEC_FRAME_DECRYPT_CMD:
	{
		struct ipsec_frame_decrypt_command *str =
			(struct ipsec_frame_decrypt_command *)data_addr;
		
		default_task_params.parser_starting_hxs = str->starting_hxs;
		/* Run parser, in case the starting HXS changed */ 
		parse_result_generate_default (PARSER_NO_FLAGS);
		
		str->status = ipsec_frame_decrypt(
				//*((uint64_t *)(str->ipsec_handle_ptr)),
				sa_desc_handle[str->sa_desc_id],
				&str->dec_status		
				);
		*((int32_t *)(str->status_addr)) = str->status;
		str->prc = *((struct presentation_context *) HWC_PRC_ADDRESS);
		str->pr = *((struct parse_result *) HWC_PARSE_RES_ADDRESS);
		str_size = (uint16_t)sizeof(struct ipsec_frame_decrypt_command);
		
		break;
	}
	case IPSEC_FRAME_ENCRYPT_CMD:
	{
		struct ipsec_frame_encrypt_command *str =
			(struct ipsec_frame_encrypt_command *)data_addr;
		
		default_task_params.parser_starting_hxs = str->starting_hxs;
		/* Run parser, in case the starting HXS changed */ 
		parse_result_generate_default (PARSER_NO_FLAGS);
		
		str->status = ipsec_frame_encrypt(
				//*((uint64_t *)(str->ipsec_handle_ptr)),
				sa_desc_handle[str->sa_desc_id],
				&str->enc_status		
				);
		*((int32_t *)(str->status_addr)) = str->status;
		str->prc = *((struct presentation_context *) HWC_PRC_ADDRESS);
		str->pr = *((struct parse_result *) HWC_PARSE_RES_ADDRESS);
		str_size = (uint16_t)sizeof(struct ipsec_frame_encrypt_command);
		
		/* Run parser on new (encrypted) frame 
		 * This is done for cases when decrypt after encrypt is required */ 
		parse_result_generate_default (PARSER_NO_FLAGS);
		
		break;
	}
	
	case IPSEC_FRAME_ENCR_DECR_CMD:
	{
		struct ipsec_frame_encr_decr_command *str =
			(struct ipsec_frame_encr_decr_command *)data_addr;
		
		default_task_params.parser_starting_hxs = str->starting_hxs;
		/* Run parser, in case the starting HXS changed */ 
		parse_result_generate_default (PARSER_NO_FLAGS);
		
		/* Encryption */
		str->fm_encr_status = ipsec_frame_encrypt(
				//*((uint64_t *)(str->ipsec_encr_handle_ptr)),
				sa_desc_handle[str->encr_sa_desc_id],
				&str->enc_status		
				);
		*((int32_t *)(str->fm_encr_status_addr)) = str->fm_encr_status;
		
		/* if encryption failed, don't do decryption */
		if (str->fm_encr_status)
			break;
		
		
		/* Run parser on new (encrypted) frame */ 
		parse_result_generate_default (PARSER_NO_FLAGS);
		
		/* Decryption */
		str->fm_decr_status = ipsec_frame_decrypt(
				//*((uint64_t *)(str->ipsec_decr_handle_ptr)),
				sa_desc_handle[str->decr_sa_desc_id],
				&str->dec_status		
		);
		*((int32_t *)(str->fm_decr_status_addr)) = str->fm_decr_status;
		
		str->prc = *((struct presentation_context *) HWC_PRC_ADDRESS);
		str->pr = *((struct parse_result *) HWC_PARSE_RES_ADDRESS);
		str_size = (uint16_t)sizeof(struct ipsec_frame_encr_decr_command);
		
		break;
	}
	
	
	#ifndef IPSEC_OVERRIDE_RTA
	/* RTA descriptor debug */
	/*
	case IPSEC_RUN_DESC_DEBUG:
	{
		struct ipsec_run_desc_debug_command *str =
				(struct ipsec_run_desc_debug_command *)data_addr;
		
			
		str->status = sec_run_desc(&(str->descriptor_addr));
			
		*((int32_t *)(str->status_addr)) = str->status;
		str->prc = *((struct presentation_context *) HWC_PRC_ADDRESS);
		str_size = (uint16_t)sizeof(struct ipsec_run_desc_debug_command);
			
		break;
	}
	*/
	#endif
	
	case IPSEC_CREATE_DEBUG_BUFFER_CMD:
	{
		struct ipsec_create_debug_buffer_command *str =
			(struct ipsec_create_debug_buffer_command *)data_addr;
		
		ipsec_debug_buf_addr = str->buffer_addr;
		ipsec_debug_buf_size = str->buffer_size;
		
		str->status = 0;
		*((int32_t *)(str->status_addr)) = str->status;
		str->prc = *((struct presentation_context *) HWC_PRC_ADDRESS);
		str_size = (uint16_t)sizeof(struct ipsec_create_debug_buffer_command);
		break;
	}
	
	
	default:
	{
		return STR_SIZE_ERR;
	}
	}
	
	return str_size;
}






