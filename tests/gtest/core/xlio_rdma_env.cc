/*
 * SPDX-FileCopyrightText: NVIDIA CORPORATION & AFFILIATES
 * Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: GPL-2.0-only or BSD-2-Clause
 */

#include "common/def.h"
#include "common/log.h"
#include "common/sys.h"
#include "common/base.h"
#include "common/cmn.h"

#include "xlio_base.h"

#if defined(EXTRA_API_ENABLED) && (EXTRA_API_ENABLED == 1)

class xlio_rdma_env : public xlio_base {};

/**
 * @test xlio_rdma_env.ti_1
 * @brief
 *    XLIO forces MLX5_SHUT_UP_BF=1 so rdma-core maps non-cached UARs
 * @details
 *    Doorbells don't flush write-combining buffers, so they rely on NC UAR pages.
 */
TEST_F(xlio_rdma_env, ti_1)
{
    // The first socket triggers the XLIO global initialization.
    int fd = socket(m_family, SOCK_STREAM, IPPROTO_IP);
    ASSERT_LE(0, fd);

    const char *env = getenv("MLX5_SHUT_UP_BF");
    ASSERT_NE(nullptr, env);
    EXPECT_STREQ("1", env);

    close(fd);
}

#endif /* EXTRA_API_ENABLED */
