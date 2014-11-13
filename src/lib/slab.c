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

#include "common/types.h"
#include "fsl_errors.h"
#include "common/fsl_string.h"
#include "fsl_malloc.h"
#include "kernel/fsl_spinlock.h"
#include "dplib/fsl_dprc.h"
#include "dplib/fsl_dpbp.h"
#include "fsl_dbg.h"
#include "slab.h"
#include "fdma.h"
#include "fsl_io.h"
#include "fsl_cdma.h"
#include "fsl_io_ccsr.h"
#include "aiop_common.h"
#include "fsl_mc_init.h"

struct slab_bman_pool_desc g_slab_bman_pools[SLAB_MAX_BMAN_POOLS_NUM];
struct slab_virtual_pools_main_desc g_slab_virtual_pools;

uint64_t g_slab_pool_pointer_ddr;
uint64_t g_slab_last_pool_pointer_ddr;

struct memory_types_table *g_slab_early_init_data;

#define SLAB_ASSERT_COND_RETURN(COND, ERR)  \
	do { if (!(COND)) return (ERR); } while (0)

#define FOUND_SMALLER_SIZE(A, B) \
	slab_m->hw_pools[(A)].buff_size > slab_m->hw_pools[(B)].buff_size

#define CP_POOL_DATA(MOD, INFO, I) \
	{                                                              \
		if (MOD->hw_pools[I].pool_id == INFO->pool_id) {       \
			INFO->buff_size  = MOD->hw_pools[I].buff_size; \
			INFO->alignment  = MOD->hw_pools[I].alignment; \
			INFO->mem_pid    = MOD->hw_pools[I].mem_pid;   \
			return 0;                                      \
		}                                                      \
	}

int slab_module_early_init(void);
/***************************************************************************
 * slab_read_pool used by: slab_debug_info_get
 ***************************************************************************/
static int slab_read_pool(uint32_t slab_pool_id,
                                  uint16_t *bman_pool_id,
                                  int32_t *max_bufs,
                                  int32_t *committed_bufs,
                                  int32_t *allocated_bufs,
                                  uint8_t *flags,
                                  slab_release_cb_t **callback_func)
{

	uint16_t cluster =SLAB_CLUSTER_ID_GET(slab_pool_id);
	struct slab_v_pool *slab_virtual_pool;
	struct slab_v_pool slab_virtual_pool_ddr;
	uint32_t pool_id = SLAB_POOL_ID_GET(slab_pool_id); /*fetch pool id*/
	uint64_t pool_data_address;

	pr_info("pool ID %d, Cluster %d\n", pool_id, cluster);
	if(cluster == 0){
		slab_virtual_pool = (struct slab_v_pool *)
					g_slab_virtual_pools.virtual_pool_struct;

		slab_virtual_pool += pool_id;
	}

	else {

		pool_data_address = g_slab_virtual_pools.slab_context_address[cluster] +
					(sizeof(slab_virtual_pool_ddr) *
						pool_id);
		cdma_read(  &slab_virtual_pool_ddr,
		            pool_data_address,
		            (uint16_t)sizeof(slab_virtual_pool_ddr));
		slab_virtual_pool = &slab_virtual_pool_ddr;
	}


	*max_bufs = slab_virtual_pool->max_bufs;
	*committed_bufs = slab_virtual_pool->committed_bufs;
	*allocated_bufs = slab_virtual_pool->allocated_bufs;
	*bman_pool_id =
		g_slab_bman_pools[slab_virtual_pool->bman_array_index].bman_pool_id;

	pr_info("max buffs %d, committed %d, allocated %d\n", *max_bufs, *committed_bufs, *allocated_bufs);
	*flags = slab_virtual_pool->flags;

	*callback_func = slab_virtual_pool->callback_func;
	return 0;
} /* slab_read_virtual_pool */

/***************************************************************************
 * slab_pool_init used by: slab_module_init
 ***************************************************************************/
static void slab_pool_init(
	struct slab_v_pool *virtual_pool_struct,
	uint16_t num_of_virtual_pools,
	uint8_t flags)
{
	int i;

	struct slab_v_pool *slab_virtual_pool;

	/* Mask when down-casting.
	 *  Currently the address is only in Shared RAM (32 bit)
	 */
	slab_virtual_pool =
		(struct slab_v_pool *)virtual_pool_struct ;

	g_slab_virtual_pools.virtual_pool_struct = virtual_pool_struct;
	g_slab_virtual_pools.shram_count = 0;
	g_slab_virtual_pools.flags = flags;

	/* Init 'max' to zero, since it's an indicator to
	 * pool ID availability */
	for(i = 0; i < num_of_virtual_pools; i++) {
		slab_virtual_pool->max_bufs = 0;
		slab_virtual_pool->spinlock = 0; /* clear spinlock indicator */
		slab_virtual_pool++; /* increment the pointer */
	}

	
	for (i = 0; i< SLAB_MAX_BMAN_POOLS_NUM; i++) {
		g_slab_bman_pools[i].remaining = -1;
		/* clear spinlock indicator */
		g_slab_bman_pools[i].spinlock = 0;
	}
	
	for (i = 0; i <= g_slab_virtual_pools.num_clusters ; i++) /*number of clusters: 1 for SHRAM and 100 for DDR*/
		g_slab_virtual_pools.slab_context_address[i] = 0;

} /* End of vpool_init */

/*****************************************************************************/

static int slab_add_bman_buffs_to_pool(
	uint16_t bman_pool_id,
	int32_t additional_bufs)
{
	uint16_t  bman_array_index = 0;

#ifdef DEBUG
	/* Check the arguments correctness */
	if (bman_pool_id >= SLAB_MAX_BMAN_POOLS_NUM)
		return -EINVAL;
#endif

	/* Check which BMAN pool ID array element matches the ID */
	for (bman_array_index = 0; bman_array_index < SLAB_MAX_BMAN_POOLS_NUM; bman_array_index++) {
		if (g_slab_bman_pools[bman_array_index].bman_pool_id == bman_pool_id) {
			break;
		}
	}


	/* Check the arguments correctness */
	if (bman_array_index == SLAB_MAX_BMAN_POOLS_NUM)
		return -EINVAL;

	/* Increment the total available BMAN pool buffers */
	atomic_incr32(&g_slab_bman_pools[bman_array_index].remaining,
	              additional_bufs);

	return 0;
} /* End of vpool_add_total_bman_bufs */

/*****************************************************************************/

static int slab_decr_bman_buffs_from_pool(
	uint16_t bman_pool_id,
	int32_t less_bufs)
{

	int i;
	uint16_t bman_array_index = 0;

#ifdef DEBUG
	/* Check the arguments correctness */
	if (bman_pool_id >= SLAB_MAX_BMAN_POOLS_NUM)
		return -EINVAL;
#endif

	/* Check which BMAN pool ID array element matches the ID */
	for (i=0; i< SLAB_MAX_BMAN_POOLS_NUM; i++) {
		if (g_slab_bman_pools[i].bman_pool_id == bman_pool_id) {
			bman_array_index = (uint16_t)i;
			break;
		}
	}

	lock_spinlock(
		(uint8_t *)&g_slab_bman_pools[bman_array_index].spinlock);

	/* Check if there are enough buffers to reserve */
	if (g_slab_bman_pools[bman_array_index].remaining >= less_bufs) {
		/* decrement the BMAN pool buffers */
		g_slab_bman_pools[bman_array_index].remaining -=
			less_bufs;

		unlock_spinlock((uint8_t *)
		                &g_slab_bman_pools[bman_array_index].spinlock);

	} else {
		unlock_spinlock((uint8_t *)
		                &g_slab_bman_pools[bman_array_index].spinlock);
		return -ENOMEM;
	}

	return 0;
} /* End of vpool_decr_bman_bufs */




static void free_buffs_from_bman_pool(uint16_t bpid, int32_t num_buffs,
                                      uint16_t icid, uint32_t flags)
{
	int      i;
	uint64_t addr = 0;


	for (i = 0; i < num_buffs; i++) {
		fdma_acquire_buffer(icid, flags, bpid, &addr);
	}
}


/*****************************************************************************/
int slab_find_and_unreserve_bpid(int32_t num_buffs,
                            uint16_t bpid)
{
	int error = 0;
	error = slab_add_bman_buffs_to_pool(bpid, num_buffs);
	if(error)
		return error;
	
	return 0;
}

