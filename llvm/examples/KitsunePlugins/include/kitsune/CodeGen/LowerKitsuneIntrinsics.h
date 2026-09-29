//===- LowerKitsuneIntrinsics.h - Lower kitsune intrinsics ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Lowers kitsune's intrinsics. These typically correspond to a single function
// in Kitsune's runtime, but not necessarily.
//
//===----------------------------------------------------------------------===//

#ifndef KITSUNE_CODEGEN_LOWER_KITSUNE_INTRINSICS_H
#define KITSUNE_CODEGEN_LOWER_KITSUNE_INTRINSICS_H

#include "llvm/IR/PassManager.h"

namespace llvm {

// class ModulePass;

/// This pass is responsible for lowering Kitsune's runtime intrinsics. These
/// are intrinsics that correspond to a function in kitsune's runtime.
class LowerKitsuneIntrinsicsPass
    : public PassInfoMixin<LowerKitsuneIntrinsicsPass> {
public:
  PreservedAnalyses run(Module &m, ModuleAnalysisManager &mam);

  static bool isRequired() { return true; }
};

// ModulePass *createLowerKitsuneIntrinsicsLegacyPass();

} // end namespace llvm

#endif // KITSUNE_CODEGEN_LOWER_KITSUNE_INTRINSICS_H
