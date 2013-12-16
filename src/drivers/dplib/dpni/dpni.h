/**************************************************************************//*
 @File          dpni.h

 @Description   DPNI FLib internal file

 @Cautions      None.
*//***************************************************************************/

#ifndef _DPNI_H
#define _DPNI_H

#include "common\fsl_cmdif.h"
#include "dplib\fsl_dpni.h"

/**************************************************************************//**
 @Description   FLib internal dataBase

                more_detailed_description
*//***************************************************************************/
struct dpni
{
	struct cmdif_dev	*dev;
};


void prepare_init_cmd(struct cmdif_cmd_desc	*desc,
                      const struct dpni_cfg *cfg,
                      const struct dpni_init_params *params);

void prepare_attach_cmd(struct cmdif_cmd_desc *desc, 
                        const struct dpni_attach_params *cfg);

void prepare_tx_tc_cmd(struct cmdif_cmd_desc *desc, 
                       const struct dpni_tx_tc_cfg *tc_cfg);

void prepare_rx_tc_cmd(struct cmdif_cmd_desc *desc, 
                       const struct dpni_rx_tc_cfg *tc_cfg);

void prepare_tx_q_cfg_cmd(struct cmdif_cmd_desc *desc, 
                          uint8_t tcid, 
                          const struct dpni_tx_queue_cfg *params);

void prepare_rx_flow_io_cmd(struct cmdif_cmd_desc *desc, 
                          uint8_t tcid,
                          uint8_t flowid,
                          const struct dpni_rx_flow_io *params);

void prepare_rx_flow_cfg_cmd(struct cmdif_cmd_desc *desc, 
                          uint8_t tcid,
                          uint8_t flowid,
                          const struct dpni_rx_flow_cfg *params);
#if 0
void recieve_get_cfg_cmd(struct cmdif_cmd_desc *desc, 
						 struct dpni_int_cfg *cfg);
#endif
void recieve_get_attr_cmd(struct cmdif_cmd_desc *desc, 
                          struct dpni_attributes *attr);

void recieve_get_stats_cmd(struct cmdif_cmd_desc *desc,
                           const struct dpni_stats *stats);

void recieve_get_qdid_cmd(struct cmdif_cmd_desc *desc, 
                          uint16_t		*qdid);

void recieve_get_tx_data_offset_cmd(struct cmdif_cmd_desc *desc, 
                                    uint16_t		  *offset);

void prepare_set_mfl_cmd(struct cmdif_cmd_desc *desc, uint16_t mfl);

void recieve_get_mfl_cmd(struct cmdif_cmd_desc *desc, 
			uint16_t		  *mfl);

void prepare_set_mtu_cmd(struct cmdif_cmd_desc *desc, uint16_t mtu);

void prepare_mcast_promisc_cmd(struct cmdif_cmd_desc *desc, int en);

void prepare_mod_prim_mac_cmd(struct cmdif_cmd_desc *desc, uint8_t addr[]);

void prepare_add_mac_cmd(struct cmdif_cmd_desc *desc, const uint8_t addr[]);

void prepare_remove_mac_cmd(struct cmdif_cmd_desc *desc, const uint8_t addr[]);

void prepare_add_vlan_id_cmd(struct cmdif_cmd_desc *desc, uint16_t vid);

void prepare_remove_vlan_id_cmd(struct cmdif_cmd_desc *desc, uint16_t vid);

void prepare_set_qos_tbl_cmd(struct cmdif_cmd_desc *desc, 
                             const struct dpni_qos_tbl_params *params);

void prepare_add_qos_entry_cmd(struct cmdif_cmd_desc *desc, 
                               const struct dpni_key_params *params, 
                               uint8_t tcid);

void prepare_remove_qos_entry_cmd(struct cmdif_cmd_desc *desc, 
                                  const struct dpni_key_params *params);

void prepare_set_dist_cmd(struct cmdif_cmd_desc *desc,
                          const struct dpni_dist_params *dist);

void prepare_set_fs_tbl_cmd(struct cmdif_cmd_desc 		*desc,
                            uint8_t 				tcid,
                            const struct dpni_fs_tbl_params 	*params);

void prepare_delete_fs_tbl_cmd(struct cmdif_cmd_desc *desc, uint8_t tcid);

void prepare_add_fs_entry_cmd(struct cmdif_cmd_desc *desc, 
                              uint8_t tcid,
                              const struct dpni_key_params *params,
                              uint16_t flowid);

void prepare_remove_fs_entry_cmd(struct cmdif_cmd_desc *desc, 
                                 uint8_t tcid, 
                                 const struct dpni_key_params *params);

void prepare_clear_fs_tbl_cmd(struct cmdif_cmd_desc *desc, uint8_t tcid);

void recieve_get_stats_cmd(struct cmdif_cmd_desc *desc, 
                           const struct dpni_stats *stats);

int dpni_modify_primary_mac_addr(struct dpni *dpni, uint8_t addr[]);



#endif /* _DPNI_H */
