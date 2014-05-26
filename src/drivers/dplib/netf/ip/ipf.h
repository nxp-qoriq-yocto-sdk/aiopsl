/**************************************************************************//**
 @File          ipf.h

 @Description   This file contains the AIOP SW IP Fragmentation Internal API
*//***************************************************************************/
#ifndef __IPF_H
#define __IPF_H

#include "dplib/fsl_ipf.h"

/**************************************************************************//**
@Group		AIOP_IPF_INTERNAL  AIOP IPF Internal

@Description	AIOP IPF Internal

@{
*//***************************************************************************/


/**************************************************************************//**
@Group		IPF_INTERNAL_STRUCTS IPF Internal Structures

@Description	AIOP IPF Internal Structures

@{
*//***************************************************************************/

/**************************************************************************//**
@Description	IPF Context Internally used by IPF functions.
*//***************************************************************************/
struct ipf_context {
	 /** Remaining frame's FD  */
	struct ldpaa_fd rem_fd
			__attribute__((aligned(sizeof(struct ldpaa_fd))));
	/** Frame ID. Used for IPv6 fragmentation extension in case of
	 * fragmentation according to MTU.
	 * In case \ref IPF_RESTORE_ORIGINAL_FRAGMENTS flag is set, this
	 * parameter is ignored and the ID is inherited from the original
	 * fragments. */
	uint32_t frame_id;
	/** Flags - Please refer to \ref IPF_FLAGS */
	uint32_t flags;
	/** Original Starting HXS for Parser from Task default */
	uint16_t parser_starting_hxs;
	/** Original segment address from PRC */
	uint16_t prc_seg_address;
	/** Original segment length from PRC */
	uint16_t prc_seg_length;
	/** Remaining payload length (for split by MTU) */
	uint16_t remaining_payload_length;
	/** MTU payload length (for split by MTU) */
	uint16_t mtu_payload_length;
	/** Split size (for split by MTU) */
	uint16_t split_size;
	/** Previous Fragment Offset */
	uint16_t prev_frag_offset;
	/** Original Parser Profile ID from Task default */
	uint8_t parser_profile_id;
	/** Remaining frame handle */
	uint8_t rem_frame_handle;
	/** First fragment indication */
	uint8_t	first_frag;
	/** IPv4 indication */
	uint8_t ipv4;
	/** IP offset */
	uint8_t ip_offset;
	/** IPv6 Fragment header offset	*/
	uint8_t ipv6_frag_hdr_offset;
	/** Padding */
	uint8_t	pad[4];
};

/** @} */ /* end of IPF_INTERNAL_STRUCTS */


/**************************************************************************//**
@Group	IPF_INTERNAL_MACROS IPF Internal Macros

@Description	IPF Internal Macros

@{
*//***************************************************************************/

/**************************************************************************//**
 @Group	IPF_GENERAL_INT_DEFINITIONS IPF General Internal Definitions

 @Description IPF General Internal Definitions.

 @{
*//***************************************************************************/

	/** Size of IPF Context. */
#define SIZEOF_IPF_CONTEXT	(sizeof(struct ipf_context))

#pragma warning_errors on
/** IPF internal struct size assertion check. */
ASSERT_STRUCT_SIZE(SIZEOF_IPF_CONTEXT, IPF_CONTEXT_SIZE);
#pragma warning_errors off

/** @} */ /* end of IPF_GENERAL_INT_DEFINITIONS */

/** @} */ /* end of IPF_INTERNAL_MACROS */


/*inline*/ int32_t ipf_restore_orig_fragment(struct ipf_context *ipf_ctx);
/*inline */int32_t ipf_after_split_ipv6_fragment(struct ipf_context *ipf_ctx,
						uint32_t last_ext_hdr_size);
/*inline*/ int32_t ipf_after_split_ipv4_fragment(struct ipf_context *ipf_ctx);
/*inline*/ int32_t ipf_split_ipv4_fragment(struct ipf_context *ipf_ctx);
/*inline*/ int32_t ipf_split_ipv6_fragment(struct ipf_context *ipf_ctx,
						uint32_t last_ext_hdr_size);
/*inline*/ int32_t ipf_move_remaining_frame(struct ipf_context *ipf_ctx);
/*inline*/int32_t ipf_ipv4_last_frag(struct ipf_context *ipf_ctx);
/*inline*/int32_t ipf_ipv6_last_frag(struct ipf_context *ipf_ctx);

/*
int32_t ipf_insert_ipv6_frag_header(struct ipf_context *ipf_ctx,
		uint16_t frag_hdr_offset);
*/

/** @} */ /* end of AIOP_IPF_INTERNAL */



#endif /* __GSO_H */
