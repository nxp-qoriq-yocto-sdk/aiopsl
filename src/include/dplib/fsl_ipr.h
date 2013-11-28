/**************************************************************************//**
@File          fsl_ipr.h

@Description   This file contains the AIOP SW IP Reassembly API

	       Copyright 2013 Freescale Semiconductor, Inc.
*//***************************************************************************/

#ifndef __FSL_IPR_H
#define __FSL_IPR_H

/**************************************************************************//**
@Description	IPR Instance handle Type definition

 *//***************************************************************************/
typedef uint64_t ipr_instance_handle_t;

/**************************************************************************//**
@Description	User callback function, called after ipr_delete_instance
		function has finished deleting the instance and release all its
		recourses. The user provides this function and the IPR process
		invokes it.

@Param[in]	cb_delete_arg - Argument of the callback function.

 *//***************************************************************************/
typedef void (ipr_delete_callback_t)(uint64_t cb_delete_arg);

/**************************************************************************//**
@Description	User callback function, called after time out expired.
		The user provides this function and the IPR process invokes it.

		In case the first fragment (frag offset=0) was received, the
		first fragment will be set as the default frame.
		Otherwise, another fragment will be set as the default frame.

@Param[in]	cb_timeout_arg - Argument of the callback function.
@Param[in]	flags - \link FSL_IPRTOCallbackFlags IPR Time Out Callback flags
		\endlink,

@Cautions	No task default parameters beside frame_handle will be valid
		(e.g parse result).

 *//***************************************************************************/
typedef void (ipr_timeout_callback_t)(uint64_t cb_timeout_arg, uint32_t flags);

/**************************************************************************//**
@Description	IPR Parameters
*//***************************************************************************/
struct ipr_params {
		/** if extended_stat flag is set in
		\link FSL_IPRInsFlags IP reassembly flags \endlink,
		this is the address to the statistics data structure
		\ref extended_stats_cntrs. This structure should be allocated
		in DDR.\n
		If the extended_stat flag is reset, this parameter is invalid*/
	uint64_t  extended_stats_addr;
		/** maximum concurrently IPv4 open frames. */
	uint32_t  max_open_frames_ipv4;
		/** maximum concurrently IPv6 open frames */
	uint32_t  max_open_frames_ipv6;
	uint16_t  max_reass_frm_size;	/** maximum reassembled frame size */
	uint16_t  min_frag_size;	/** minimum fragment size allowed */
	uint16_t  timeout_value_ipv4;/** reass timeout value for ipv4 */
	uint16_t  timeout_value_ipv6;/** reass timeout value for ipv6 */
		/** function to call upon Time Out occurrence for ipv4 */
	ipr_timeout_callback_t *ipv4_timeout_cb;
	/** Argument to be passed upon invocation of the IPv4 callback
	    function*/
	ipr_timeout_callback_t *ipv6_timeout_cb;
	/** Argument to be passed upon invocation of the IPv6 callback
	    function*/
	uint64_t cb_timeout_ipv4_arg;
		/** function to call upon Time Out occurrence for ipv6 */
	uint64_t cb_timeout_ipv6_arg;
		/** \link FSL_IPRInsFlags IP reassembly flags \endlink */
	uint32_t  flags;
		/** 32-bit alignment. */
	uint8_t  pad[4];
};

/**************************************************************************//**
@Description	IPR Statistics counters
*//***************************************************************************/

