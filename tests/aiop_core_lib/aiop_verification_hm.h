/**************************************************************************//**
@File          aiop_verification_HM.h

@Description   This file contains the AIOP HM SW Verification Structures
*//***************************************************************************/


#ifndef __AIOP_VERIFICATION_HM_H_
#define __AIOP_VERIFICATION_HM_H_

#include "dplib/fsl_ldpaa.h"
#include "net/fsl_net.h"


/**************************************************************************//**
 @addtogroup		AIOP_Service_Routines_Verification

 @{
*//***************************************************************************/

/**************************************************************************//**
 @Group		AIOP_HM_SRs_Verification

 @Description	AIOP HM Verification structures definitions.

 @{
*//***************************************************************************/

#define HM_VERIF_ACCEL_ID	0xFE
	/**< HM accelerator ID For verification purposes*/
#define PARSER_INIT_BPID	1
	/**< A BPID to use for the parser init.
	 * This BPID needs to be initialized in BMAN in order for the
	 * HM tests to run correctly. The HM tests requires only one buffer. */
#define PARSER_INIT_BUFF_SIZE	128
	/**< A buffer size that corresponds to the PARSER_INIT_BPID to use for
	 * the parser init.
	 * This BPID needs to be initialized in BMAN in order for the
	 * HM tests to run correctly */
#define HM_VERIF_ACCEL_ID	0xFE
	/**< HM accelerator ID For verification purposes*/


/*! \enum e_hm_verif_cmd_type defines the statistics engine CMDTYPE field.*/
enum e_hm_verif_cmd_type {
	HM_CMDTYPE_VERIF_INIT = 0,
	HM_CMDTYPE_L2_HEADER_REMOVE,
	HM_CMDTYPE_VLAN_HEADER_REMOVE,
	HM_CMDTYPE_IPV4_MODIFICATION,
	HM_CMDTYPE_IPV6_MODIFICATION,
	HM_CMDTYPE_IPV4_ENCAPSULATION,
	HM_CMDTYPE_IPV6_ENCAPSULATION,
	HM_CMDTYPE_EXT_IPV4_ENCAPSULATION,
	HM_CMDTYPE_EXT_IPV6_ENCAPSULATION,
	HM_CMDTYPE_IP_DECAPSULATION,
	HM_CMDTYPE_UDP_MODIFICATION,
	HM_CMDTYPE_TCP_MODIFICATION,
	HM_CMDTYPE_NAT_IPV4,
	HM_CMDTYPE_NAT_IPV6,
	HM_CMDTYPE_SET_VLAN_VID,
	HM_CMDTYPE_SET_VLAN_PCP,
	HM_CMDTYPE_SET_DL_SRC,
	HM_CMDTYPE_SET_DL_DST,
	HM_CMDTYPE_SET_NW_SRC,
	HM_CMDTYPE_SET_NW_DST,
	HM_CMDTYPE_SET_TP_SRC,
	HM_CMDTYPE_SET_TP_DST,
	HM_CMDTYPE_PUSH_VLAN,
	HM_CMDTYPE_POP_VLAN
};

/* HM Commands Structure identifiers */

#define HM_VERIF_INIT_CMD_STR	((HM_MODULE << 16) | \
		(uint32_t)HM_CMDTYPE_VERIF_INIT)

#define HM_L2_HEADER_REMOVE_CMD_STR	((HM_MODULE << 16) | \
		(uint32_t)HM_CMDTYPE_L2_HEADER_REMOVE)

#define HM_VLAN_HEADER_REMOVE_CMD_STR	((HM_MODULE << 16) | \
		(uint32_t)HM_CMDTYPE_VLAN_HEADER_REMOVE)

#define HM_IPV4_MODIFICATION_CMD_STR	((HM_MODULE << 16) | \
		(uint32_t)HM_CMDTYPE_IPV4_MODIFICATION)

#define HM_IPV6_MODIFICATION_CMD_STR	((HM_MODULE << 16) | \
		(uint32_t)HM_CMDTYPE_IPV6_MODIFICATION)

#define HM_IPV4_ENCAPSULATION_CMD_STR	((HM_MODULE << 16) | \
		(uint32_t)HM_CMDTYPE_IPV4_ENCAPSULATION)

#define HM_IPV6_ENCAPSULATION_CMD_STR	((HM_MODULE << 16) | \
		(uint32_t)HM_CMDTYPE_IPV6_ENCAPSULATION)

#define HM_IPV4_EXT_ENCAPSULATION_CMD_STR	((HM_MODULE << 16) | \
		(uint32_t)HM_CMDTYPE_EXT_IPV4_ENCAPSULATION)

#define HM_IPV6_EXT_ENCAPSULATION_CMD_STR	((HM_MODULE << 16) | \
		(uint32_t)HM_CMDTYPE_EXT_IPV6_ENCAPSULATION)

#define HM_IP_DECAPSULATION_CMD_STR	((HM_MODULE << 16) | \
		(uint32_t)HM_CMDTYPE_IP_DECAPSULATION)