/*****************************************************************************/
int slab_find_and_reserve_bpid(uint32_t num_buffs,
                               uint16_t buff_size,
                               uint16_t alignment,
                               enum memory_partition_id  mem_pid,
                               uint16_t *bpid_array_index,
                               uint16_t *bpid)
{
	int        error = 0, i, found = FALSE;
	uint16_t bman_array_index;

	struct slab_module_info *slab_m = \
		sys_get_unique_handle(FSL_OS_MOD_SLAB);

	if (slab_m == NULL)
		return -EINVAL;

	for (i = 0; i < slab_m->num_hw_pools; i++)
	{
		if ((slab_m->hw_pools[i].mem_pid == mem_pid)         &&
			(slab_m->hw_pools[i].alignment >= alignment) &&
			(SLAB_SIZE_GET(slab_m->hw_pools[i].buff_size) >= buff_size))
		{
			/*First find the first relevant
			 * Lock
			 * check if there is enough buffers
			 * acquire
			 * unlock
			 * if lower found, lock acquire return*/


			lock_spinlock((uint8_t *)&g_slab_bman_pools[i].spinlock);
			if (g_slab_bman_pools[i].remaining >= num_buffs)
			{
				if(!found) { /*found  = FALSE in the first time */
					/* decrement the total available BMAN pool buffers */
					g_slab_bman_pools[i].remaining -= num_buffs;
					found = TRUE;
					bman_array_index = (uint16_t)i;
				}
				else if(FOUND_SMALLER_SIZE(bman_array_index, i))
				{
					g_slab_bman_pools[i].remaining -= num_buffs;
					atomic_incr32(&g_slab_bman_pools[bman_array_index].remaining,
										(int32_t)num_buffs);
					bman_array_index = (uint16_t)i;

				}
			}
			unlock_spinlock((uint8_t *) &g_slab_bman_pools[i].spinlock);

		}
	} /*for (i = 0; i < num_bpids; i++)*/

	if(!found)
		return -ENOSPC;

	*bpid = slab_m->hw_pools[bman_array_index].pool_id;
	if(bpid_array_index != NULL)
		*bpid_array_index = bman_array_index;
	return 0;
}

/*****************************************************************************/
static void free_slab_module_memory(struct slab_module_info *slab_m)
{
	/* TODO there still some static allocations in VP init
	 * need to add them to slab_module_init() and then free them here
	 */
	int i;
	/* TODO - cluster free support needed*/
	for(i = (int)g_slab_virtual_pools.num_clusters ; i > 0; i-- )
	if (g_slab_virtual_pools.slab_context_address[i])
		cdma_refcount_decrement_and_release(g_slab_virtual_pools.slab_context_address[i]);
	fsl_os_xfree(g_slab_virtual_pools.slab_context_address);
	if (g_slab_virtual_pools.virtual_pool_struct)
		fsl_os_xfree(g_slab_virtual_pools.virtual_pool_struct);
	if (slab_m->hw_pools)
		fsl_os_xfree(slab_m->hw_pools);
	fsl_os_xfree(slab_m);
}
/*****************************************************************************/
#ifdef DEBUG
static inline int sanity_check_slab_create(uint16_t    buff_size,
                                           uint16_t    alignment,
                                           enum memory_partition_id  mem_pid,
                                           uint32_t    flags)
{
	SLAB_ASSERT_COND_RETURN(buff_size > 0,   -EINVAL);
	SLAB_ASSERT_COND_RETURN(alignment > 0,   -EINVAL);
	/* TODO need to support more then 8, align all to 64 bytes */
	SLAB_ASSERT_COND_RETURN(alignment <= 8,  -EINVAL);
	SLAB_ASSERT_COND_RETURN(flags == 0 || flags == SLAB_DDR_MANAGEMENT_FLAG,
	                        -EINVAL);
	SLAB_ASSERT_COND_RETURN(is_power_of_2(alignment), -EINVAL);
	SLAB_ASSERT_COND_RETURN(((mem_pid == MEM_PART_DP_DDR) ||
		(mem_pid == MEM_PART_SYSTEM_DDR) ||
		(mem_pid == MEM_PART_PEB)), -EINVAL);
	return 0;
}
#endif
/*****************************************************************************/
int slab_create(uint32_t    committed_buffs,
                uint32_t    max_buffs,
                uint16_t    buff_size,
                uint16_t    alignment,
                enum memory_partition_id  mem_pid,
                uint32_t    flags,
                slab_release_cb_t *release_cb,
                struct slab **slab)
{
	int      error = 0;
	uint16_t cluster;
	uint32_t pool_id;
	uint16_t bman_array_index, bpid;
	uint64_t slab_pool_pointer_ddr;

	int found = FALSE;
	struct slab_v_pool *slab_virtual_pool;
	struct slab_module_info *slab_m;
	struct slab_v_pool slab_virtual_pool_ddr;
	uint64_t context_address;

#ifdef DEBUG
	/* Sanity checks */
	error = sanity_check_slab_create(buff_size,
	                                 alignment,
	                                 mem_pid,
	                                 flags);
	if (error)
		return -ENAVAIL;

	if (max_buffs < committed_buffs)
		return -EINVAL;

	/* committed_bufs and max_bufs must not be 0 */
	if (!max_buffs)
		return -EINVAL;
#endif

	*((uint32_t *)slab) = 0;
	/* Only HW SLAB is supported */

	/*Find hardware pool with enough space*/
	error = slab_find_and_reserve_bpid(committed_buffs, buff_size, alignment,
	                           mem_pid, &bman_array_index, &bpid);
	if (error)
		return error;


/*********************found the right bpid for future virtual pool*************************/

	lock_spinlock((uint8_t *)&g_slab_virtual_pools.global_spinlock);
	/*
	 * Allocate pool
	 * Return with error if it was not possible to allocate pool.
	 * Find cluster with space for new pool metadatat
	 * */
	if(((flags & SLAB_DDR_MANAGEMENT_FLAG) == 0) && (g_slab_virtual_pools.shram_count < SLAB_MAX_NUM_VP_SHRAM)){
		/*use SHRAM for virtual pools*/
		slab_virtual_pool = (struct slab_v_pool *)
					g_slab_virtual_pools.virtual_pool_struct;
		for(pool_id = 0; pool_id < SLAB_MAX_NUM_VP_SHRAM; pool_id++) {
			if(slab_virtual_pool->max_bufs == 0) {
				/* use max_bufs as indicator */
				slab_virtual_pool->max_bufs = (int32_t)max_buffs;
				g_slab_virtual_pools.shram_count ++;
				found = TRUE;
				break;
			}
			slab_virtual_pool++; /* increment the pointer for slab virtual pull */
		}
		if(found){
			unlock_spinlock((uint8_t *)&g_slab_virtual_pools.global_spinlock);

			slab_virtual_pool->committed_bufs = (int32_t)committed_buffs;
			slab_virtual_pool->allocated_bufs = 0;
			slab_virtual_pool->bman_array_index = bman_array_index;
			slab_virtual_pool->flags = (uint8_t)flags;
			slab_virtual_pool->callback_func = release_cb;
			*((uint32_t *)slab) = SLAB_HW_POOL_CREATE(pool_id); /*the cluster is 0, no need to write it in slab id*/
			return 0;
		}
	}

	unlock_spinlock((uint8_t *)&g_slab_virtual_pools.global_spinlock);
	
	if((flags & SLAB_DDR_MANAGEMENT_FLAG) == 0){ /*Check if DDR management flag is on*/
		atomic_incr32(&g_slab_bman_pools[bman_array_index].remaining,
			              (int32_t)committed_buffs); /*return hardware pools*/
		return -ENOMEM;
	}

/***********************************************************************************************************************************************/
	/*try create virtual pool management in DDR*/
	/*search for free cluster which use DDR*/
	cdma_mutex_lock_take(g_slab_last_pool_pointer_ddr,
				                     CDMA_MUTEX_WRITE_LOCK);


	slab_pool_pointer_ddr = g_slab_pool_pointer_ddr;

	if(slab_pool_pointer_ddr == g_slab_last_pool_pointer_ddr){ /*No free virtual pool found*/
			atomic_incr32(&g_slab_bman_pools[bman_array_index].remaining,
			              (int32_t)committed_buffs);
			cdma_mutex_lock_release(g_slab_last_pool_pointer_ddr);
			return -ENOMEM;
	}

