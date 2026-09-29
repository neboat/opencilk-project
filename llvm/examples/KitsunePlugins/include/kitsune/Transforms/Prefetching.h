//===- Prefetching.h - Generate dtoh/htod prefetch calls --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Generate prefetch calls to initiate movement of data between host and device.
//
//===----------------------------------------------------------------------===//

#ifndef KITSUNE_TRANSFORMS_PREFETCHING_H
#define KITSUNE_TRANSFORMS_PREFETCHING_H

#include "llvm/IR/PassManager.h"

namespace llvm {

/// This pass generates calls to initiate movement of data between host and
/// device. This will only generate calls to Kitsne's prefetch intrinsics. This
/// must should typically be run early in the Kitsune's post-tapir pipeline, but
/// it is ok to run it later in the pipeline as well. This should only ever
/// modify the host, but it may be profitable to examine the embedded device
/// modules when deciding if/when to prefetch.
class PrefetchingPass : public PassInfoMixin<PrefetchingPass> {
public:
  PreservedAnalyses run(Module &m, ModuleAnalysisManager &mam);

  static bool isRequired() { return true; }
};

} // namespace llvm

#endif // KITSUNE_TRANSFORMS_PREFETCHING_H
