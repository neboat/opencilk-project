//===- StripKitsuneAddrSpaces.cpp - Strip Kitsune's address spaces --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implementation of passes to strip kitsune address spaces from types.
//
//===----------------------------------------------------------------------===//

#include "kitsune/CodeGen/StripKitsuneAddrSpaces.h"
#include "kitsune/Core/AddrSpaceUtils.h"
#include "kitsune/Support/AddrSpace.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"

#define DEBUG_TYPE "kit-strip-addr-spaces"

using namespace llvm;

namespace {

// /// Legacy pass to compile the embedded bitcode to fat binaries.
// class StripKitsuneAddrSpacesLegacyPass : public ModulePass {
// public:
//   StripKitsuneAddrSpacesLegacyPass() : ModulePass(ID) {
//     initializeStripKitsuneAddrSpacesLegacyPassPass(
//         *PassRegistry::getPassRegistry());
//   }

//   StringRef getPassName() const override {
//     return "Strip Kitsune address spaces";
//   }

//   void getAnalysisUsage(AnalysisUsage &au) const override {}

//   bool runOnModule(Module &m) override { return stripKitsuneAddrSpaces(m); }

// public:
//   static char ID;
// };

} // namespace

// char StripKitsuneAddrSpacesLegacyPass::ID = 0;

// INITIALIZE_PASS_BEGIN(StripKitsuneAddrSpacesLegacyPass, DEBUG_TYPE,
//                       "Strip kitsune address spaces", false, false)
// INITIALIZE_PASS_END(StripKitsuneAddrSpacesLegacyPass, DEBUG_TYPE,
//                     "Strip kitsune address spaces", false, false)

// ModulePass *llvm::createStripKitsuneAddrSpacesLegacyPass() {
//   return new StripKitsuneAddrSpacesLegacyPass();
// }

PreservedAnalyses StripKitsuneAddrSpacesPass::run(Module &m,
                                                  ModuleAnalysisManager &mam) {
  if (stripKitsuneAddrSpaces(m))
    return PreservedAnalyses::none();
  return PreservedAnalyses::all();
}