	g_slab_pool_pointer_ddr += sizeof(pool_id);

	slab_m = sys_get_unique_handle(FSL_OS_MOD_SLAB);
	fdma_dma_data(sizeof(pool_id),
	              slab_m->icid,
	              &pool_id,
	              slab_pool_pointer_ddr,
	              (slab_m->fdma_dma_flags |
	        	      FDMA_DMA_DA_SYS_TO_WS_BIT));

	pool_id = SLAB_VP_POOL_GET(pool_id);
	cluster = SLAB_CLUSTER_ID_GET(pool_id);
	pool_id = SLAB_POOL_ID_GET(pool_id);


	if(g_slab_virtual_pools.slab_context_address[cluster] == NULL) //goto new_buffer_allocation;
	{
		/*new_buffer_allocation*/
		if(slab_find_and_reserve_bpid(SLAB_BUFFER_TO_MANAGE_IN_DDR,
		                              sizeof(slab_virtual_pool_ddr) * SLAB_MAX_NUM_VP_DDR,
		                              SLAB_DEFAULT_ALIGN,
		                              MEM_PART_DP_DDR,
		                              NULL,
		                              &bpid) !=0 )
			/*recovery if bpid not found do create management structure for 64 more pools*/
			goto error_recovery_return;


		error = (int32_t)cdma_acquire_context_memory(bpid, &context_address);
		
		if(error){ /*No buffer found to increase management struct for more pool allocations*/
			slab_find_and_unreserve_bpid(SLAB_BUFFER_TO_MANAGE_IN_DDR,bpid);
			goto error_recovery_return;
		}
		g_slab_virtual_pools.slab_context_address[cluster] = context_address;
	}
	else
		context_address = g_slab_virtual_pools.slab_context_address[cluster];

	slab_virtual_pool_ddr.max_bufs = (int32_t)max_buffs;
	slab_virtual_pool_ddr.committed_bufs = (int32_t)committed_buffs;
	slab_virtual_pool_ddr.allocated_bufs = 0;
	slab_virtual_pool_ddr.spinlock = 0;
	slab_virtual_pool_ddr.bman_array_index = bman_array_index;
	slab_virtual_pool_ddr.flags = (uint8_t)flags | SLAB_DDR_MANAGEMENT_FLAG;
	slab_virtual_pool_ddr.callback_func = release_cb;
	/* Check if a callback_func exists*/
	
	context_address += (sizeof(slab_virtual_pool_ddr) * pool_id);

	cdma_write(
		context_address,
		/* uint64_t ext_address */
		&slab_virtual_pool_ddr,
		/* void *ws_dst */
		(uint16_t)sizeof(slab_virtual_pool_ddr)
		/* uint16_t size */
		);
	
	cdma_mutex_lock_release(g_slab_last_pool_pointer_ddr);

	pool_id = SLAB_CLUSTER_ID_SET(cluster) | pool_id;
	/* Return the ID (cluster and pool_id) */
	*((uint32_t *)slab) = SLAB_HW_POOL_CREATE(pool_id);

	return 0;
		

error_recovery_return:

	atomic_incr32(&g_slab_bman_pools[bman_array_index].remaining,
	              (int32_t)committed_buffs); /*return hardware pools*/
	
	g_slab_pool_pointer_ddr -= sizeof(pool_id);
	cdma_mutex_lock_release((uint64_t)&g_slab_last_pool_pointer_ddr);
	return -ENOMEM;

}

/*****************************************************************************/
int slab_free(struct slab **slab)
{
	uint32_t pool_id =  SLAB_VP_POOL_GET(*slab);
	uint16_t cluster = SLAB_CLUSTER_ID_GET(pool_id);
	
	struct slab_v_pool *slab_virtual_pool;
	struct slab_v_pool slab_virtual_pool_ddr;
	uint64_t pool_data_address;
	struct slab_module_info *slab_m = sys_get_unique_handle(FSL_OS_MOD_SLAB);

#ifdef DEBUG
	if (!SLAB_IS_HW_POOL(*slab))
		return -EINVAL;
#endif
	
	pool_id = SLAB_POOL_ID_GET(pool_id); /*take only the pool id without the cluster id bits*/
	if(cluster == 0){
		lock_spinlock((uint8_t *)&g_slab_virtual_pools.global_spinlock);
		slab_virtual_pool = (struct slab_v_pool *)
						g_slab_virtual_pools.virtual_pool_struct;
		slab_virtual_pool += pool_id;
		if (slab_virtual_pool->allocated_bufs != 0) {
			unlock_spinlock((uint8_t *)&g_slab_virtual_pools.global_spinlock);
			return -EACCES;	

		}
		/* Increment the total available BMAN pool buffers */
		atomic_incr32(&g_slab_bman_pools[slab_virtual_pool->bman_array_index].remaining,
					slab_virtual_pool->committed_bufs);
		slab_virtual_pool->max_bufs = 0;
		slab_virtual_pool->callback_func = NULL;
		g_slab_virtual_pools.shram_count --;
		unlock_spinlock((uint8_t *)&g_slab_virtual_pools.global_spinlock);
	}
	else{
		pool_data_address = g_slab_virtual_pools.slab_context_address[cluster] +
			(sizeof(slab_virtual_pool_ddr) *
				pool_id);
		cdma_read_with_mutex(pool_data_address,
		                     CDMA_PREDMA_MUTEX_WRITE_LOCK,
		                     &slab_virtual_pool_ddr,
		                     sizeof(slab_virtual_pool_ddr));




		if (slab_virtual_pool_ddr.allocated_bufs != 0) {
			cdma_mutex_lock_release(pool_data_address);
			return -EACCES;
		}


		/* max_bufs = 0 indicates a free pool */
		slab_virtual_pool_ddr.max_bufs = 0;
		slab_virtual_pool_ddr.callback_func = NULL;

		cdma_write_with_mutex(pool_data_address,
		                      CDMA_POSTDMA_MUTEX_RM_BIT,
		                      &slab_virtual_pool_ddr,
		                      sizeof(slab_virtual_pool_ddr));

		cdma_mutex_lock_take(g_slab_last_pool_pointer_ddr,
		                     CDMA_MUTEX_WRITE_LOCK);

		g_slab_pool_pointer_ddr -= sizeof(pool_id);
		pool_id = SLAB_CLUSTER_ID_SET(cluster) | pool_id;
		pool_id = SLAB_HW_POOL_CREATE(pool_id);
		fdma_dma_data(sizeof(pool_id),
		              slab_m->icid,
		              &pool_id,
		              g_slab_pool_pointer_ddr,
		              (slab_m->fdma_dma_flags |
		        	      FDMA_DMA_DA_WS_TO_SYS_BIT));

		/* Increment the total available BMAN pool buffers */
		atomic_incr32(
			&g_slab_bman_pools[slab_virtual_pool_ddr.bman_array_index].remaining,
			slab_virtual_pool_ddr.committed_bufs);

		cdma_mutex_lock_release(g_slab_last_pool_pointer_ddr);

	}
	*((uint32_t *)slab) = 0; /**< Delete all pool information */
	return 0;
}