#define HM_UDP_MODIFICATION_CMD_STR	((HM_MODULE << 16) | \
		(uint32_t)HM_CMDTYPE_UDP_MODIFICATION)

#define HM_TCP_MODIFICATION_CMD_STR	((HM_MODULE << 16) | \
		(uint32_t)HM_CMDTYPE_TCP_MODIFICATION)

#define HM_NAT_IPV4_CMD_STR	((HM_MODULE << 16) | \
		(uint32_t)HM_CMDTYPE_NAT_IPV4)

#define HM_NAT_IPV6_CMD_STR	((HM_MODULE << 16) | \
		(uint32_t)HM_CMDTYPE_NAT_IPV6)

#define HM_SET_VLAN_VID_CMD_STR	((HM_MODULE << 16) | \
		(uint32_t)HM_CMDTYPE_SET_VLAN_VID)

#define HM_SET_VLAN_PCP_CMD_STR	((HM_MODULE << 16) | \
		(uint32_t)HM_CMDTYPE_SET_VLAN_PCP)

#define HM_SET_DL_SRC_CMD_STR	((HM_MODULE << 16) | \
		(uint32_t)HM_CMDTYPE_SET_DL_SRC)

#define HM_SET_DL_DST_CMD_STR	((HM_MODULE << 16) | \
		(uint32_t)HM_CMDTYPE_SET_DL_DST)

#define HM_SET_NW_SRC_CMD_STR	((HM_MODULE << 16) | \
		(uint32_t)HM_CMDTYPE_SET_NW_SRC)

#define HM_SET_NW_DST_CMD_STR	((HM_MODULE << 16) | \
		(uint32_t)HM_CMDTYPE_SET_NW_DST)

#define HM_SET_TP_SRC_CMD_STR	((HM_MODULE << 16) | \
		(uint32_t)HM_CMDTYPE_SET_TP_SRC)

#define HM_SET_TP_DST_CMD_STR	((HM_MODULE << 16) | \
		(uint32_t)HM_CMDTYPE_SET_TP_DST)

#define HM_PUSH_VLAN_CMD_STR	((HM_MODULE << 16) | \
		(uint32_t)HM_CMDTYPE_PUSH_VLAN)

#define HM_POP_VLAN_CMD_STR	((HM_MODULE << 16) | \
		(uint32_t)HM_CMDTYPE_POP_VLAN)



/**************************************************************************//**
@Description	HM verification init Command structure.

		This command inits the parser that are needed for the HM
		verification. This command must be called before calling the
		HM commands (it only needed to be called once in the
		beginning of the test).
*//***************************************************************************/
struct hm_init_verif_command {
	uint32_t	opcode;
		/**< Command structure identifier. */
	uint16_t	parser_starting_hxs;
	uint8_t		pad[2];
};

/**************************************************************************//**
@Description	HM L2 header remove Command structure.

		Includes information needed for HM Command verification.
*//***************************************************************************/
struct hm_l2_header_remove_command {
	uint32_t	opcode;
		/**< Command structure identifier. */
};

/**************************************************************************//**
@Description	HM VLAN header remove Command structure.

		Includes information needed for HM Command verification.
*//***************************************************************************/
struct hm_vlan_remove_command {
	uint32_t	opcode;
	/**< Command structure identifier. */
	int32_t		status;
};

/**************************************************************************//**
@Description	HM IPv4 Header Modification Command structure.

		Includes information needed for HM Command verification.
*//***************************************************************************/
struct hm_ipv4_modification_command {
	uint32_t	opcode;
		/**< Command structure identifier. */
	int32_t		status;
	uint32_t	ip_src_addr;
	uint32_t	ip_dst_addr;
	uint16_t	id;
	uint8_t		flags;
	uint8_t		tos;
};

/**************************************************************************//**
@Description	HM IPv6 Header Modification Command structure.

		Includes information needed for HM Command verification.
*//***************************************************************************/
struct hm_ipv6_modification_command {
	uint32_t	opcode;
		/**< Command structure identifier. */
	int32_t		status;
	uint32_t	flow_label;
	uint8_t		flags;
	uint8_t		tc;
	uint8_t		ip_src_addr[16];
	uint8_t		ip_dst_addr[16];
	uint8_t		pad[2];
};

/**************************************************************************//**
@Description	HM IPv4 Header Encapsulation Command structure.

		Includes information needed for HM Command verification.
*//***************************************************************************/
struct hm_ipv4_encapsulation_command {
	uint32_t	opcode;
		/**< Command structure identifier. */
	int32_t		status;
	struct ipv4hdr	ipv4_header_ptr;
	uint8_t		flags;
	uint8_t		ipv4_header_size;
	uint8_t		pad[2];
};

/**************************************************************************//**
@Description	HM IPv6 Header Encapsulation Command structure.

		Includes information needed for HM Command verification.
*//***************************************************************************/
struct hm_ipv6_encapsulation_command {
	uint32_t	opcode;
		/**< Command structure identifier. */
	int32_t		status;
	struct ipv6hdr	ipv6_header_ptr;
	uint8_t		flags;
	uint8_t		ipv6_header_size;
	uint8_t		pad[2];
};