struct extended_stats_cntrs {
		/** Counts the number of valid IPv4 fragments handled */
	uint32_t	valid_frags_cntr_ipv4;
		/** Counts the number of malformed IPv4 fragments handled
		     malformed fragments are:\n
		     -duplicate,\n
		     -overlap,\n
		     -short fragments (first or middle fragments with
		      size < ipr_config::min_frag_size),\n
		      out of range fragments (fragment offset is beyond the end
		      of the last fragment),\n
		     -first or middle fragments whose IP payload size is not a
		      multiple of 8,\n
		     -middle or last fragments in which the fragment offset * 8
		      + fragment IP payload length + IP header length exceeds
		      64Kbytes,\n
		     -fragment which carries Not-ECT codepoint and
		      any other fragment of this IP packet to be reassembled has
		      the CE codepoint set,\n
		     -fragment which carries CE codepoint set and any other
		      fragment of this IP packet to be reassembled has the
		      Not-ECT codepoint */
	uint32_t	malformed_frags_cntr_ipv4;
		/** Counts the number of times the re-assembly process can't
		     start since the number of open IPv4 reassembled frames
		     has exceeded the ipr_config::maximum_open_frames_ipv4. */
	uint32_t	open_reass_frms_exceed_ipv4_cntr;
		/** Counts the number of times the re-assembly process came up
		     against more than 64 fragments per IPv4 frame.*/
	uint32_t	more_than_64_frags_ipv4_cntr;
		/** Counts the number of times the re-assembly process of
		     an IPv4 frame stopped due to Time Out occurrence.*/
	uint32_t	time_out_ipv4_cntr;
		/** Counts the number of valid IPv6 fragments handled */
	uint32_t	valid_frags_cntr_ipv6;
		/** Counts the number of malformed IPv6 fragments handled
		     malformed fragments are:
		     -duplicate,\n
		     -overlap,\n
		     -short fragments (first or middle fragments with
		      size < ipr_config::min_frag_size),\n
		      out of range fragments (fragment offset is beyond the end
		      of the last fragment),\n
		     -first or middle fragments whose IP payload size is not a
		      multiple of 8,\n
		     -middle or last fragments in which the fragment offset * 8
		      + fragment IP payload length + IP header length exceeds
		      64Kbytes,\n
		     -fragment which carries Not-ECT codepoint and
		      any other fragment of this IP packet to be reassembled has
		      the CE codepoint set,\n
		     -fragment which carries CE codepoint set and any other
		      fragment of this IP packet to be reassembled has the
		      Not-ECT codepoint */
	uint32_t	malformed_frags_cntr_ipv6;
		/** Counts the number of times the re-assembly process can't
		     start since the number of open IPv6 reassembled frames
		     has exceeded the ipr_config::maximum_open_frames_ipv6. */
	uint32_t	open_reass_frms_exceed_ipv6_cntr;
		/** Counts the number of times the re-assembly process came up
		     against more than 64 fragments per IPv6 frame.*/
	uint32_t	more_than_64_frags_ipv6_cntr;
		/** Counts the number of times the re-assembly process of
		     an IPv6 frame stopped due to time out occurrence.*/
	uint32_t	time_out_ipv6_cntr;
		/** Counts the number of times the re-assembly process
		     couldn't start due to lack of allocated buffers in the
		     ipr_config::bman_pool_id.*/
	uint32_t	no_free_buffer_cntr;



};

/**************************************************************************//**
@Group		FSL_IPR FSL_AIOP_IPR

@Description	AIOP IP reassembly functions macros and definitions

@{
*//***************************************************************************/

/**************************************************************************//**
@Group		FSL_IPR_Modes_And_Flags IPR Modes And Flags

@Description	AIOP IP reassembly Modes and Flags

@{
*//***************************************************************************/

/**************************************************************************//**
@Group		FSL_IPRInsFlags IPR instance flags

@Description IP reassembly flags.


|      0      |      1     |      2     | 3-31 |
|-------------|------------|------------|------|
|Extended_stat|IPv4_TO_type|IPv6_TO_type|      |
\n

@{
*//***************************************************************************/

/** If set, for IPv4 frames, Timeout limits the reassembly time from the
 *   first fragment (opening fragment). If reset, Timeout limits the interval
 *   time between two consecutive fragments.*/
#define IPR_MODE_IPV4_TO_TYPE			0x40000000
/** If set, for IPv6 frames, Timeout limits the reassembly time from the
 *   first fragment (opening fragment). If reset, Timeout limits the interval
 *   time between two consecutive fragments.*/
#define IPR_MODE_IPV6_TO_TYPE			0x20000000
/** If set, extended statistics is enabled */
#define IPR_MODE_EXTENDED_STATS_EN		0x80000000

/* @} end of group FSL_IPRInsModeBits */

/**************************************************************************//**
@Group		FSL_IPRInitFlags IPR init flags

@Description	IPR init flags.

@{
*//***************************************************************************/

/** If set, the ins_mem_base_addr points to a DDR address.
 If reset, the ins_mem_base_addr points to a Shared RAM address. */
#define IPR_INIT_EXT_MEM 0x01

/* @} end of group FSL_IPRInitFlags */

/**************************************************************************//**
@Group		FSL_IPRStatsFlags IPR stats flags

@Description	IPR Statistics flags.

@{
*//***************************************************************************/

/** If set, the returned value of the function is the IPv4 number of
  reassembled frames. If reset, the returned value of the function
  is the IPv6 number of reassembled frames */
#define IPR_STATS_IP_VERSION 0x01

/* @} end of group FSL_IPRStatsFlags */

/**************************************************************************//**
@Group		FSL_IPRUpdateFlags IPR Update flags

@Description	IPR Update flags.

@{
*//***************************************************************************/

