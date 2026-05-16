#include "ddp_mmp.h"
#include "ddp_reg.h"
#include "ddp_log.h"

#include "ddp_m4u.h"

static struct DDP_MMP_Events DDP_MMP_Events;

void init_ddp_mmp_events(void)
{
}

void ddp_mmp_ovl_layer(struct OVL_CONFIG_STRUCT *pLayer,
	unsigned int down_sample_x,
	unsigned int down_sample_y,
	unsigned int session /*1:primary, 2:external, 3:memory */)
{
}

void ddp_mmp_wdma_layer(struct WDMA_CONFIG_STRUCT *wdma_layer,
	unsigned int wdma_num, unsigned int down_sample_x,
	unsigned int down_sample_y)
{
}

void ddp_mmp_rdma_layer(struct RDMA_CONFIG_STRUCT *rdma_layer,
	unsigned int rdma_num, unsigned int down_sample_x,
	unsigned int down_sample_y)
{
}

struct DDP_MMP_Events *ddp_mmp_get_events(void)
{
	return &DDP_MMP_Events;
}

void ddp_mmp_init(void)
{
}
