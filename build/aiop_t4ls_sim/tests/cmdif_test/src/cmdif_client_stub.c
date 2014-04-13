#include "common/types.h"
#include "common/gen.h"
#include "common/errors.h"
#include "common/fsl_string.h"
#include "cmdif_srv.h"
#include "cmdif_client.h"
#include "general.h"
#include "fsl_ldpaa_aiop.h"
#include "io.h"
#include "fsl_fdma.h"
#include "sys.h"
#include "dbg.h"
#include "fsl_cdma.h"
#include "errors.h"

/* The purpose of this code is to modify the defaults FD in WS 
 * in order to test Server.
 * This means that cmd data and size are already set. In real world NADK API
 * will set it for us. 
 * This code also define the communication between client and Server through FD */

static void cmd_id_set(uint16_t cmd_id) 
{
	uint64_t data = LDPAA_FD_GET_FLC(HWC_FD_ADDRESS);
	data &= ~CMD_ID_MASK;
	data |= (((uint64_t)cmd_id) << CMD_ID_OFF);
	LDPAA_FD_SET_FLC(HWC_FD_ADDRESS, data);		
}

/** Module name is first 8 bytes inside data */
static void cmd_m_name_set(const char *name)
{
	uint8_t * addr = (uint8_t *)PRC_GET_SEGMENT_ADDRESS();
	addr += PRC_GET_SEGMENT_OFFSET();

	/* I expect that name will end by \0 if it has less than 8 chars */
	if (name != NULL) {
		if ((PRC_GET_SEGMENT_LENGTH() >= M_NAME_CHARS) &&
			(addr != NULL)) {
			strncpy((char *)addr, name, M_NAME_CHARS);
		}
	}
}

static void cmd_sync_addr_set(void *sync_addr)
{
	uint8_t * addr = (uint8_t *)PRC_GET_SEGMENT_ADDRESS();
	addr += PRC_GET_SEGMENT_OFFSET() + M_NAME_CHARS;

	/* Phys addr for cdma */
	*((uint64_t *)addr) = fsl_os_virt_to_phys(sync_addr); 
}

static void cmd_inst_id_set(uint8_t inst_id)
{
	uint32_t frc = LDPAA_FD_GET_FRC(HWC_FD_ADDRESS) & ~INST_ID_MASK;
	
	frc |= inst_id;	
	LDPAA_FD_SET_FRC(HWC_FD_ADDRESS, frc);	
}

static void cmd_auth_id_set(uint16_t auth_id)
{
	uint64_t data = 0;
	data = LDPAA_FD_GET_FLC(HWC_FD_ADDRESS) & ~AUTH_ID_MASK;
	data |= (((uint64_t)auth_id) << AUTH_ID_OFF);
	LDPAA_FD_SET_FLC(HWC_FD_ADDRESS, data);	
}

static void cmd_dev_set(void *dev)
{
	/* Virtual address for linux is 39 bit, we need only virtual addr */
	uint64_t data = 0;
	uint64_t dev_for_asynch = (uint64_t)dev;
	
	/* Low */
	LDPAA_FD_SET_FRC(HWC_FD_ADDRESS, ((uint32_t)dev_for_asynch));
	/* High */
	data = LDPAA_FD_GET_FLC(HWC_FD_ADDRESS) & ~DEV_H_MASK;
	data |=  (dev_for_asynch & 0x0000007F00000000) << DEV_H_OFF;
	LDPAA_FD_SET_FLC(HWC_FD_ADDRESS, data);		
}

/** These functions define how GPP should setup the FD 
 * It does not include data_addr and data_lenght because it's setup by NADK API
 * This code is used to modify the defaults FD inside WS */
void client_open_cmd(struct cmdif_desc *client, void *sync_done); 
void client_close_cmd(struct cmdif_desc *client); 
void client_sync_cmd(struct cmdif_desc *client); 
void client_async_cmd(struct cmdif_desc *client); 
void client_no_resp_cmd(struct cmdif_desc *client); 

void client_open_cmd(struct cmdif_desc *client, void *sync_done) 
{
	uint16_t   cmd_id = CMD_ID_OPEN;
	const char *module = "ABCABC";	
	cmd_id_set(cmd_id); 
	cmd_m_name_set(module);	
	cmd_sync_addr_set(sync_done);
	cmd_inst_id_set(3);
	cmd_auth_id_set(OPEN_AUTH_ID);
	((struct cmdif_dev *)client->dev)->sync_done = sync_done;
}

void client_close_cmd(struct cmdif_desc *client) 
{
	uint16_t cmd_id = CMD_ID_CLOSE;
	cmd_id_set(cmd_id);
	cmd_auth_id_set(((struct cmdif_dev *)client->dev)->auth_id); 
}

void client_sync_cmd(struct cmdif_desc *client) 
{
	uint16_t cmd_id = 0xCC; /* Any number */
	cmd_id_set(cmd_id); 
	cmd_auth_id_set(((struct cmdif_dev *)client->dev)->auth_id); 
}

void client_async_cmd(struct cmdif_desc *client) 
{
	uint16_t cmd_id = CMDIF_ASYNC_CMD | 0xA;
	cmd_id_set(cmd_id);
	cmd_auth_id_set(((struct cmdif_dev *)client->dev)->auth_id); 
	cmd_dev_set(client->dev);
}

void client_no_resp_cmd(struct cmdif_desc *client) 
{
	uint16_t cmd_id = CMDIF_NORESP_CMD | 0x2;
	cmd_id_set(cmd_id);
	cmd_auth_id_set(((struct cmdif_dev *)client->dev)->auth_id); 
}
