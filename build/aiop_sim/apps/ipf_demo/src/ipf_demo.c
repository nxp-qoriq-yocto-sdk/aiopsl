/*
 * Copyright 2014-2015 Freescale Semiconductor, Inc.
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

#include "common/types.h"
#include "common/fsl_stdio.h"
#include "fsl_dpni_drv.h"
#include "fsl_ip.h"
#include "platform.h"
#include "fsl_io.h"
#include "fsl_parser.h"
#include "general.h"
#include "fsl_dbg.h"
/*#include "fsl_cmdif_server.h"*/
#include "dplib/fsl_cdma.h"
#include "dplib/fsl_l2.h"

int app_init(void);
int app_early_init(void);
void app_free(void);

#define APP_NI_GET(ARG)   ((uint16_t)((ARG) & 0x0000FFFF))
/**< Get NI from callback argument, it's demo specific macro */
#define APP_FLOW_GET(ARG) (((uint16_t)(((ARG) & 0xFFFF0000) >> 16)
/**< Get flow id from callback argument, it's demo specific macro */

#define IPF_DEMO_WITH_HM	0x80
#define IPF_DEMO_WITHOUT_HM	0x00

__TASK ipf_ctx_t ipf_context_addr
	__attribute__((aligned(sizeof(struct ldpaa_fd))));


__declspec(entry_point) static void app_process_packet_flow0 (void)
{
	int      err = 0;
	const uint16_t ipv4hdr_length = sizeof(struct ipv4hdr);
	uint16_t ipv4hdr_offset = 0;
	uint8_t *p_ipv4hdr = 0;
	struct ipv4hdr *ipv4_hdr;

	uint8_t ipf_demo_flags = IPF_DEMO_WITH_HM;
	uint32_t vlan_tag1 = 0x81008a6b;
	uint32_t vlan_tag2 = 0x8100c78d;
	uint16_t mtu;
	int ipf_status;
	int local_test_error = 0;
	uint32_t fd_length;
	uint16_t offset;

/*	ipf_ctx_t ipf_context_addr __attribute__((aligned(sizeof(struct ldpaa_fd))));*/

	sl_prolog();

	mtu = 1500;

	if (PARSER_IS_OUTER_IPV4_DEFAULT())
	{
		fsl_os_print
		("ipf_demo:Core %d received packet with ipv4 header:\n",
	    core_get_id());
		ipv4hdr_offset = (uint16_t)PARSER_GET_OUTER_IP_OFFSET_DEFAULT();
		p_ipv4hdr = UINT_TO_PTR((ipv4hdr_offset + PRC_GET_SEGMENT_ADDRESS()));
		for( int i = 0; i < ipv4hdr_length ;i ++)
		{
			fsl_os_print(" %x",p_ipv4hdr[i]);
		}
		fsl_os_print("\n");
	}

	if (ipf_demo_flags == IPF_DEMO_WITH_HM)
	{
		l2_push_and_set_vlan(vlan_tag1);
		fsl_os_print
			("ipr_demo: Core %d inserted vlan 0x%x to frame\n",
				core_get_id(), vlan_tag1);
	}

	ipf_context_init(0, mtu, ipf_context_addr);
	fsl_os_print("ipf_demo: ipf_context_init done, MTU = %d\n", mtu);

	do {
		ipf_status = ipf_generate_frag(ipf_context_addr);

		if (ipf_demo_flags == IPF_DEMO_WITH_HM)
		{
			l2_push_and_set_vlan(vlan_tag2);
			fsl_os_print
				("ipf_demo: Core %d inserted vlan 0x%x to fragment\n",
				core_get_id(), vlan_tag2);
		}

		if (ipf_status > 0){
			fsl_os_print
			("ipf_demo: Core %d will send a fragment with ipv4 header:\n"
				, core_get_id());
		
			ipv4hdr_offset = (uint16_t)PARSER_GET_OUTER_IP_OFFSET_DEFAULT();
			p_ipv4hdr = UINT_TO_PTR((ipv4hdr_offset + PRC_GET_SEGMENT_ADDRESS()));

			for( int i = 0; i < ipv4hdr_length ;i ++)
			{
				fsl_os_print(" %x",p_ipv4hdr[i]);
			}
			fsl_os_print("\n");
			fd_length = LDPAA_FD_GET_LENGTH(HWC_FD_ADDRESS);
			if (fd_length != 1522)
			{
				fsl_os_print("fragment length error!\n");
				local_test_error |= 1;
			}
			ipv4_hdr = (struct ipv4hdr *)p_ipv4hdr;
			if (ipv4_hdr->id != 1)
			{
				fsl_os_print("fragment ID error!\n");
				local_test_error |= 1;
			}
			if (!(ipv4_hdr->flags_and_offset & 0x2000))
			{
				fsl_os_print("fragment flags error!\n");
				local_test_error |= 1;
			}
			offset = (ipv4_hdr->flags_and_offset) & ~0x2000;
			offset *= 8;
			if ((offset % 1480) != 0)
			{
				fsl_os_print("fragment offset error!\n");
				local_test_error |= 1;
			}
		}
		err = dpni_drv_send(dpni_get_receive_niid());
		if (err){
			fsl_os_print("ERROR = %d: dpni_drv_send()\n",err);
			local_test_error |= err;
			if(err == -ENOMEM)
			{
				fdma_discard_default_frame(FDMA_DIS_NO_FLAGS);
			}
			else /* (err == -EBUSY) */
				fdma_discard_fd((struct ldpaa_fd *)HWC_FD_ADDRESS, FDMA_DIS_NO_FLAGS);

			
			if (ipf_status == IPF_GEN_FRAG_STATUS_IN_PROCESS)
				ipf_discard_frame_remainder(ipf_context_addr);
			break;
		}
	} while (ipf_status != IPF_GEN_FRAG_STATUS_DONE);

	fsl_os_print
		("ipf_demo: Core %d will send last fragment with ipv4 header:\n"
			, core_get_id());
	for( int i = 0; i < ipv4hdr_length ;i ++)
	{
		fsl_os_print(" %x",p_ipv4hdr[i]);
	}
		fsl_os_print("\n");

	fd_length = LDPAA_FD_GET_LENGTH(HWC_FD_ADDRESS);
	if (fd_length != 73)
	{
		fsl_os_print("last fragment length error!\n");
		local_test_error |= 1;
	}
	ipv4_hdr = (struct ipv4hdr *)p_ipv4hdr;
	if (ipv4_hdr->id != 1)
	{
		fsl_os_print("fragment ID error!\n");
		local_test_error |= 1;
	}
	if (ipv4_hdr->flags_and_offset & 0x2000)
	{
		fsl_os_print("last fragment flags error!\n");
		local_test_error |= 1;
	}
	offset = (ipv4_hdr->flags_and_offset) & ~0x2000;
	offset *= 8;
	if ((offset % 1480) != 0)
	{
		fsl_os_print("last fragment offset error!\n");
		local_test_error |= 1;
	}

	if(!local_test_error) /*No error found during injection of packets*/
		fsl_os_print("Finished SUCCESSFULLY\n");
	else
		fsl_os_print("Finished with ERRORS\n");
	/*MUST call fdma_terminate task in the end of cb function*/
	fdma_terminate_task();
}


