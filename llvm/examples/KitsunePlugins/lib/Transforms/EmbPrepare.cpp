//===- EmbPrepare.cpp - Prepare embedded modules for codegen --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Prepare embedded modules for code generation.
//
//===----------------------------------------------------------------------===//

#include "kitsune/Transforms/EmbPrepare.h"
#include "EmbPrepareImpl.h"
#include "kitsune/Analysis/TapirTargetAnalysis.h"
#include "kitsune/Core/CommandLineOptions.h"

using namespace llvm;

static cl::opt<bool> clInlineAll(
    "emb-inline-all", cl::init(false), cl::Hidden,
    cl::desc("Inline all device functions in the kernel module, unless they "
             "have the 'noinline' attribute"),
    cl::cat(cl::catKitClDevOpts));

static cl::opt<bool> clInlineAllForce(
    "emb-inline-all-force", cl::init(false), cl::Hidden,
    cl::desc("Inline all device functions in the kernel module, including "
             "those that have the 'noinline' attribute"),
    cl::cat(cl::catKitClDevOpts));

namespace llvm {

bool EmbPreparePass::run(TTID tt, Module &devM, Module &hostM,
                         ModuleAnalysisManager &hostMAM) {
  detail::EmbPrepareOptions prepOpts;
  prepOpts.inlineAll = clInlineAll;
  prepOpts.inlineAllForce = clInlineAllForce;

  const TapirTargetInfo &tgi = hostMAM.getResult<TapirTargetAnalysis>(hostM);
  const KitsuneTTOptions &tto = tgi.getOptions();
  switch (tt) {
  case TTID::Cuda:
    return detail::embPrepareCuda(devM, tto, prepOpts);
  case TTID::Hip:
    return detail::embPrepareHip(devM, tto, prepOpts);
  default:
    llvm_unreachable("EmbPreparePass::run: TTID not handled");
  }
}

} // namespace llvm
