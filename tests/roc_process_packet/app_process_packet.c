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
#include "fsl_stdio.h"
#include "fsl_dpni_drv.h"
#include "fsl_ip.h"
#include "fsl_parser.h"
#include "fsl_general.h"
#include "fsl_cdma.h"
#include "fsl_l2.h"
#include "fsl_evmng.h"

#include "aiop_verification.h"
#include "fsl_slab.h"

int app_early_init(void);

int app_init(void);
void app_free(void);


static void app_process_packet_flow0 (void)
{
	int      err = 0;
	int local_test_error = 0;
	uint16_t ipv4hdr_length = 0;
	uint16_t ipv4hdr_offset = 0;
	uint16_t ni_id;
	uint8_t *p_ipv4hdr = 0;
	uint8_t *p_eth_hdr;
	uint32_t dst_addr = 0;// ipv4 dst_addr - will store original destination address
	uint8_t local_hw_addr[NET_HDR_FLD_ETH_ADDR_SIZE];
	struct ipv4hdr *ipv4header;

	if (PARSER_IS_OUTER_IPV4_DEFAULT())
	{
		fsl_print
		("app_process_packet: Received packet with ipv4 header:\n");
		ipv4hdr_offset = (uint16_t)PARSER_GET_OUTER_IP_OFFSET_DEFAULT();
		p_ipv4hdr = (uint8_t *) PRC_GET_SEGMENT_ADDRESS() + ipv4hdr_offset ;

		ipv4header = (struct ipv4hdr *)p_ipv4hdr;
		ipv4hdr_length = (uint16_t)((ipv4header->vsn_and_ihl & 0x0F) << 2);
		for( int i = 0; i < ipv4hdr_length; i++)
		{
			fsl_print(" %x",p_ipv4hdr[i]);
		}
		fsl_print("\n");

		dst_addr = ipv4header->dst_addr;
		/*for reflection when switching between src and dst IP the checksum remains the same*/
		err = ip_set_nw_dst(ipv4header->src_addr);
		if (err) {
			fsl_print("ERROR = %d: ip_set_nw_dst(src_addr)\n", err);
			local_test_error |= err;
	}
		err = ip_set_nw_src(dst_addr);
	if (err) {
			fsl_print("ERROR = %d: ip_set_nw_src(dst_addr)\n", err);
		local_test_error |= err;
	}
	}

	ni_id = (uint16_t)task_get_receive_niid();
	/*switch between src and dst MAC addresses*/
	p_eth_hdr = PARSER_GET_ETH_POINTER_DEFAULT();
	p_eth_hdr += NET_HDR_FLD_ETH_ADDR_SIZE;
	l2_set_dl_dst(p_eth_hdr);
	dpni_drv_get_primary_mac_addr(ni_id, local_hw_addr);
	l2_set_dl_src(local_hw_addr);

	if (!local_test_error && PARSER_IS_OUTER_IPV4_DEFAULT())
	{
		fsl_print
		("app_process_packet: Will send a modified packet with ipv4 header:\n");
		for( int i = 0; i < ipv4hdr_length; i++)
		{
			fsl_print(" %x",p_ipv4hdr[i]);
		}
		fsl_print("\n");
	}

	err = dpni_drv_send(ni_id, DPNI_DRV_SEND_MODE_NONE);
	if (err){
		fsl_print("ERROR = %d: dpni_drv_send(ni_id)\n",err);
		local_test_error |= err;
	}

	if(!local_test_error) /*No error found during injection of packets*/
		fsl_print("Finished SUCCESSFULLY\n");
	else
		fsl_print("Finished with ERRORS\n");

	fdma_terminate_task();
}


int app_early_init(void){

	int err;

	/* exists in aiop_sl_early_init */
	err = ipr_early_init(3, 750);
	
	if (err)
		return err;
	
	/* IPsec resources reservation */
	err = ipsec_early_init(
		10, /* uint32_t total_instance_num */
		20, /* uint32_t total_committed_sa_num */
		40, /* uint32_t total_max_sa_num */
		0  /* uint32_t flags */
	);

	if (err)
		return err;

	/* Reserve some general buffers for keys etc. */
	err = slab_register_context_buffer_requirements(
			10, /* uint32_t committed_buffs*/
			10, /* uint32_t max_buffs */
			512, /* uint16_t buff_size */
			8, /* uint16_t alignment */
			MEM_PART_SYSTEM_DDR, /* enum memory_partition_id  mem_pid */
	        0, /* uint32_t flags */
	        0 /* uint32_t num_ddr_pools */
	        );

	if (err)
		return err;

	/* Set DHR to 256 in the default storage profile, for IPsec */
	err = dpni_drv_register_rx_buffer_layout_requirements(256, 0, 0, 0);
	
	return err;
}

static int app_dpni_event_added_cb(
			uint8_t generator_id,
			uint8_t event_id,
			uint64_t app_ctx,
			void *event_data)
{
	uint16_t ni = (uint16_t)((uint32_t)event_data);
	int err;

	UNUSED(generator_id);
	UNUSED(event_id);
	pr_info("Event received for AIOP NI ID %d\n",ni);
	err = dpni_drv_register_rx_cb(ni/*ni_id*/,
	                              (rx_cb_t *)app_ctx);
	if (err){
		pr_err("dpni_drv_register_rx_cb for ni %d failed: %d\n", ni, err);
		return err;
	}
	err = dpni_drv_enable(ni);
	if(err){
		pr_err("dpni_drv_enable for ni %d failed: %d\n", ni, err);
		return err;
	}
	return 0;
}


int app_init(void)
{
	int        err  = 0;

	fsl_print("Running app_init()\n");

	err = evmng_register(EVMNG_GENERATOR_AIOPSL, DPNI_EVENT_ADDED, 1,(uint64_t) aiop_verification_fm, app_dpni_event_added_cb);
	if (err){
		pr_err("EVM registration for DPNI_EVENT_ADDED failed: %d\n", err);
		return err;
	}


	fsl_print("To start test inject packets: \"eth_ipv4_udp.pcap\"\n");

	return 0;
}

void app_free(void)
{
	/* free application resources*/
}