#ifdef AIOP_STANDALONE
/* This is temporal WA for stand alone demo only */
#define WRKS_REGS_GET \
	(sys_get_memory_mapped_module_base(FSL_OS_MOD_CMGW,            \
	                                   0,                          \
	                                   E_MAPPED_MEM_TYPE_GEN_REGS) \
	                                   + SOC_PERIPH_OFF_AIOP_WRKS);
static void epid_setup()
{
	struct aiop_ws_regs *wrks_addr = (struct aiop_ws_regs *)WRKS_REGS_GET;

	/* EPID = 0 is saved for cmdif, need to set it for stand alone demo */
	iowrite32(0, &wrks_addr->epas);
	iowrite32((uint32_t)receive_cb, &wrks_addr->ep_pc);
}
#endif /* AIOP_STANDALONE */

/*static int open_cb(uint8_t instance_id, void **dev)
{
	UNUSED(dev);
	fsl_os_print("open_cb inst_id = 0x%x\n", instance_id);
	return 0;
}

static int close_cb(void *dev)
{
	UNUSED(dev);
	fsl_os_print("close_cb\n");
	return 0;
}

static int ctrl_cb(void *dev, uint16_t cmd, uint32_t size, uint64_t data)
{
	UNUSED(dev);
	UNUSED(size);
	UNUSED(data);
	fsl_os_print("ctrl_cb cmd = 0x%x, size = %d, data high= 0x%x data low= 0x%x\n",
	             cmd,
	             size,
	             (uint32_t)((data & 0xFF00000000) >> 32),
	             (uint32_t)(data & 0xFFFFFFFF));
	return 0;
}

static struct cmdif_module_ops ops = {
                               .open_cb = open_cb,
                               .close_cb = close_cb,
                               .ctrl_cb = ctrl_cb
};
*/
int app_init(void)
{
	int        err  = 0;
	uint32_t   ni   = 0;
	dma_addr_t buff = 0;

	uint16_t    mfl = 0x2000; /* Maximum Frame Length */

	fsl_os_print("Running app_init()\n");

#ifdef AIOP_STANDALONE
	/* This is temporal WA for stand alone demo only */
	epid_setup();
#endif /* AIOP_STANDALONE */

	for (ni = 0; ni < dpni_drv_get_num_of_nis(); ni++)
	{
		err = dpni_drv_register_rx_cb((uint16_t)ni /*ni_id*/,
		                              app_process_packet_flow0 /* callback */);
		if (err) return err;

		err = dpni_drv_set_max_frame_length((uint16_t)ni/*ni_id*/,
		                        mfl /* Max frame length*/);
		if (err) return err;

	}

/*
	err = cmdif_register_module("TEST0", &ops);
	if (err)
		fsl_os_print("FAILED cmdif_register_module\n!");
*/
	fsl_os_print("To start test inject packets: \"reassembled_frame.pcap\" after AIOP boot complete.\n");
	return 0;
}

int app_early_init(void)
{
	/* Early initialization */
	return 0;
}

void app_free(void)
{
	/* TODO - complete!*/
}