/*****************************************************************************/
int slab_acquire(struct slab *slab, uint64_t *buff)
{
	int return_val;
	int allocate = 0;
	uint64_t pool_data_address;
	uint32_t slab_pool_id = SLAB_VP_POOL_GET(slab);
	uint16_t cluster = SLAB_CLUSTER_ID_GET(slab_pool_id); /* fetch cluster ID*/

	struct slab_v_pool *slab_virtual_pool;
	struct slab_v_pool slab_virtual_pool_ddr;


#ifdef DEBUG
	SLAB_ASSERT_COND_RETURN(SLAB_IS_HW_POOL(slab), -EINVAL);
#endif

	slab_pool_id = SLAB_POOL_ID_GET(slab_pool_id); /*fetch pool id*/

	if(cluster == 0){
		slab_virtual_pool = (struct slab_v_pool *)
				g_slab_virtual_pools.virtual_pool_struct;



		slab_virtual_pool += slab_pool_id;

		lock_spinlock((uint8_t *)&slab_virtual_pool->spinlock);
	}

	else {
		pool_data_address = g_slab_virtual_pools.slab_context_address[cluster] +
			(sizeof(slab_virtual_pool_ddr) *
				slab_pool_id);
		cdma_read_with_mutex(pool_data_address,
		                     CDMA_PREDMA_MUTEX_WRITE_LOCK,
		                     &slab_virtual_pool_ddr,
		                     sizeof(slab_virtual_pool_ddr));
		slab_virtual_pool = &slab_virtual_pool_ddr;
	}

	/* First check if there are still available buffers
	 * in the VP committed area */
	if(slab_virtual_pool->allocated_bufs <
		slab_virtual_pool->committed_bufs) {
		allocate = 1; /* allocated from committed area */
		/* Else, check if there are still available buffers
		 * in the VP max-committed area */
	} else if (slab_virtual_pool->allocated_bufs < slab_virtual_pool->max_bufs) {
		/* There is still an extra space in the virtual pool,
		 * check BMAN pool */

		/* spinlock this BMAN pool counter */
		lock_spinlock((uint8_t *)&g_slab_bman_pools[slab_virtual_pool->
		                                            bman_array_index].spinlock);

		if ((g_slab_bman_pools[
		                       slab_virtual_pool->bman_array_index].remaining) > 0)
		{
			allocate = 2; /* allocated from remaining area */
			g_slab_bman_pools
			[slab_virtual_pool->bman_array_index].remaining--;
		}

		unlock_spinlock((uint8_t *)&g_slab_bman_pools[slab_virtual_pool->
		                                              bman_array_index].spinlock);
	}

	/* Request CDMA to allocate a buffer*/
	if (allocate) {
		slab_virtual_pool->allocated_bufs++;
		if(cluster == 0)
			unlock_spinlock((uint8_t *)&slab_virtual_pool->spinlock);
		else
			cdma_write_with_mutex(pool_data_address,
			                      CDMA_POSTDMA_MUTEX_RM_BIT,
			                      &slab_virtual_pool_ddr,
			                      sizeof(slab_virtual_pool_ddr));

		/* allocate a buffer with the CDMA */
		return_val = cdma_acquire_context_memory(
			(uint16_t)g_slab_bman_pools
			[slab_virtual_pool->bman_array_index].bman_pool_id,
			(uint64_t *)buff); /* context_memory */

		/* If allocation failed,
		 * undo the counters increment/decrement */
		if (return_val) {
			if(cluster == 0)
				atomic_decr32(&slab_virtual_pool->allocated_bufs, 1); /* slab_virtual_pool points to SHRAM */

			if (allocate == 2) /* only if it was allocated from
					the remaining area */
				atomic_incr32(&g_slab_bman_pools[slab_virtual_pool->
				                                 bman_array_index].remaining, 1);
			if(cluster > 0) /*If managed in DDR, the structure should be updated*/
			{
				cdma_read_with_mutex(pool_data_address,
				                     CDMA_PREDMA_MUTEX_WRITE_LOCK,
				                     &slab_virtual_pool_ddr,
				                     sizeof(slab_virtual_pool_ddr));
				slab_virtual_pool_ddr.allocated_bufs --;
				cdma_write_with_mutex(pool_data_address,
				                      CDMA_POSTDMA_MUTEX_RM_BIT,
				                      &slab_virtual_pool_ddr,
				                      sizeof(slab_virtual_pool_ddr));	
			}
			return (return_val);
		}
		return 0;
	} else {
		if(cluster == 0)
			unlock_spinlock((uint8_t *)&slab_virtual_pool->spinlock);
		else
			cdma_mutex_lock_release(pool_data_address);
		
		pr_err("No mem\n");
		return -ENOMEM;
	}
}

/*****************************************************************************/
/* Must be used only in DEBUG
 * Accessing DDR in runtime also fsl_os_phys_to_virt() is not optimized */
#ifdef DEBUG
static int slab_check_bpid(struct slab *slab, uint64_t buff)
{
	uint16_t bpid;
	uint16_t cluster;
	uint32_t meta_bpid = 0;
	int      err = -EFAULT;
	struct slab_module_info *slab_m=sys_get_unique_handle(FSL_OS_MOD_SLAB);
	meta_bpid = SLAB_POOL_ID_GET(SLAB_VP_POOL_GET(slab));
	struct slab_v_pool slab_virtual_pool_ddr;
	cluster = SLAB_CLUSTER_ID_GET(SLAB_VP_POOL_GET(slab));

	if(slab_m == NULL)
		return err;

	if(cluster == 0){
		bpid = g_slab_bman_pools[(g_slab_virtual_pools.virtual_pool_struct + meta_bpid)->bman_array_index].bman_pool_id;
	}
	else
	{
		cdma_read(&slab_virtual_pool_ddr,
		          g_slab_virtual_pools.slab_context_address[cluster] +
		          sizeof(slab_virtual_pool_ddr) * meta_bpid,
		          (uint16_t)  sizeof(slab_virtual_pool_ddr));
		bpid =g_slab_bman_pools[slab_virtual_pool_ddr.bman_array_index].bman_pool_id;

	}



	if (buff >= 8) {
		fdma_dma_data(4,
		              slab_m->icid,
		              &meta_bpid,
		              (buff - 4),
		              (slab_m->fdma_dma_flags |
		        	      FDMA_DMA_DA_SYS_TO_WS_BIT));
		if (bpid == (meta_bpid & 0x00003FFF))
			return 0;
	}

	return err;
}
#endif

/*****************************************************************************/
int slab_release(struct slab *slab, uint64_t buff)
{
	uint32_t slab_pool_id = SLAB_VP_POOL_GET(slab);
	uint16_t cluster = SLAB_CLUSTER_ID_GET(slab_pool_id);
	uint64_t pool_data_address;
	struct slab_v_pool *slab_virtual_pool;
	struct slab_v_pool slab_virtual_pool_ddr;

#ifdef DEBUG
	SLAB_ASSERT_COND_RETURN(SLAB_IS_HW_POOL(slab), -EINVAL);
	SLAB_ASSERT_COND_RETURN(slab_check_bpid(slab, buff) == 0, -EFAULT);
#endif
	slab_pool_id = SLAB_POOL_ID_GET(slab_pool_id); /*Fetch pool ID*/
	if(cluster == 0) {
		slab_virtual_pool = (struct slab_v_pool *)
				g_slab_virtual_pools.virtual_pool_struct;
		slab_virtual_pool += slab_pool_id;
	}

	else {
		pool_data_address = g_slab_virtual_pools.slab_context_address[cluster] +
			(sizeof(slab_virtual_pool_ddr) *
				slab_pool_id);
		cdma_read_with_mutex(pool_data_address,
		                     CDMA_PREDMA_MUTEX_WRITE_LOCK,
		                     &slab_virtual_pool_ddr,
		                     sizeof(slab_virtual_pool_ddr));
		slab_virtual_pool = &slab_virtual_pool_ddr;
	}

	if (slab_virtual_pool->callback_func != NULL)
		(slab_virtual_pool->callback_func)(buff);

	cdma_release_context_memory(buff);

	if(cluster == 0)
	{
		lock_spinlock((uint8_t *)&slab_virtual_pool->spinlock);

	/* First check if buffers were allocated from the common pool */
		if(slab_virtual_pool->allocated_bufs >
			slab_virtual_pool->committed_bufs)
			/* One buffer returns to the common pool */
			atomic_incr32(&g_slab_bman_pools
				      [slab_virtual_pool->bman_array_index].remaining, 1);

		slab_virtual_pool->allocated_bufs--;
	
		unlock_spinlock((uint8_t *)&slab_virtual_pool->spinlock);
	}
	else
	{

		if(slab_virtual_pool_ddr.allocated_bufs >
			slab_virtual_pool_ddr.committed_bufs)
			/* One buffer returns to the common pool */
			atomic_incr32(&g_slab_bman_pools
			              [slab_virtual_pool_ddr.bman_array_index].remaining, 1);

		slab_virtual_pool_ddr.allocated_bufs--;
		cdma_write_with_mutex(pool_data_address,
		                      CDMA_POSTDMA_MUTEX_RM_BIT,
		                      &slab_virtual_pool_ddr,
		                      sizeof(slab_virtual_pool_ddr));

	}
	return 0;
}

/*****************************************************************************/
static int dpbp_add(struct dprc_obj_desc *dev_desc,
                    struct slab_bpid_info *bpids_arr,
                    struct mc_dprc *dprc)
{
	int      dpbp_id  = dev_desc->id;
	int      err      = 0;
	uint16_t dpbp     = 0;
	struct dpbp_attr attr;