/**************************************************************************//**
@Description	HM IP Header Decapsulation Command structure.

		Includes information needed for HM Command verification.
*//***************************************************************************/
struct hm_ip_decapsulation_command {
	uint32_t	opcode;
		/**< Command structure identifier. */
	int32_t		status;
	uint8_t		flags;
	uint8_t		pad[3];
};

/**************************************************************************//**
@Description	HM UDP Header Modification Command structure.

		Includes information needed for HM Command verification.
*//***************************************************************************/
struct hm_udp_modification_command {
	uint32_t	opcode;
		/**< Command structure identifier. */
	int32_t		status;
	uint16_t	udp_src_port;
	uint16_t	udp_dst_port;
	uint8_t		flags;
	uint8_t		pad[3];
};

/**************************************************************************//**
@Description	HM TCP Header Modification Command structure.

		Includes information needed for HM Command verification.
*//***************************************************************************/
struct hm_tcp_modification_command {
	uint32_t	opcode;
		/**< Command structure identifier. */
	int32_t		status;
	uint16_t	tcp_src_port;
	uint16_t	tcp_dst_port;
	int16_t		tcp_seq_num_delta;
	int16_t		tcp_ack_num_delta;
	uint16_t	tcp_mss;
	uint8_t		flags;
	uint8_t		pad[3];
};

/**************************************************************************//**
@Description	HM IPv4 NAT Command structure.

		Includes information needed for HM Command verification.
*//***************************************************************************/
struct hm_ipv4_nat_command {
	uint32_t	opcode;
		/**< Command structure identifier. */
	int32_t		status;
	uint32_t	ip_src_addr;
	uint32_t	ip_dst_addr;
	uint16_t	l4_src_port;
	uint16_t	l4_dst_port;
	int16_t		tcp_seq_num_delta;
	int16_t		tcp_ack_num_delta;
	uint8_t		flags;
	uint8_t		pad[3];
};

/**************************************************************************//**
@Description	HM IPv6 NAT Command structure.

		Includes information needed for HM Command verification.
*//***************************************************************************/
struct hm_ipv6_nat_command {
	uint32_t	opcode;
		/**< Command structure identifier. */
	int32_t		status;
	uint32_t	ip_src_addr[4];
	uint32_t	ip_dst_addr[4];
	uint16_t	l4_src_port;
	uint16_t	l4_dst_port;
	int16_t		tcp_seq_num_delta;
	int16_t		tcp_ack_num_delta;
	uint8_t		flags;
	uint8_t		pad[3];
};

/**************************************************************************//**
@Description	HM set VLAN VID Command structure.

		Includes information needed for HM Command verification.
*//***************************************************************************/
struct hm_vlan_vid_command {
	uint32_t	opcode;
		/**< Command structure identifier. */
	int32_t		status;
	uint16_t	vlan_vid;
	uint8_t		pad[2];
};

/**************************************************************************//**
@Description	HM set VLAN PCP Command structure.

		Includes information needed for HM Command verification.
*//***************************************************************************/
struct hm_vlan_pcp_command {
	uint32_t	opcode;
		/**< Command structure identifier. */
	int32_t		status;
	uint8_t		vlan_pcp;
	uint8_t		pad[3];
};

/**************************************************************************//**
@Description	HM set MAC SRC or DST Command structure.

		Includes information needed for HM Command verification.
*//***************************************************************************/
struct hm_set_dl_command {
	uint32_t	opcode;
		/**< Command structure identifier. */
	uint8_t		mac_addr[6];
	uint8_t		pad[2];
};

/**************************************************************************//**
@Description	HM set IPv4 SRC or DST address Command structure.

		Includes information needed for HM Command verification.
*//***************************************************************************/
struct hm_set_nw_command {
	uint32_t	opcode;
		/**< Command structure identifier. */
	int32_t		status;
	uint32_t	ipv4_addr;
};

/**************************************************************************//**
@Description	HM set UDP/TCP SRC/DST ports Command structure.

		Includes information needed for HM Command verification.
*//***************************************************************************/
struct hm_set_tp_command {
	uint32_t	opcode;
		/**< Command structure identifier. */
	int32_t		status;
	uint16_t	port;
	uint8_t		pad[2];
};

/**************************************************************************//**
@Description	HM push VLAN Command structure.

		Includes information needed for HM Command verification.
*//***************************************************************************/
struct hm_push_vlan_command {
	uint32_t	opcode;
		/**< Command structure identifier. */
	uint16_t	ethertype;
	uint8_t		pad[2];
};

/**************************************************************************//**
@Description	HM pop VLAN Command structure.

		Includes information needed for HM Command verification.
*//***************************************************************************/
struct hm_pop_vlan_command {
	uint32_t	opcode;
		/**< Command structure identifier. */
	int32_t		status;
};



void aiop_hm_init_parser();

uint16_t aiop_verification_hm(uint32_t asa_seg_addr);

/** @} */ /* end of AIOP_HM_SRs_Verification */

/** @} */ /* end of AIOP_Service_Routines_Verification */


#endif /* __AIOP_VERIFICATION_HM_H_ */
