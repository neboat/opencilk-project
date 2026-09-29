//===- GenerateCtors.cpp - Generate global ctors for Kitsune --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implementation of pass that generates global constructors for Kitsune's
// runtime
//
//===----------------------------------------------------------------------===//

#include "kitsune/Transforms/GenerateCtors.h"
#include "GenerateCtorsImpl.h"
#include "kitsune/Analysis/TapirTargetAnalysis.h"
#include "kitsune/Core/CommandLineOptions.h"
#include "kitsune/Core/ConstantUtils.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

// FIXME: We really should not be exposing command line options from other
// source files. These are experimental options that have been hacked in for the
// moment. If this is useful, we should consider adding it to the tapir target
// options instead. Otherwise, it should be removed altogether.
//
// These are declared [[weak]] because they are defined in one of the tapir
// targets. The tapir targets are not guaranteed to be built, therefore, these
// globals may not be available at link time.
//
#ifdef __APPLE__
#define WEAK weak_import
#else
#define WEAK weak
#endif
extern __attribute__((WEAK)) cl::opt<bool> clRefineLaunches;
extern __attribute__((WEAK)) cl::opt<bool> clUseYLaunch;

/// Is the given intrinsic \param id called at least once in the module \param m
/// with the tapir target id \param tt
static bool isCalledWithTTID(Module &m, Intrinsic::ID id, TTID tt) {
  if (Function *f = m.getFunction(Intrinsic::getBaseName(id)))
    for (Use &u : f->uses())
      if (auto *call = dyn_cast<CallBase>(u.getUser()))
        // Although unlikely, the intrinsic could have been passed as an
        // argument to some other function. Just in case, check that the callee
        // at this site is the launch kernel function.
        if (call->getCalledFunction() == f)
          if (auto *cint = dyn_cast<ConstantInt>(call->getArgOperand(0)))
            if (std::optional<TTID> ttid = createTTIDFrom(*cint))
              if (*ttid == tt)
                return true;
  return false;
}

/// Should a ctor be generated for a tapir target.
static bool shouldGenerateCtor(Module &m, TTID tt) {
  switch (tt) {
  case TTID::Cuda:
  case TTID::Hip:
    return isCalledWithTTID(m, Intrinsic::kit_async_launch_kernel, tt);
  case TTID::Pthreads:
    return isCalledWithTTID(m, Intrinsic::kit_async_launch_threads, tt);
  default:
    llvm_unreachable("shouldGenereateCtor: TTID not handled");
  }
}

static const std::map<TTID, detail::GenerateCtorImplFn> genCtorFns = {
    // {TTID::Pthreads, detail::genCtorPthreads},
    {TTID::Cuda, detail::genCtorCuda},
    // {TTID::Hip, detail::genCtorHip},
};

PreservedAnalyses GenerateCtorsPass::run(Module &m,
                                         ModuleAnalysisManager &mam) {
  // If no primary tapir target has been set, there will be nothing to do, so
  // bail out immediately.
  const TapirTargetInfo &tgi = mam.getResult<TapirTargetAnalysis>(m);
  if (not tgi.hasTTID())
    return PreservedAnalyses::all();

  auto &fam = mam.getResult<FunctionAnalysisManagerModuleProxy>(m).getManager();
  auto getTLI = [&](Function &f) -> TargetLibraryInfo & {
    return fam.getResult<TargetLibraryAnalysis>(f);
  };

  detail::GenerateCtorOptions genCtorOpts;
  if (&clRefineLaunches)
    genCtorOpts.refineLaunches = clRefineLaunches;
  if (&clUseYLaunch)
    genCtorOpts.useYLaunch = clUseYLaunch;

  const KitsuneTTOptions &ttOpts = tgi.getOptions();
  for (const auto &[tt, genCtorFn] : genCtorFns)
    if (shouldGenerateCtor(m, tt))
      genCtorFn(m, getTLI, ttOpts, genCtorOpts);

  // This never invalidates any analyses since, at most, only the initializer of
  // a global variable will have changed. The generated ctors will not be called
  // explicitly in the code, so the callgraph will not have changed either.
  return PreservedAnalyses::all();
}