/** If set, the maximum reassembled frame size is updated with a new value.*/
#define IPR_UPDATE_MAX_FRM_SIZE 0x01
/** If set, the minimium fragment size is updated with a new value.*/
#define IPR_UPDATE_MIN_FRG_SIZE 0x02
/** If set, the Time Out value for IPv4 frames is updated with a new value.*/
#define IPR_UPDATE_TO_VALUE_IPV4 0x04
/** If set, the Time Out value for IPv6 frames is updated with a new value.*/
#define IPR_UPDATE_TO_VALUE_IPV6 0x08
/** If set, the Time Out Type (see IPR_MODE_IPV4_TO_TYPE &
 *   IPR_MODE_IPV6_TO_TYPE)  is changed the other type.*/
#define IPR_UPDATE_TO_MODE 0x10

/* @} end of group FSL_IPRUpdateFlags */

/**************************************************************************//**
@Group		FSL_IPRCreateReturnStatus IPR create instance return status

@{
*//***************************************************************************/

/* Instance was successfully created */
#define IPR_CREATE_INSTANCE_SUCCESS	SUCCESS
/* Instance creation failed due to lack of allocated buffers */
#define IPR_MAX_BUFFERS_REACHED		(IPR_MODULE_STATUS_ID + 0x0100)


/* @} end of group FSL_IPRCreateReturnStatus */


/**************************************************************************//**
@Group		FSL_IPRRessReturnStatus IPR functions return status

@{
*//***************************************************************************/

/* Frame was correctly reassembled or was a regular frame*/
#define IPR_REASSEMBLY_SUCCESS		SUCCESS
/* Reassembly isn't completed yet but fragment was successfully added to the
   partially reassembled frame*/
#define IPR_REASSEMBLY_NOT_COMPLETED	(IPR_MODULE_STATUS_ID + 0x0200)
/* There were not enough hierarchy scope levels (IPR requires 2 hierarchy scope
   levels) so the reassembly was done serially (leading to a depressed
   performance)*/
#define IPR_MAX_NESTED_REACHED		(IPR_MODULE_STATUS_ID + 0x0300)
/* Fragment has been recognized as malformed, and wasn't added
    to the partially reassembled frame*/
#define IPR_MALFORMED_FRAG		(IPR_MODULE_STATUS_ID + 0x0400)

/* @} end of group FSL_IPRRessReturnStatus */

/**************************************************************************//**
@Group		FSL_IPRTOCallbackFlags IPR Time Out Callback flags

@{
*//***************************************************************************/

/* If set, it indicates that the first fragment (frag offset=0) was received*/
#define IPR_TO_CB_FIRST_FRAG		0x01

/* @} end of group FSL_IPRTOCallbackFlags */
/* @} end of group FSL_IPR_Modes_And_Flags */

/**************************************************************************//**
@Group		FSL_IPR_Functions IPR functions

@Description	IP reassembly functions

@{
*//***************************************************************************/

/**************************************************************************//**
@Function	ipr_create_instance

@Description	Creates an IPR instance.\n

@Param[in]	ipr_params_ptr - pointer to IPR params structure \ref ipr_params
@Param[out]	ipr_instance_ptr - Pointer to the IPR Instance handle to be used
		by the caller to invoke IP reassembly functions.


@Return		Success or Failure.
		Failure can be one of the following:
		\link FSL_IPRCreateReturnStatus IPR create instance return
		status \endlink \n
		\ref FSL_CTLU_STATUS_TBL_CREATE \n
		\ref CDMA_ACQUIRE_CONTEXT_MEMORY_STATUS \n
		\ref CDMA_WRITE_STATUS \n

@Cautions	In this function, the task yields.
*//***************************************************************************/
int32_t ipr_create_instance(struct ipr_params *ipr_params_ptr,
			    ipr_instance_handle_t *ipr_instance_ptr);


/**************************************************************************//**
@Function	ipr_delete_instance

@Description	Delete a specified IPR instance.

		The delete request has been registered and the deletion will be
		performed gradually.

@Param[in]	ipr_instance - The IPR instance handle.
@Param[in]	confirm_callback - The function to be used for confirmation
		after all resources associated to the instance have been
		deleted.
@Param[in]	cb_delete_arg - Argument of the confirm callback function.

@Return		Success or Failure.
		Failure can be one of the following:
		\ref CDMA_RELEASE_CONTEXT_MEMORY_STATUS

@Cautions	In this function, the task yields.
*//***************************************************************************/
int32_t ipr_delete_instance(ipr_instance_handle_t ipr_instance,
			    ipr_delete_callback_t *confirm_callback,
			    uint64_t cb_delete_arg);

