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

/**************************************************************************//**
@File		aiop_verification_osm.h

@Description	This file contains the AIOP OSM SW Verification Structures.

*//***************************************************************************/


#ifndef __AIOP_VERIFICATION_OSM_H_
#define __AIOP_VERIFICATION_OSM_H_

//#include "dplib/fsl_ldpaa.h"
#include "dplib/fsl_osm.h"
#include "general.h"
#include "osm.h"

/**************************************************************************//**
 @addtogroup		AIOP_Service_Routines_Verification

 @{
*//***************************************************************************/

/* OSM Register  */
#define	OSM_REG_ORTAR 	0x0209c020
#define	OSM_REG_OERR 	0x0209ce00
#define ENABLE_ERR_REG 	0x8000001f

/* Read task registers */
#define OSM_REG_ORTDR0()\
	(((struct osm_registers *)OSM_REG_ORTAR)->ortdr0)
#define OSM_REG_ORTDR1()\
	(((struct osm_registers *)OSM_REG_ORTAR)->ortdr1)
#define OSM_REG_ORTDR2()\
	(((struct osm_registers *)OSM_REG_ORTAR)->ortdr2)
#define OSM_REG_ORTDR3()\
	(((struct osm_registers *)OSM_REG_ORTAR)->ortdr3)
#define OSM_REG_ORTDR4()\
	(((struct osm_registers *)OSM_REG_ORTAR)->ortdr4)
#define OSM_REG_ORTDR5()\
	(((struct osm_registers *)OSM_REG_ORTAR)->ortdr5)
#define OSM_REG_ORTDR6()\
	(((struct osm_registers *)OSM_REG_ORTAR)->ortdr6)
#define OSM_REG_ORTDR7()\
	(((struct osm_registers *)OSM_REG_ORTAR)->ortdr7)

/* Error report registers */
#define OSM_REG_OEDR()\
	(((struct osm_error_reg *)OSM_REG_OERR)->oedr)
#define OSM_REG_OEDDR()\
	(((struct osm_error_reg *)OSM_REG_OERR)->oeddr)
#define OSM_REG_OECR0()\
	(((struct osm_error_reg *)OSM_REG_OERR)->oecr0)
#define OSM_REG_OECR1()\
	(((struct osm_error_reg *)OSM_REG_OERR)->oecr1)
#define OSM_REG_OECR2()\
	(((struct osm_error_reg *)OSM_REG_OERR)->oecr2)
#define OSM_REG_OECR3()\
	(((struct osm_error_reg *)OSM_REG_OERR)->oecr3)
#define OSM_REG_OECR4()\
	(((struct osm_error_reg *)OSM_REG_OERR)->oecr4)
#define OSM_REG_OECR5()\
	(((struct osm_error_reg *)OSM_REG_OERR)->oecr5)
#define OSM_REG_OECR6()\
	(((struct osm_error_reg *)OSM_REG_OERR)->oecr6)
#define OSM_REG_OECR7()\
	(((struct osm_error_reg *)OSM_REG_OERR)->oecr7)



/**************************************************************************//**
 @Group		AIOP_OSM_SRs_Verification

 @Description	AIOP OSM Verification structures definitions.

 @{
*//***************************************************************************/

/** \enum osm_verif_cmd_type defines the OSM verification CMDTYPE
 * field. */
enum osm_verif_cmd_type {
	OSM_SCOPE_TRANS_TO_EX_INC_SCOPE_ID_CMDTYPE = 0,
	OSM_SCOPE_TRANS_TO_EX_NEW_SCOPE_ID_CMDTYPE,
	OSM_SCOPE_TRANS_TO_CON_INC_SCOPE_ID_CMDTYPE,
	OSM_SCOPE_TRANS_TO_CON_NEW_SCOPE_ID_CMDTYPE,	
	OSM_SCOPE_RELINQUISH_EX_CMDTYPE,
	OSM_SCOPE_ENTER_TO_EX_INC_SCOPE_ID_CMDTYPE,
	OSM_SCOPE_ENTER_TO_EX_NEW_SCOPE_ID_CMDTYPE,
	OSM_SCOPE_ENTER_CMDTYPE,
	OSM_SCOPE_EXIT_CMDTYPE,
	OSM_GET_SCOPE_CMDTYPE
};

#define OSM_SCOPE_TRANS_TO_EX_INC_SCOPE_ID_STR  ((OSM_MODULE << 16) | \
		OSM_SCOPE_TRANS_TO_EX_INC_SCOPE_ID_CMDTYPE)

#define OSM_SCOPE_TRANS_TO_EX_NEW_SCOPE_ID_STR  ((OSM_MODULE << 16) | \
		OSM_SCOPE_TRANS_TO_EX_NEW_SCOPE_ID_CMDTYPE)

#define OSM_SCOPE_TRANS_TO_CON_INC_SCOPE_ID_STR  ((OSM_MODULE << 16) | \
		OSM_SCOPE_TRANS_TO_CON_INC_SCOPE_ID_CMDTYPE)

#define OSM_SCOPE_TRANS_TO_CON_NEW_SCOPE_ID_STR  ((OSM_MODULE << 16) | \
		OSM_SCOPE_TRANS_TO_CON_NEW_SCOPE_ID_CMDTYPE)

#define OSM_SCOPE_RELINQUISH_EX_STR  ((OSM_MODULE << 16) | \
		OSM_SCOPE_RELINQUISH_EX_CMDTYPE)

#define OSM_SCOPE_ENTER_TO_EX_INC_SCOPE_ID_STR  ((OSM_MODULE << 16) | \
		OSM_SCOPE_ENTER_TO_EX_INC_SCOPE_ID_CMDTYPE)

