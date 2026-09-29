//==- EmbResolveLibDeviceCalls.h - Resolve libdevice calls ------*- C++ -*--==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Embedded modules may contain calls to library functions for which
// device-specific implementations exist. This resolves calls to such functions
// in embedded modules to use the device-specific implementations.
//
//===----------------------------------------------------------------------===//

#ifndef KITSUNE_TRANSFORMS_EMB_RESOLVE_LIB_DEVICE_CALLS_H
#define KITSUNE_TRANSFORMS_EMB_RESOLVE_LIB_DEVICE_CALLS_H

#include "kitsune/Transforms/EmbModulePass.h"

namespace llvm {

/// Resolve the calls to functions in the embedded bitcode that have
/// device-specific implementations in one or more vendor-provided bitcode files
/// for the device (usually a GPU). This will look for calls to functions that
/// have device equivalents, add declarations for the called device equivalents,
/// then replace the calls with these new declarations. In some cases, bitcode
/// files are provided by vendors containing the definitions of these functions.
/// Those bitcode files are linked into the embedded bitcode module in a
/// separate pass.
class EmbResolveLibDeviceCallsPass
    : public EmbModulePass<EmbResolveLibDeviceCallsPass> {
public:
  bool run(TTID tt, Module &devM, Module &hostM,
           ModuleAnalysisManager &hostMAM);

  using EmbModulePass<EmbResolveLibDeviceCallsPass>::run;
};

} // namespace llvm

#endif // KITSUNE_TRANSFORMS_EMB_RESOLVE_LIB_DEVICE_CALLS_H