/**************************************************************************//**
@Function	ipr_reassemble

@Description	Perform reassembly.\n
		When called for a regular frame,no action is done.\n
		When called for a non-closing fragment, the fragment is
		inserted to the partially reassembled frame.\n
		When called for a closing fragment, reassembly is done.\n
		The reassembled frame is returned to the caller.

		The function returns with the same ordering scope mode
		it enters (exclusive or concurrent).

@Param[in]	ipr_instance - The IPR instance handle.

@Return		Status -\n
		\link FSL_IPRRessReturnStatus IP Reassembly Return status
		\endlink \n
		\ref FSL_CTLU_STATUS_RULE_CREATE \n
		\ref FSL_CTLU_STATUS_RULE_DELETE \n
		\ref cdma_errors \n
		\ref FDMA_CONCATENATE_FRAMES_ERRORS \n
		\ref TMANReturnStatus \n


@Cautions	If this function is called in concurrent mode, the scope_id is
		incremented.\n
		If this function is called while the task is currently
		in exclusive mode, the scope_id is preserved.
		In this function, the task yields.

*//***************************************************************************/
int32_t ipr_reassemble(ipr_instance_handle_t ipr_instance);

/**************************************************************************//**
@Function	ipr_modify_max_reass_frm_size

@Description	Update max_reass_frm_size parameter for the specified instance.

@Param[in]	ipr_instance - The IPR instance handle.
@Param[in]	max_reass_frm_size - New maximum reassembled frame size.

@Return		Success or Failure.
		Failure can be one of the following:
		\ref CDMA_READ_WITH_MUTEX_STATUS \n
		\ref CDMA_WRITE_WITH_MUTEX_STATUS \n

@Cautions	In this function, the task yields.
*//***************************************************************************/
int32_t ipr_modify_max_reass_frm_size(ipr_instance_handle_t ipr_instance,
				      uint16_t max_reass_frm_size);

/**************************************************************************//**
@Function	ipr_modify_min_frag_size

@Description	Update min_frag_size parameter for the specified instance.

@Param[in]	ipr_instance - The IPR instance handle.
@Param[in]	min_frag_size - New minimum fragment size.

@Return		Success or Failure.
		Failure can be one of the following:
		\ref CDMA_READ_WITH_MUTEX_STATUS \n
		\ref CDMA_WRITE_WITH_MUTEX_STATUS \n

@Cautions	In this function, the task yields.
*//***************************************************************************/
int32_t ipr_modify_min_frag_size(ipr_instance_handle_t ipr_instance,
				 uint16_t min_frag_size);

/**************************************************************************//**
@Function	ipr_modify_timeout_value_ipv4

@Description	Update reasm_timeout_value_ipv4 parameter for the specified
		instance.

@Param[in]	ipr_instance - The IPR instance handle.
@Param[in]	reasm_timeout_value_ipv4 - New reassembly timeout value for ipv4

@Return		Success or Failure.
		Failure can be one of the following:
		\ref CDMA_READ_WITH_MUTEX_STATUS \n
		\ref CDMA_WRITE_WITH_MUTEX_STATUS \n

@Cautions	In this function, the task yields.
*//***************************************************************************/
int32_t ipr_modify_timeout_value_ipv4(ipr_instance_handle_t ipr_instance,
				      uint16_t reasm_timeout_value_ipv4);

/**************************************************************************//**
@Function	ipr_modify_timeout_value_ipv6

@Description	Update reasm_timeout_value_ipv6 parameter for the specified
		instance.

@Param[in]	ipr_instance - The IPR instance handle.
@Param[in]	reasm_timeout_value_ipv6 - New reassembly timeout value for ipv6

@Return		Success or Failure.
		Failure can be one of the following:
		\ref CDMA_READ_WITH_MUTEX_STATUS \n
		\ref CDMA_WRITE_WITH_MUTEX_STATUS \n

@Cautions	In this function, the task yields.
*//***************************************************************************/
int32_t ipr_modify_timeout_value_ipv6(ipr_instance_handle_t ipr_instance,
				      uint16_t reasm_timeout_value_ipv6);

/**************************************************************************//**
@Function	ipr_get_reass_frm_cntr

@Description	Returns the number of reassembled frames counter value
		of the given instance.

@Param[in]	ipr_instance - The IPR instance handle.
@Param[in]	flags - \link FSL_IPRStatsFlags flags \endlink
@Param[out]	reass_frm_cntr - The number of IPv4/IPv6 reassembled frames
		for this instance.

@Return		Success or Failure.
		Failure can be one of the \ref CDMA_READ_STATUS

@Cautions	None.
*//***************************************************************************/
uint32_t ipr_get_reass_frm_cntr(ipr_instance_handle_t ipr_instance,
				uint32_t flags, uint32_t *reass_frm_cntr);

/* @} end of group FSL_IPR_Functions */
/* @} end of group FSL_IPR */

#endif /* __FSL_IPR_H */