	if ((err = dpbp_open(&dprc->io, dpbp_id, &dpbp)) != 0) {
		pr_err("Failed to open DP-BP%d.\n", dpbp_id);
		return err;
	}

	if ((err = dpbp_enable(&dprc->io, dpbp)) != 0) {
		pr_err("Failed to enable DP-BP%d.\n", dpbp_id);
		return err;
	}

	if ((err = dpbp_get_attributes(&dprc->io, dpbp, &attr)) != 0) {
		pr_err("Failed to get attributes from DP-BP%d.\n", dpbp_id);
		return err;
	}


	pr_info("found DPBP ID: %d, with BPID %d\n",dpbp_id, attr.bpid);
	bpids_arr->bpid = attr.bpid; /*Update found BP-ID*/
	return 0;
}

/*****************************************************************************/
static int dpbp_discovery(struct slab_bpid_info *bpids_arr,
                          int *n_bpids, int add_flag)
{
	struct dprc_obj_desc dev_desc;
	int dev_count;
	int num_bpids = 0;
	int err = 0;
	int i = 0;
	struct mc_dprc *dprc = sys_get_unique_handle(FSL_OS_MOD_AIOP_RC);


	/*Calling MC to get bpid's*/
	if (dprc == NULL)
	{
		pr_err("Don't find AIOP root container \n");
		return -ENODEV;
	}


	if ((err = dprc_get_obj_count(&dprc->io, dprc->token, &dev_count)) != 0) {
		pr_err("Failed to get device count for AIOP RC auth_id = %d.\n",
		       dprc->token);
		return err;
	}


	for (i = 0; i < dev_count; i++) {
		dprc_get_obj(&dprc->io, dprc->token, i, &dev_desc);
		if (strcmp(dev_desc.type, "dpbp") == 0) {
			/* TODO: print conditionally based on log level */
			pr_info("Found First DPBP ID: %d, Skipping, will be used for frame buffers\n", dev_desc.id);
			num_bpids = 1;
			break;
		}
	}

	if(num_bpids != 1) { /*Check if first dpbp was found*/
		pr_err("DP-BP not found in the container.\n");
		return -ENODEV;
	}

	num_bpids = 0; /*for now we save the first dpbp for later use.*/
	/*Continue to search for dpbp's*/
	for (i = i+1; i < dev_count; i++) {
		dprc_get_obj(&dprc->io, dprc->token, i, &dev_desc);
		if (strcmp(dev_desc.type, "dpbp") == 0) {

			num_bpids++;
			if(add_flag == TRUE)
			{/*flag to run dpbp_add function*/
				err = dpbp_add(&dev_desc, bpids_arr,
				               dprc);
				if (err) {
					return err;
				}
				bpids_arr ++;
				
				if(num_bpids == *n_bpids)
					break;
			}
		}
	}

	if (num_bpids == 0) {
		pr_err("DP-BP not found in the container.\n");
		return -ENODEV;
	}
	
	if(add_flag == TRUE)/*flag to run dpbp_add function*/
		bpids_arr -= num_bpids;
	else
		*n_bpids = num_bpids;
	
	return 0;
}

static int slab_alocate_memory(int num_bpids, struct slab_module_info *slab_m, struct slab_bpid_info **bpids_arr)
{
	int i = 0, j = 0;
	int err = 0;
	dma_addr_t addr = 0;
	uint32_t *virtual_address;
	uint32_t buff_size;
	/* Set BPIDs */
	for(i = 0; i < num_bpids; i++)
	{		
		slab_m->hw_pools[i].alignment = (*bpids_arr)->alignment;
		slab_m->hw_pools[i].flags = 0;
		slab_m->hw_pools[i].mem_pid = (*bpids_arr)->mem_pid;
		slab_m->hw_pools[i].buff_size = SLAB_SIZE_SET((*bpids_arr)->size);
		slab_m->hw_pools[i].total_num_buffs = (*bpids_arr)->num_buffers;
		slab_m->hw_pools[i].pool_id = (*bpids_arr)->bpid;
		buff_size = slab_m->hw_pools[i].buff_size;
		for (j = 0; j< SLAB_MAX_BMAN_POOLS_NUM; j++) {
			/* check if virtual_bman_pools[i] is empty */
			if (g_slab_bman_pools[j].remaining == -1) {
				g_slab_bman_pools[j].bman_pool_id = slab_m->hw_pools[i].pool_id;
				g_slab_bman_pools[j].remaining = 0;
				break;
			}
		}
		if(j == SLAB_MAX_BMAN_POOLS_NUM){
			pr_err("Error during search for free pool\n");
			return -ENOMEM;
		}

		(*bpids_arr) ++; /*move array pointer after saving the bpid*/
		
		/*Big malloc allocation for all buffers in bpid*/
		virtual_address = (uint32_t *) fsl_os_xmalloc(
			(size_t)(buff_size * slab_m->hw_pools[i].total_num_buffs),
			slab_m->hw_pools[i].mem_pid,
			slab_m->hw_pools[i].alignment);

		if (virtual_address == NULL) {
			pr_err("Error, unable to allocate space for pools\n");
			return -ENOMEM;
		}

		addr = fsl_os_virt_to_phys((void *)virtual_address);
//		pr_info("Malloc phys address is: 0x%x,%x\n", (uint32_t)(addr >> 32) , (uint32_t)(addr ));

		/* Isolation is enabled */
		for(j = 0; j < slab_m->hw_pools[i].total_num_buffs; j++){
			fdma_release_buffer(slab_m->icid,
			                    slab_m->fdma_flags,
			                    slab_m->hw_pools[i].pool_id,
			                    addr);

			addr += buff_size;

		}

		err = slab_add_bman_buffs_to_pool(slab_m->hw_pools[i].pool_id,
		                                  (int32_t) slab_m->hw_pools[i].total_num_buffs);
		if(err){
			return -EINVAL;
		}

	}
	(*bpids_arr) -= num_bpids;
	fsl_os_xfree(*bpids_arr);
	return err;
}

int slab_module_early_init(void){
	int i = 0, err;
	pr_info("Initialize memory for App early requests from slab\n");
	g_slab_early_init_data = (struct memory_types_table *)
					fsl_os_xmalloc((sizeof(struct memory_types_table) ),
						SLAB_FAST_MEMORY,
						1);
	g_slab_early_init_data->num_ddr_pools = 0;
	for(i = 0; i < SLAB_NUM_MEM_PARTITIONS; i++)
		if(g_slab_early_init_data->mem_pid_buffer_request[i])
			g_slab_early_init_data->mem_pid_buffer_request[i] = NULL;
	err = slab_register_context_buffer_requirements(50,50,2000,64,MEM_PART_PEB,0, 0);
	err |= slab_register_context_buffer_requirements(50,100,2000,64,MEM_PART_SYSTEM_DDR,0, 0);
	err |= slab_register_context_buffer_requirements(50,100,2000,64,MEM_PART_DP_DDR,0, 0);

	/*err |= slab_register_context_buffer_requirements(0,1,500,32,MEM_PART_SYSTEM_DDR,0, 0);
	err |= slab_register_context_buffer_requirements(50,100,200,64,MEM_PART_DP_DDR,0, 10);
	err |= slab_register_context_buffer_requirements(50,100,700,256,MEM_PART_DP_DDR,0, 10);
	err |= slab_register_context_buffer_requirements(50,100,3000,16,MEM_PART_DP_DDR,0, 50);
	err |= slab_register_context_buffer_requirements(50,100,5000,64,MEM_PART_DP_DDR,0, 40);
	err |= slab_register_context_buffer_requirements(50,50,11000,64,MEM_PART_DP_DDR,0, 50);
	err |= slab_register_context_buffer_requirements(1,1,10000,64,MEM_PART_DP_DDR,0, 50);
	err |= slab_register_context_buffer_requirements(1,2,10000,16,MEM_PART_DP_DDR,0, 50);
	err |= slab_register_context_buffer_requirements(50,100,100,64,MEM_PART_CCSR,0, 0);
	err |= slab_register_context_buffer_requirements(50,100,200,64,MEM_PART_SYSTEM_DDR,0, 10);
	err |= slab_register_context_buffer_requirements(5,10,32000,19000,MEM_PART_SYSTEM_DDR,0, 10);
	err |= slab_register_context_buffer_requirements(0,1,32000,32768,MEM_PART_SYSTEM_DDR,0, 10);
	err |= slab_register_context_buffer_requirements(0,1,12000,32768,MEM_PART_SYSTEM_DDR,0, 10);
	err |= slab_register_context_buffer_requirements(50,100,3000,64,MEM_PART_SYSTEM_DDR,0, 0);
	err |= slab_register_context_buffer_requirements(50,100,5000,64,MEM_PART_SH_RAM,0, 0);
	err |= slab_register_context_buffer_requirements(50,100,2000,64,MEM_PART_SYSTEM_DDR,0, 0);
	err |= slab_register_context_buffer_requirements(50,50,200,64,MEM_PART_PEB,0, 0);
	err |= slab_register_context_buffer_requirements(0,2,1000,64,MEM_PART_PEB,0, 0);
	err |= slab_register_context_buffer_requirements(50,100,100,64,MEM_PART_CCSR,0, 0);
	err |= slab_register_context_buffer_requirements(0,0,100,64,MEM_PART_MC_PORTALS,0, 0);*/
	if(err){
		pr_err("Failed to register context buffers\n");
		return err;
	}
	return 0;
}

