/*
 * SPDX-FileCopyrightText: NVIDIA CORPORATION & AFFILIATES
 * Copyright (c) 2021-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: GPL-2.0-only or BSD-2-Clause
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(DEFINED_DIRECT_VERBS) && (DEFINED_DIRECT_VERBS == 3)

#include "ib/mlx5/ib_mlx5.h"

int xlio_ib_mlx5dv_init_obj(struct mlx5dv_obj *obj, uint64_t type)
{
    int ret = 0;

    ret = mlx5dv_init_obj(obj, type);

    return ret;
}

int xlio_ib_mlx5_req_notify_cq(xlio_ib_mlx5_cq_t *mlx5_cq, int solicited)
{
    uint64_t doorbell;
    uint32_t sn;
    uint32_t ci;
    uint32_t cmd;

    sn = mlx5_cq->cq_sn & 3;
    ci = mlx5_cq->cq_ci & 0xffffff;
    cmd = solicited ? MLX5_CQ_DB_REQ_NOT_SOL : MLX5_CQ_DB_REQ_NOT;

    doorbell = sn << 28 | cmd | ci;
    doorbell <<= 32;
    doorbell |= mlx5_cq->cq_num;

    mlx5_cq->dbrec[XLIO_IB_MLX5_CQ_ARM_DB] = htonl(sn << 28 | cmd | ci);

    /*
     * Make sure that the doorbell record in host memory is written before ringing the
     * doorbell via PCI MMIO. The same as mlx5_cq_arm() of the Linux kernel. The UAR page is
     * non-cached, so no write-combining flush is needed after the doorbell.
     */
    wmb();

    *(uint64_t *)((uint8_t *)mlx5_cq->uar + MLX5_CQ_DOORBELL) = htonll(doorbell);

    return 0;
}

void xlio_ib_mlx5_get_cq_event(xlio_ib_mlx5_cq_t *mlx5_cq, int count)
{
    mlx5_cq->cq_sn += count;
}

bool xlio_ib_mlx5_is_nc_uar(struct ibv_context *context, const void *uar_addr)
{
    /*
     * A UAR page is mapped with system page granularity. With 64KB pages, it holds 16 device
     * UARs of 4KB, all mapped with the same attributes.
     */
    static const uintptr_t uar_map_size = (uintptr_t)sysconf(_SC_PAGESIZE);

    /*
     * rdma-core returns the context singleton NC UAR here without allocating a new one.
     * The same page backs CQs and, with MLX5_SHUT_UP_BF=1, SQs.
     */
    struct mlx5dv_devx_uar *nc = mlx5dv_devx_alloc_uar(context, MLX5DV_UAR_ALLOC_TYPE_NC);
    if (!nc) {
        return false;
    }
    bool is_nc = (uintptr_t)uar_addr - (uintptr_t)nc->base_addr < uar_map_size;
    mlx5dv_devx_free_uar(nc);

    return is_nc;
}

#endif /* (DEFINED_DIRECT_VERBS == 3) */
