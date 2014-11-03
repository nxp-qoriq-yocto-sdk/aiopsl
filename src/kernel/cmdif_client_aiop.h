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


/*!
 *  @file    cmdif_client.h
 *  @brief   Cmdif client AIOP<->GPP internal header file
 */

#ifndef __CMDIF_CLIENT_AIOP_H
#define __CMDIF_CLIENT_AIOP_H

#include "cmdif_client.h"
#include "cmdif_srv.h"
#include "dplib/fsl_dpci.h"
#include "fsl_gen.h"
#include "fsl_string.h"

#pragma warning_errors on
ASSERT_STRUCT_SIZE(CMDIF_OPEN_SIZEOF, CMDIF_OPEN_SIZE);
#pragma warning_errors off

/* Use it to see debug prints */
//#define CMDIF_DBG_PRINT(...) pr_debug(__VA_ARGS__) 
#define CMDIF_DBG_PRINT(...) do { } while(0)

/** BDI */
#define BDI_GET \
((((struct additional_dequeue_context *)HWC_ADC_ADDRESS)->fdsrc_va_fca_bdi) \
	& ADC_BDI_MASK)
/** eVA is OR between FD[VA]  ADC[VA config] */
#define VA_GET \
(((((struct additional_dequeue_context *)HWC_ADC_ADDRESS)->fdsrc_va_fca_bdi) \
	& ADC_VA_MASK) || LDPAA_FD_GET_VA(HWC_FD_ADDRESS))

/** BMT for memory accesses */
#define BMT_GET \
	LDPAA_FD_GET_BMT(HWC_FD_ADDRESS)

/** PL_ICID from Additional Dequeue Context */
#define PL_ICID_GET \
	LH_SWAP(0, &(((struct additional_dequeue_context *)HWC_ADC_ADDRESS)->pl_icid))

/** Get ICID to send response */
#define ICID_GET(PL_AND_ICID) ((PL_AND_ICID) & ADC_ICID_MASK)

/** Get PL to send response */
#define PL_GET(PL_AND_ICID) ((PL_AND_ICID)  & ADC_PL_MASK)

#define ADD_AMQ_FLAGS(FL, PL_AND_ICID)		\
	do {					\
		uint8_t va_get = VA_GET;	\
		uint8_t bmt_get = BMT_GET;	\
		if (PL_GET(PL_AND_ICID))	\
			FL |= FDMA_DMA_PL_BIT;	\
		if (va_get)			\
			FL |= FDMA_DMA_eVA_BIT;	\
		if (bmt_get)			\
			FL |= FDMA_DMA_BMT_BIT;	\
	}while(0)

#define CMDIF_MN_SESSIONS	(64 << 1) 
/**< Maximal number of sessions: 64 SW contexts and avg of 2 modules per each */
#define CMDIF_NUM_PR		2
#define CMDIF_FREE_SESSION	'\0'

struct cmdif_reg {
	uint16_t dpci_token;	/**< Open AIOP dpci device */
	struct dpci_attr *attr; /**< DPCI attributes */
	struct dpci_peer_attr *peer_attr; /**< DPCI peer attributes */
	struct dpci_tx_queue_attr *tx_queue_attr[DPCI_PRIO_NUM]; /**< DPCI TX attributes */
	uint32_t dma_flags;	/**< FDMA dma data flags */
	uint32_t enq_flags;	/**< FDMA enqueue flags */
	uint16_t icid;		/**< ICID per DPCI */
};

/* To be allocated on DDR */
struct cmdif_cl {
	struct {
		char m_name[M_NAME_CHARS + 1];
		/**< Module application name */
		struct cmdif_reg *regs;
		/**< Send device, to be placed as cidesc.reg */
		struct cmdif_dev *dev;
		/**< To be placed as cidesc.dev */
		uint8_t ins_id;
		/**< Instanse id that was used for open */
	} gpp[CMDIF_MN_SESSIONS];

	uint8_t count;
	/**< Count the number of sessions */
	uint8_t lock;
	/**< Lock for adding & removing new entries */
};


static inline int cmdif_cl_free_session_get(struct cmdif_cl *cl)
{
	int i;
	
	if (cl->count >= CMDIF_MN_SESSIONS)
		return -ENOSPC;
	
	for (i = 0; i < CMDIF_MN_SESSIONS; i++) {
		if (cl->gpp[i].m_name[0] == CMDIF_FREE_SESSION) 
			return i;		
	}
	
	return -ENOSPC;
}

static inline int cmdif_cl_session_get(struct cmdif_cl *cl,
                                       const char *m_name,
                                       uint8_t ins_id,
                                       uint32_t dpci_id)
{
	int i; 
	
	/* TODO stop searching if passed all open sessions cl->count */
	for (i = 0; i < CMDIF_MN_SESSIONS; i++) {
		if ((cl->gpp[i].ins_id == ins_id) &&
			(cl->gpp[i].regs->peer_attr->peer_id == dpci_id) &&
			(cl->gpp[i].m_name[0] != CMDIF_FREE_SESSION) &&
			(strncmp((const char *)&(cl->gpp[i].m_name[0]),
			         m_name,
			         M_NAME_CHARS) == 0))	
			return i;
	}
	return -ENAVAIL;	
}

static inline int cmdif_cl_auth_id_find(struct cmdif_cl *cl,
                                       uint16_t auth_id,
                                       uint32_t dpci_id)
{
	int i;
	
	for (i = 0; i < CMDIF_MN_SESSIONS; i++) {
		if ((cl->gpp[i].regs->peer_attr->peer_id == dpci_id) &&
			(cl->gpp[i].m_name[0] != CMDIF_FREE_SESSION) &&
			(cl->gpp[i].dev->auth_id == auth_id))	
			return i;
	}
	return -ENAVAIL;	
}

#endif /* __CMDIF_CLIENT_H */