/*Method used to split the memory to buffer sizes using number of available bpids*/
static int slab_divide_memory_for_bpids(int available_bpids, 
                                        int requested_bpids, 
                                        enum memory_partition_id  mem_pid,
                                        struct   slab_bpid_info **bpids_arr)
{
	int j, first_found_flag = FALSE;
	struct request_table_info local_info[] = SLAB_BUFF_SIZES_ARR;
	int buffer_types_array_size = ARRAY_SIZE(local_info);
	uint32_t sum_all_requests = 0;
	
	UNUSED(requested_bpids);
	for(j = buffer_types_array_size -1; j >= 0 ; j--) /*search which memory partitions filled for buffers and calculate 
				the number of requests per partition and the total requests*/
	{
		/* in case someone requested only extra buffers from that type we will check max*/
		if(g_slab_early_init_data->mem_pid_buffer_request[mem_pid]->table_info[j].max > 0)
		{
			sum_all_requests += g_slab_early_init_data->mem_pid_buffer_request[mem_pid]->table_info[j].max;
		}
	}
	
	
	/* use greedy algorithm, for more then 3  available bpids,
	 * greedy algorithm provides better results in this case */
	
//	if (available_bpids == requested_bpids || available_bpids > 3)
//	{
	
		j = buffer_types_array_size -1;/*last array index*/
		if(available_bpids > 1)
		{
			for(; j >= 0 ; j--) /*search which sizes of buffers requested */
			{
				/* in case someone requested only extra buffers from that type we will check max*/
				if(g_slab_early_init_data->mem_pid_buffer_request[mem_pid]->table_info[j].max > 0)
				{
					(*bpids_arr)->alignment = g_slab_early_init_data->mem_pid_buffer_request[mem_pid]->table_info[j].alignment;
					(*bpids_arr)->mem_pid = mem_pid;
					(*bpids_arr)->size = g_slab_early_init_data->mem_pid_buffer_request[mem_pid]->table_info[j].buff_size;
					(*bpids_arr)->num_buffers = g_slab_early_init_data->mem_pid_buffer_request[mem_pid]->table_info[j].max;
					(*bpids_arr) ++;
					available_bpids --;
					if(available_bpids == 1){
						j --;
						break;
					}
				}
			}	
		}
		for(; j >= 0 ; j--) /*search which sizes of buffers requested */
		{
			if(g_slab_early_init_data->mem_pid_buffer_request[mem_pid]->table_info[j].max > 0 &&
				!first_found_flag)
			{
				(*bpids_arr)->alignment = g_slab_early_init_data->mem_pid_buffer_request[mem_pid]->table_info[j].alignment;
				(*bpids_arr)->mem_pid = mem_pid;
				(*bpids_arr)->size = g_slab_early_init_data->mem_pid_buffer_request[mem_pid]->table_info[j].buff_size;
				(*bpids_arr)->num_buffers = g_slab_early_init_data->mem_pid_buffer_request[mem_pid]->table_info[j].max;
				first_found_flag = TRUE;
				continue;
			}
			if(g_slab_early_init_data->mem_pid_buffer_request[mem_pid]->table_info[j].max > 0 &&
				first_found_flag)
			{
				(*bpids_arr)->num_buffers += g_slab_early_init_data->mem_pid_buffer_request[mem_pid]->table_info[j].max;
				if((*bpids_arr)->alignment < g_slab_early_init_data->mem_pid_buffer_request[mem_pid]->table_info[j].alignment)
					(*bpids_arr)->alignment = g_slab_early_init_data->mem_pid_buffer_request[mem_pid]->table_info[j].alignment;
			}
		}
		(*bpids_arr) ++;
//	}
//	else /* use algorithm built on difference calculation between buffer sizes*/
//	{
//		
//	}
	
	return 0;
}


