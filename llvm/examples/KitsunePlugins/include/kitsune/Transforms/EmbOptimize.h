//===- EmbOptimize.h - Optimize embedded modules ---------------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Run the standard sequence of optimization passes on the embedded bitcode
// in the module.
//
//===----------------------------------------------------------------------===//

#ifndef KITSUNE_TRANSFORMS_EMB_OPTIMIZE_H
#define KITSUNE_TRANSFORMS_EMB_OPTIMIZE_H

#include "kitsune/Transforms/EmbModulePass.h"

namespace llvm {

/// This will run the standard optimization passes on the embedded module for
/// the given tapir target.
class EmbOptimizePass : public EmbModulePass<EmbOptimizePass> {
public:
  bool run(TTID tt, Module &devM, Module &hostM, ModuleAnalysisManager &hostAM);

  using EmbModulePass<EmbOptimizePass>::run;
};

} // namespace llvm

#endif // KITSUNE_TRANSFORMS_EMB_OPTIMIZE_H
