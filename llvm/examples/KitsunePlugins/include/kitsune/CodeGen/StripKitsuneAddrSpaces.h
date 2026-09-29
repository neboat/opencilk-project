//===- StripKitsuneAddrSpaces.h - Strip kitsune address spaces --*- C++ -*--==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Strip kitsune-specific address spaces from pointers and replace them with
// pointers in the default address space.
//
//===----------------------------------------------------------------------===//

#ifndef KITSUNE_CODEGEN_STRIP_KITSUNE_ADDR_SPACES_H
#define KITSUNE_CODEGEN_STRIP_KITSUNE_ADDR_SPACES_H

#include "llvm/IR/PassManager.h"

namespace llvm {

class ModulePass;

/// Change the types of entities that are in kitsune-specific address spaces.
/// Change these to the default address space. This also strips kitsune's
/// address spaces from any embedded modules. This is done because the backends
/// do not currently know what to with Kitsune's address spaces.
class StripKitsuneAddrSpacesPass
    : public PassInfoMixin<StripKitsuneAddrSpacesPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);

  static bool isRequired() { return true; }
};

ModulePass *createStripKitsuneAddrSpacesLegacyPass();

} // end namespace llvm

#endif // KITSUNE_CODEGEN_STRIP_KITSUNE_ADDR_SPACES_H