static int slab_proccess_registered_requests(int *num_bpids, struct   slab_bpid_info **bpids_arr)
{
	int i, j, temp, err = 0;
	struct request_table_info   local_info[] = SLAB_BUFF_SIZES_ARR; /*sample table with all the buffer sizes to each memory*/
	int buffer_types_array_size = ARRAY_SIZE(local_info); /*give the number of different sizes for buffer available to each memory.*/
	int requested_bpids_per_partition[SLAB_NUM_MEM_PARTITIONS] = {0}; /*store number of requests for bpids to each partition */
	int total_requested_bpids = 0, minimum_needed_bpids = 0;/*minimum needed bpids to supply at least one for each memory*/
	int available_bpids_per_partition[SLAB_NUM_MEM_PARTITIONS] = {0}; /*store calculated number of available bpids per partition*/
	int reminder_bpids_calc_per_partition[SLAB_NUM_MEM_PARTITIONS] = {0}; /*reminder left after suppling bpids*/
	int maximum_requested_bpids = 0, maximum_requested_index = 0; /*maximum number of bpids requested for memory and the index of that memory*/
	int minimum_requested_bpids = 0, minimum_requested_index = 0; /*minimum number of bpids requested for memory and the index of that memory*/
	int available_bpids = *num_bpids; /*local number of available bpids for AIOP*/
	uint16_t bpid_id = 0;
	for(i = 0; i < SLAB_NUM_MEM_PARTITIONS; i++)
	{
		if(g_slab_early_init_data->mem_pid_buffer_request[i])
		{

			for(j = 0; j < buffer_types_array_size ; j++) /*search which memory partitions needed for buffers and calculate 
			the number of requests per partition and the total requests*/
			{
				/* in case someone requested only extra buffers from that type*/
				if(g_slab_early_init_data->mem_pid_buffer_request[i]->table_info[j].max > 0)
				{
					requested_bpids_per_partition[i] ++;
					total_requested_bpids ++;
				}
			}
		}
	}

	/*calculate the right amount of bpids to each mempid*/
	for(i = 0; i < SLAB_NUM_MEM_PARTITIONS; i++)
	{
		if(requested_bpids_per_partition[i] > 0){
			/* Use subtraction instead of using division and floating point to calculate proportion of bpids to each memory partition */
			temp = (requested_bpids_per_partition[i]) * (*num_bpids);
			while( temp >= total_requested_bpids && 
				available_bpids_per_partition[i] < requested_bpids_per_partition[i])
			{
				temp -= total_requested_bpids;
				available_bpids_per_partition[i] ++;
				available_bpids --;

			}
			reminder_bpids_calc_per_partition[i] = temp;
			if( available_bpids_per_partition[i] > maximum_requested_bpids )
			{
				maximum_requested_index = i;
				maximum_requested_bpids = available_bpids_per_partition[i];
			}
			if( available_bpids_per_partition[i] == 0 ) /*Need to take bpid from other partitions*/
			{
				available_bpids --;
				available_bpids_per_partition[i] ++;
			}
			minimum_needed_bpids ++;
			
		}
	}
	if(available_bpids > 0) /*check if there are spears of bpids and can be used by some of partitions*/
	{
		while( available_bpids > 0 )	
		{
			maximum_requested_bpids = 0;
			for(i = 0; i < SLAB_NUM_MEM_PARTITIONS; i++){
				if(reminder_bpids_calc_per_partition[i] > maximum_requested_bpids &&
					requested_bpids_per_partition[i] > available_bpids_per_partition[i]){
					maximum_requested_index = i;
					maximum_requested_bpids = reminder_bpids_calc_per_partition[i];
				}
			}
			if(maximum_requested_bpids > 0){
				reminder_bpids_calc_per_partition[maximum_requested_index] = 0;
				available_bpids_per_partition[maximum_requested_index] ++;
				available_bpids --;
			}
			else{
				*num_bpids -= available_bpids; /*not all bpids needed*/
				break;
			}
			
		}
	}
	else if(available_bpids < 0) /*reserved to much bpids, need to return*/
	{
		while( available_bpids < 0 )	
		{
			minimum_requested_bpids = total_requested_bpids;
			for(i = 0; i < SLAB_NUM_MEM_PARTITIONS; i++){
				if( reminder_bpids_calc_per_partition[i] >= 0 &&
					available_bpids_per_partition[i] > 1 &&
					reminder_bpids_calc_per_partition[i] <= minimum_requested_bpids )
				{
					minimum_requested_index = i;
					minimum_requested_bpids = reminder_bpids_calc_per_partition[i];
				}
			}
			if( minimum_requested_bpids < total_requested_bpids )
			{
				reminder_bpids_calc_per_partition[minimum_requested_index] = minimum_requested_bpids;
				available_bpids_per_partition[minimum_requested_index] --;
				available_bpids ++;
			}
			else
			{
				pr_err("Not enough DP-BP's to supply one for each requested memory partition.\n");
				return -ENODEV;
			}

		}
	}
	
	pr_info("Total amount of available bpids: %d\n",*num_bpids);
	pr_info("Minimum amount of needed bpids: %d\n",minimum_needed_bpids);
	available_bpids = *num_bpids;
	for(i = 0; i < SLAB_NUM_MEM_PARTITIONS; i ++){
		
		if(requested_bpids_per_partition[i] > 0)
		{
			pr_info("Partition %d:\n", i);
			pr_info("Requested %d\n", requested_bpids_per_partition[i]);
			available_bpids -= available_bpids_per_partition[i];
			if (available_bpids < 0)
				available_bpids_per_partition[i] = 0;
			pr_info("Available %d\n", available_bpids_per_partition[i]);

		}
		
	}

	*bpids_arr = (struct slab_bpid_info *)
		fsl_os_xmalloc((sizeof(struct slab_bpid_info) * (*num_bpids)),
		               SLAB_FAST_MEMORY,
		               1);
	if(*bpids_arr == NULL)
		return -ENOMEM;
	
//	pr_info("Allocated memory address for bpids array: %x\n",*bpids_arr);


	for(i = 0; i < SLAB_NUM_MEM_PARTITIONS; i++)/*j used as array index*/
	{
		if(available_bpids_per_partition[i] > 0)
		{
			err = slab_divide_memory_for_bpids(
				available_bpids_per_partition[i],
			        requested_bpids_per_partition[i],
			        (enum memory_partition_id) i,
			        bpids_arr);
			if(err)
				return err;
		}

	}
	
	/* Move the pointer to beginning of array - The array is fully filled */
	(*bpids_arr) -= *num_bpids;
	
	/*Free memory used for slab early init */
	for(i = 0; i < SLAB_NUM_MEM_PARTITIONS; i++)
	{
		if(g_slab_early_init_data->mem_pid_buffer_request[i])
		{
			fsl_os_xfree(g_slab_early_init_data->mem_pid_buffer_request[i]->table_info);
			fsl_os_xfree(g_slab_early_init_data->mem_pid_buffer_request[i]);
		}
	}
	fsl_os_xfree(g_slab_early_init_data);

	return 0;
}

/*****************************************************************************/
int slab_module_init(void)
{
	struct   slab_bpid_info *bpids_arr_init = NULL;
	int      num_bpids = 0;
	struct   slab_module_info *slab_m = NULL;
	int      err = 0, i, j;
	uint32_t *slab_ddr_pointer_stack;
	dma_addr_t ddr_pool_addr;
	uint32_t ddr_value_ptr[SLAB_MAX_NUM_VP_DDR];
	uint32_t cdma_cfg = 0;
	uint32_t num_clusters_for_ddr_mamangement_pools = 0;
	struct slab_v_pool *virtual_pool_struct = NULL;
	struct aiop_tile_regs *ccsr = (struct aiop_tile_regs *)\
		sys_get_memory_mapped_module_base(FSL_OS_MOD_CMGW, 0,
		                                  E_MAPPED_MEM_TYPE_GEN_REGS);


	err = dpbp_discovery(NULL, &num_bpids, FALSE);/*used to find number of available bpids*/
	if (err) {
		pr_err("Failed DPBP discovery\n");
		return -ENODEV;
	}



	/*Register buffers for DDR management pools*/
	num_clusters_for_ddr_mamangement_pools = (g_slab_early_init_data->num_ddr_pools >> 6) + 1; /*divide by 64*/
	g_slab_virtual_pools.num_clusters = num_clusters_for_ddr_mamangement_pools;
	g_slab_virtual_pools.slab_context_address = (uint64_t *)
					fsl_os_xmalloc((sizeof(uint64_t) *
						(num_clusters_for_ddr_mamangement_pools + 1)), /*cluster #0 is for shram*/
						SLAB_FAST_MEMORY,
						1);
	/*This call must be before slab_proccess_registered_requests*/
	err = slab_register_context_buffer_requirements(num_clusters_for_ddr_mamangement_pools, 
	                                                num_clusters_for_ddr_mamangement_pools,
	                                                (SLAB_MAX_NUM_VP_DDR * sizeof(struct slab_v_pool)),
	                                                SLAB_DEFAULT_ALIGN,
	                                                SLAB_DDR_MEMORY,
	                                                0,
	                                                0);
	if (err) {
		pr_err("Failed to allocate memory for DDR management pools\n");
		return -ENOMEM;
	}

	
	err = slab_proccess_registered_requests(&num_bpids, &bpids_arr_init);
	if (err) {
		pr_err("Failed DPBP distribution\n");
		return -ENODEV;
	}
	
	
	
	/*Allocate memory for slab handle*/
	slab_m = fsl_os_xmalloc(sizeof(struct slab_module_info),
	                        SLAB_FAST_MEMORY,
	                        1);
	if (slab_m == NULL)
		return -ENOMEM;


	slab_m->num_hw_pools = (uint8_t)(num_bpids & 0xFF);
	slab_m->hw_pools     =
		fsl_os_xmalloc(sizeof(struct slab_hw_pool_info) * num_bpids,
		               SLAB_DDR_MEMORY,
		               1);

	virtual_pool_struct  = (struct slab_v_pool *)
		fsl_os_xmalloc((sizeof(struct slab_v_pool) *
			SLAB_MAX_NUM_VP_SHRAM),
			SLAB_FAST_MEMORY,
			1);


	err = dpbp_discovery(bpids_arr_init, &num_bpids, TRUE);
	if (err) {
		pr_err("Failed DPBP add\n");
		return -ENODEV;
	}

	
	slab_ddr_pointer_stack = (uint32_t *)fsl_os_xmalloc(SLAB_MAX_NUM_VP_DDR *
	                                        num_clusters_for_ddr_mamangement_pools
	                                        * sizeof(uint64_t),
	                                        SLAB_DDR_MEMORY,
	                                        1);

	if ((slab_m->hw_pools == NULL) ||
		(virtual_pool_struct == NULL) ||
		(slab_ddr_pointer_stack == NULL)) {

		free_slab_module_memory(slab_m);
		return -ENOMEM;
	}

	ddr_pool_addr = (dma_addr_t)fsl_os_virt_to_phys((void *)slab_ddr_pointer_stack);
	g_slab_pool_pointer_ddr = ddr_pool_addr;
	
	g_slab_last_pool_pointer_ddr = ddr_pool_addr + (SLAB_MAX_NUM_VP_DDR * num_clusters_for_ddr_mamangement_pools * 8);
	
	
	/* TODO vpool_init() API will change to get more allocated
	 * by malloc() memories */
	slab_pool_init(virtual_pool_struct,
	               SLAB_MAX_NUM_VP_SHRAM,
	               0);


	/* CDMA CFG register bits are needed for filling BPID */
	cdma_cfg           = ioread32_ccsr(&ccsr->cdma_regs.cfg);
	slab_m->icid       = (uint16_t)(cdma_cfg & CDMA_ICID_MASK);
	slab_m->fdma_flags = FDMA_ACQUIRE_NO_FLAGS;
	if (cdma_cfg & CDMA_BDI_BIT)
		slab_m->fdma_flags |= FDMA_ACQUIRE_BDI_BIT;

	slab_m->fdma_dma_flags = 0;
	if (cdma_cfg & CDMA_BMT_BIT)
		slab_m->fdma_dma_flags |= FDMA_DMA_BMT_BIT;
	if (cdma_cfg & CDMA_PL_BIT)
		slab_m->fdma_dma_flags |= FDMA_DMA_PL_BIT;
	if (cdma_cfg & CDMA_VA_BIT)
		slab_m->fdma_dma_flags |= FDMA_DMA_eVA_BIT;

	pr_debug("CDMA CFG register = 0x%x addr = 0x%x\n", cdma_cfg, \
	         (uint32_t)&ccsr->cdma_regs.cfg);
	pr_debug("ICID = 0x%x flags = 0x%x\n", slab_m->icid, \
	         slab_m->fdma_flags);
	pr_debug("ICID = 0x%x dma flags = 0x%x\n", slab_m->icid, \
	         slab_m->fdma_dma_flags);

	err = slab_alocate_memory(num_bpids, slab_m, &bpids_arr_init);
	if(err){
		return err;
	}
	

	for(i = 1; i <= num_clusters_for_ddr_mamangement_pools; i++){ /* i = o used for SHRAM management*/
		ddr_value_ptr[0] =(uint32_t)( i << 16);
		
		for(j = 1; j < SLAB_MAX_NUM_VP_DDR ; j++){
			ddr_value_ptr[j] = ddr_value_ptr[j - 1] + 2;/*add 1 to second bit, (first used for  sw/hw pool)*/
		}
		fdma_dma_data(4 * SLAB_MAX_NUM_VP_DDR,
		              slab_m->icid,
		              &ddr_value_ptr,
		              ddr_pool_addr,
		              (slab_m->fdma_dma_flags |
		        	      FDMA_DMA_DA_WS_TO_SYS_BIT));
	
		ddr_pool_addr += 4 * SLAB_MAX_NUM_VP_DDR ;
		
	}

	/* Add to all system handles */
	err = sys_add_handle(slab_m, FSL_OS_MOD_SLAB, 1, 0);
	return err;
}

