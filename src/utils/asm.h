/*
 * SPDX-FileCopyrightText: NVIDIA CORPORATION & AFFILIATES
 * Copyright (c) 2021-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: GPL-2.0-only or BSD-2-Clause
 */

#ifndef ASM_H_
#define ASM_H_

/*
 * Write barriers follow the Linux kernel naming and semantics:
 * - wmb() orders prior stores to memory and MMIO before later stores, e.g. doorbell
 *   record update before the MMIO doorbell write.
 * - dma_wmb() orders stores to memory shared with the device, e.g. WQE before the
 *   doorbell record update. It doesn't order MMIO.
 *
 * Read barriers don't follow the Linux naming. rmb() matches Linux rmb() only on
 * aarch64. On x86_64 and ppc64 it's the equivalent of Linux dma_rmb() and smp_rmb().
 * Its users need dma_rmb() (CQE read after the ownership check) or smp_rmb()
 * (data shared between CPUs). mb() is a compiler barrier on x86_64, unlike Linux mb().
 */

#if defined(__aarch64__)
#include "asm-arm64.h"
#elif defined(__powerpc64__)
#include "asm-ppc64.h"
#elif defined(__x86_64__)
#include "asm-x86.h"
#else
#error No architecture specific memory barrier definitions found!
#endif

#endif
