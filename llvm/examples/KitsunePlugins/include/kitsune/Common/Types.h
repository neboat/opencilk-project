//===- Types.h - Types shared between the compiler and runtime -*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Types that are shared between the compiler and the runtime
//
//===----------------------------------------------------------------------===//

#ifndef KITSUNE_COMMON_TYPES_H
#define KITSUNE_COMMON_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// FIXME: This should be included in kitrt.h where it should replace the
// _kitrt_inst_mix_info struct
//
/// *** EXPERIMENTAL:
///
/// This contains information about the instruction kinds in a kernel and the
/// number of device-side functions called from it. It is gathered from the LLVM
/// form of the code (not ptx/s-code) and at this point is limited.  In general
/// we are using to explore impacts on launch parameters.
struct KernelInstMixData {
public:
  uint64_t memOps = 0;
  uint64_t fpOps = 0;
  uint64_t intOps = 0;
  uint64_t otherOps = 0;

public:
  KernelInstMixData &operator+=(const KernelInstMixData &o) {
    this->memOps += o.memOps;
    this->fpOps += o.fpOps;
    this->intOps += o.intOps;
    this->otherOps += o.otherOps;

    return *this;
  }
};

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // KITSUNE_COMMON_TYPES_H