/*****************************************************************************/
void slab_module_free(void)
{
	int i;
	struct slab_module_info *slab_m = \
		sys_get_unique_handle(FSL_OS_MOD_SLAB);

	sys_remove_handle(FSL_OS_MOD_SLAB, 0);

	if (slab_m != NULL){
		for(i = 0; i < slab_m->num_hw_pools; i++)
			free_buffs_from_bman_pool(
				slab_m->hw_pools[i].pool_id,
				(int32_t)slab_m->hw_pools[i].total_num_buffs,
				slab_m->icid,
				slab_m->fdma_flags);
		free_slab_module_memory(slab_m);

	}
}

/*****************************************************************************/
int slab_debug_info_get(struct slab *slab, struct slab_debug_info *slab_info)
{
	int32_t max = 0, committed = 0, allocated = 0, temp;
	uint8_t flags =0;
	int     i;
	slab_release_cb_t *release_cb = NULL;
	struct slab_module_info *slab_m = \
		sys_get_unique_handle(FSL_OS_MOD_SLAB);

	if ((slab_info != NULL) && (slab_m != NULL) && SLAB_IS_HW_POOL(slab)) {
		if (slab_read_pool(SLAB_VP_POOL_GET(slab),
		                           &slab_info->pool_id,
		                           &max,
		                           &committed,
		                           &allocated,
		                           &flags,
		                           &release_cb) == 0) {
			/* Modify num_buffs to have the number of available
			 * buffers not allocated */
			slab_info->committed_buffs = (uint32_t)(committed);
			slab_info->max_buffs = (uint32_t)max;

			temp = slab_m->num_hw_pools;
			for (i = 0; i < temp; i++)
				CP_POOL_DATA(slab_m, slab_info, i);
		}
	}

	return -EINVAL;
}

/*****************************************************************************/
int slab_register_context_buffer_requirements(uint32_t    committed_buffs,
                                              uint32_t    max_buffs,
                                              uint16_t    buff_size,
                                              uint16_t    alignment,
                                              enum memory_partition_id  mem_pid,
                                              uint32_t    flags,
                                              uint32_t num_ddr_pools)
{	
	int i;
	struct request_table_info local_info[] = SLAB_BUFF_SIZES_ARR;
	int array_size =  ARRAY_SIZE(local_info);
	UNUSED(flags);
	
	
	switch(mem_pid)
	{
	case MEM_PART_DP_DDR:
	case MEM_PART_SYSTEM_DDR:
	case MEM_PART_PEB: 
		break;
	default:
		pr_err("Partition type %d not supported\n", mem_pid);
		return -EINVAL;	
	}
	
	if(g_slab_early_init_data->mem_pid_buffer_request[mem_pid] == NULL)
	{
		
		g_slab_early_init_data->mem_pid_buffer_request[mem_pid] = (struct early_init_request_table *)
								fsl_os_xmalloc((sizeof(struct early_init_request_table) ),
									SLAB_FAST_MEMORY,
									1);
		
		if(g_slab_early_init_data->mem_pid_buffer_request[mem_pid] == NULL)
			return -ENOMEM;
		g_slab_early_init_data->mem_pid_buffer_request[mem_pid]->table_info = (struct request_table_info *)
							fsl_os_xmalloc((sizeof(local_info) ),
								SLAB_FAST_MEMORY,
								1);
		if(g_slab_early_init_data->mem_pid_buffer_request[mem_pid]->table_info == NULL)
			return -ENOMEM;
		else{
			for(i = 0; i < array_size; i++){
				g_slab_early_init_data->mem_pid_buffer_request[mem_pid]->table_info[i].buff_size = local_info[i].buff_size;
				g_slab_early_init_data->mem_pid_buffer_request[mem_pid]->table_info[i].alignment = 0;
				g_slab_early_init_data->mem_pid_buffer_request[mem_pid]->table_info[i].committed_bufs = 0;
				g_slab_early_init_data->mem_pid_buffer_request[mem_pid]->table_info[i].extra = 0;
			}
		}		
	}
	
	if(num_ddr_pools > 0)
		g_slab_early_init_data->num_ddr_pools += num_ddr_pools;
	


	for(i = 0; i< array_size; i++){
		if(buff_size > local_info[i].buff_size)
			continue;
		else
			break;
	}
	if(i == array_size){
		pr_err("Requested buffer size to big\n");
		return -EINVAL;
	}
	if(max_buffs < committed_buffs){
		pr_err("Max buffers must be bigger or equal to committed\n");
	}
	/* max_bufs must not be 0*/
	if (!max_buffs){
		pr_err("Max buffers can't be zero\n");
		return -EINVAL;
	}

	if(!IS_POWER_VALID_ALLIGN(alignment, SLAB_SIZE_SET(local_info[i].buff_size)))
	{
		pr_err("Invalid alignment\n");
		return -EINVAL;
	}
	g_slab_early_init_data->mem_pid_buffer_request[mem_pid]->table_info[i].committed_bufs += committed_buffs;
	
	if (max_buffs - committed_buffs > g_slab_early_init_data->mem_pid_buffer_request[mem_pid]->table_info[i].extra)
		g_slab_early_init_data->mem_pid_buffer_request[mem_pid]->table_info[i].extra = (max_buffs - committed_buffs);
	/* allocation will be using max buffers number */
	g_slab_early_init_data->mem_pid_buffer_request[mem_pid]->table_info[i].max = 
		g_slab_early_init_data->mem_pid_buffer_request[mem_pid]->table_info[i].committed_bufs + 
		g_slab_early_init_data->mem_pid_buffer_request[mem_pid]->table_info[i].extra;
		

	if(alignment > g_slab_early_init_data->mem_pid_buffer_request[mem_pid]->table_info[i].alignment)
		g_slab_early_init_data->mem_pid_buffer_request[mem_pid]->table_info[i].alignment = (uint16_t) alignment;

	return 0;
}
/*****************************************************************************/
