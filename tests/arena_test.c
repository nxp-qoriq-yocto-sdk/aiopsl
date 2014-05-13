#include "common/types.h"
#include "common/fsl_stdio.h"
#include "common/fsl_string.h"
//#include "fsl_dpni.h"
//#include "dplib/dpni_drv.h"
#include "dpni/drv.h"
#include "fsl_fdma.h"
#include "general.h"
#include "fsl_ip.h"
#include "fsl_cdma.h"
#include "common/fsl_slab.h"
#include "kernel/platform.h"
#include "io.h"
#include "aiop_common.h"

int app_init(void);
void app_free(void);

#define APP_NI_GET(ARG)   ((uint16_t)((ARG) & 0x0000FFFF))
/**< Get NI from callback argument, it's demo specific macro */
#define APP_FLOW_GET(ARG) (((uint16_t)(((ARG) & 0xFFFF0000) >> 16)
/**< Get flow id from callback argument, it's demo specific macro */


extern int slab_init();
extern int malloc_test();
extern int slab_test();

extern __SHRAM struct slab *slab_peb;
extern __SHRAM struct slab *slab_ddr;



static void app_process_packet_flow0 (dpni_drv_app_arg_t arg)
{
	int      err = 0;
		
	err = slab_test();
	
	if (err) { 
			fsl_os_print("ERROR = %d: slab_test failed  in runtime phase \n", err);
		}
	err = malloc_test();
	
	if (err) { 
		fsl_os_print("ERROR = %d: malloc_test failed in runtime phase \n", err);
	}
	
	dpni_drv_send(APP_NI_GET(arg));
}

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

int app_init(void)
{
	int        err  = 0;
	uint32_t   ni   = 0;
	dma_addr_t buff = 0;
	

	fsl_os_print("Running app_init()\n");

	/* This is temporal WA for stand alone demo only */
	epid_setup();
	
	for (ni = 0; ni < 6; ni++)
	{
		/* Every ni will have 1 flow */
		uint32_t flow_id = 0;
		err = dpni_drv_register_rx_cb((uint16_t)ni/*ni_id*/,
		                              (uint16_t)flow_id/*flow_id*/,
		                              app_process_packet_flow0, /* callback for flow_id*/
		                              (ni | (flow_id << 16)) /*arg, nic number*/);
		if (err) return err;
	}
	
	err = slab_init();
	if (err) {
			fsl_os_print("ERROR = %d: slab_init failed  in init phase()\n", err);
	}
	err = malloc_test();
	if (err) {
		fsl_os_print("ERROR = %d: malloc_test failed in init phase()\n", err);
	} 		
	return 0;
}

void app_free(void)
{
	int err = 0;
	/* TODO - complete!*/
	err = slab_free(&slab_ddr);
	err = slab_free(&slab_peb);
}