#define OSM_SCOPE_ENTER_TO_EX_NEW_SCOPE_ID_STR  ((OSM_MODULE << 16) | \
		OSM_SCOPE_ENTER_TO_EX_NEW_SCOPE_ID_CMDTYPE)

#define OSM_SCOPE_ENTER_STR  ((OSM_MODULE << 16) | \
		OSM_SCOPE_ENTER_CMDTYPE)

#define OSM_SCOPE_EXIT_STR  ((OSM_MODULE << 16) | \
		OSM_SCOPE_EXIT_CMDTYPE)

#define OSM_GET_SCOPE_STR  ((OSM_MODULE << 16) | \
		OSM_GET_SCOPE_CMDTYPE)

/** \addtogroup AIOP_Service_Routines_Verification
 *  @{
 */


/**************************************************************************//**
 @Group		AIOP_OSM_SRs_Verification

 @Description	AIOP OSM Verification structures definitions.

 @{
*//***************************************************************************/

struct osm_registers {
	uint32_t	scope_status;
	uint32_t 	reserved;
	uint32_t 	ortdr0;
	uint32_t 	ortdr1;
	uint32_t 	ortdr2;
	uint32_t 	ortdr3;
	uint32_t 	ortdr4;
	uint32_t 	ortdr5;
	uint32_t 	ortdr6;
	uint32_t 	ortdr7;
};

struct osm_error_reg {
	uint32_t	oerr;
	uint32_t 	oedr;
	uint32_t 	oeddr;
	uint32_t 	reserved[5];
	uint32_t 	oecr0;
	uint32_t 	oecr1;
	uint32_t 	oecr2;
	uint32_t 	oecr3;
	uint32_t 	oecr4;
	uint32_t 	oecr5;
	uint32_t 	oecr6;
	uint32_t 	oecr7;
};

/**************************************************************************//**
@Description	OSM Scope Transition to Exclusive Command structure.
		
		This command increment the stage of the original scope_id (according
		to the \ref OSM_SCOPE_ID_STAGE_INCREMENT_MASK definition).
*//***************************************************************************/
struct osm_scope_tran_to_ex_inc_scope_id_verif_command {
	uint32_t	opcode;
	int32_t		status;
};

/**************************************************************************//**
@Description	OSM Scope Transition to Exclusive Command structure.
		
		This command associated with the new specified scope_id.

*//***************************************************************************/
struct osm_scope_tran_to_ex_new_scope_id_verif_command {
	uint32_t	opcode;
	uint32_t 	scope_id;
	int32_t		status;
	uint8_t 	pad[4];
};

/**************************************************************************//**
@Description	OSM Scope Transition to Concurrent Command structure.
		
		This command increment the stage of the original scope_id (according
		to the \ref OSM_SCOPE_ID_STAGE_INCREMENT_MASK definition).
*//***************************************************************************/
struct osm_scope_tran_to_con_inc_scope_id_verif_command {
	uint32_t	opcode;
	int32_t		status;
};

/**************************************************************************//**
@Description	OSM Scope Transition to Concurrent Command structure.
		
		This command associated with the new specified scope_id.
		
*//***************************************************************************/
struct osm_scope_tran_to_con_new_scope_id_verif_command {
	uint32_t	opcode;
	uint32_t 	scope_id;
	int32_t		status;
	uint8_t 	pad[4];
};

/**************************************************************************//**
@Description	OSM Scope Relinquish Exclusive Command structure.
		
		This command Proceed from exclusive (XX) to concurrent (XC) stage of 
		the same ordering scope (relinquish exclusivity).
		
*//***************************************************************************/
struct osm_scope_relinquish_ex_verif_command {
	uint32_t	opcode;
	uint8_t 	pad[4];
};

/**************************************************************************//**
@Description	OSM Scope Enter to Exclusive Command structure.
		
		This command increment the stage of the original scope_id (according
		to the \ref OSM_SCOPE_ID_STAGE_INCREMENT_MASK definition).
		
*//***************************************************************************/
struct osm_scope_enter_to_ex_inc_scope_id_verif_command {
	uint32_t	opcode;
	int32_t		status;
};

/**************************************************************************//**
@Description	OSM Scope Enter to Exclusive Command structure.
		
		This command associated with the new specified scope_id.

		
*//***************************************************************************/
struct osm_scope_enter_to_ex_new_scope_id_verif_command {
	uint32_t	opcode;
	uint32_t 	child_scope_id;
	int32_t		status;
	uint8_t 	pad[4];
};


/**************************************************************************//**
@Description	OSM Scope Enter Command structure.
		
		This command enter the next level of ordering scope in the hierarchy
		("child") to either exclusive (XX) or concurrent (XC) stage.
		
*//***************************************************************************/
struct osm_scope_enter_verif_command {
	uint32_t	opcode;
	uint32_t 	scope_enter_flags;
	uint32_t 	child_scope_id;
	int32_t		status;
};

/**************************************************************************//**
@Description	OSM Scope Exit Command structure.
		
		This command exit the current ordering scope and return to its "parent"
		ordering scope.
		
*//***************************************************************************/
struct osm_scope_exit_verif_command {
	uint32_t	opcode;
	uint8_t 	pad[4];
};

/**************************************************************************//**
@Description	OSM get Scope Command structure.
		
		This command returns the current scope status.
		
*//***************************************************************************/
struct osm_get_scope_verif_command {
	uint32_t	opcode;
	uint32_t 	scope_status;
	uint32_t 	err_scope_status;
	uint8_t 	pad[4];
};

/** @} */ /* end of AIOP_OSM_SRs_Verification */

/** @}*/ /* end of AIOP_Service_Routines_Verification */


uint16_t aiop_verification_osm(uint32_t asa_seg_addr);


#endif /* __AIOP_VERIFICATION_OSM_H_ */
