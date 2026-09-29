//===- EmbOptimize.cpp - Optimize embedded modules ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Run the standard sequence of optimization passes on the embedded modules.
//
//===----------------------------------------------------------------------===//

#include "kitsune/Transforms/EmbOptimize.h"
#include "kitsune/Analysis/TapirTargetAnalysis.h"
#include "kitsune/Core/CommandLineOptions.h"
#include "kitsune/Core/KitsuneTTOptions.h"
#include "kitsune/Core/TTUtils.h"
#include "kitsune/Core/TargetUtils.h"
#include "kitsune/Support/OptznLevelUtils.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/OptimizationLevel.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/Tapir/LoweringUtils.h"

#define DEBUG_TYPE "emb-optimize"

using namespace llvm;

static cl::opt<OptznLevel> clOptznLevel(
    cl::init(OptznLevel::O2), cl::Hidden,
    cl::values(clEnumValN(OptznLevel::O0, "emb-O0", "No optimizations"),
               clEnumValN(OptznLevel::O1, "emb-O1", "Some optimizations"),
               clEnumValN(OptznLevel::O2, "emb-O2", "Most optimizations"),
               clEnumValN(OptznLevel::O3, "emb-O3",
                          "Most optimizations plus expensive ones"),
               clEnumValN(OptznLevel::Os, "emb-Os", "Optimize for size"),
               clEnumValN(OptznLevel::Oz, "emb-Oz",
                          "Aggressively optimize for size")),
    cl::desc("Optimization level for the embedded modules"),
    cl::cat(cl::catKitClDevOpts));

static cl::opt<bool> clPrintEmbPipelinePasses(
    "emb-print-pipeline-passes", cl::init(false), cl::Hidden,
    cl::desc("Print the passes that will be run on the embedded modules"),
    cl::cat(cl::catKitClDevOpts));

static OptimizationLevel mapToOptimizationLevel(OptznLevel optznLevel) {
  switch (optznLevel) {
  case OptznLevel::O0:
    return OptimizationLevel::O0;
  case OptznLevel::O1:
    return OptimizationLevel::O1;
  case OptznLevel::O2:
    return OptimizationLevel::O2;
  case OptznLevel::O3:
    return OptimizationLevel::O3;
  case OptznLevel::Os:
    return OptimizationLevel::Os;
  case OptznLevel::Oz:
    return OptimizationLevel::Oz;
  }
  llvm_unreachable("mapToOptimizationLevel: OptznLevel not handled");
}

namespace {

/// Optimize the embedded bitcode. This runs the standard sequence of
/// optimization passes on it.
class EmbOptimize {
private:
  TTID tt;
  const KitsuneTTOptions &tto;

protected:
  EmbOptimize(TTID tt, const KitsuneTTOptions &tto) : tt(tt), tto(tto) {}

  /// Construct the pipeline tuning options. These may be different depending on
  /// the bitcode being optimized.
  virtual PipelineTuningOptions
  getPipelineTuningOptions(OptimizationLevel optLevel) = 0;

public:
  virtual ~EmbOptimize() = default;

  bool run(Module &devM) {
    // If the optimization level has been overridden on the command line, prefer
    // that, otherwise, use the optimization level from the TTOptions.
    OptznLevel optznLevel = tto.getOptznLevel();
    if (clOptznLevel.getNumOccurrences())
      optznLevel = clOptznLevel;

    std::optional<PGOOptions> PGOOpt;

    // The analysis managers must be declared in this order so that they are
    // destroyed in the correct order due to inter-analysis-manager references
    LoopAnalysisManager lam;
    FunctionAnalysisManager fam;
    CGSCCAnalysisManager cgam;
    ModuleAnalysisManager mam;
    PassInstrumentationCallbacks pic;
    PrintPassOptions PrintPassOpts;
    StandardInstrumentations SI(
      devM.getContext(),
      false,
      true, PrintPassOpts);
    SI.registerCallbacks(pic, &mam);
    TargetMachine *tm = createTargetMachine(tt, tto);
    OptimizationLevel optLevel = mapToOptimizationLevel(optznLevel);
    PipelineTuningOptions pto = getPipelineTuningOptions(optLevel);

    PassBuilder pb(tm, pto, PGOOpt, &pic);
    pb.registerModuleAnalyses(mam);
    pb.registerCGSCCAnalyses(cgam);
    pb.registerFunctionAnalyses(fam);
    pb.registerLoopAnalyses(lam);
    tm->registerPassBuilderCallbacks(pb);
    pb.crossRegisterProxies(lam, fam, cgam, mam);

    ModulePassManager mpm = pb.buildPerModuleDefaultPipeline(optLevel);
    mpm.addPass(VerifierPass());
    if (clPrintEmbPipelinePasses) {
      mpm.printPipeline(outs(), [&pic](StringRef className) -> StringRef {
        StringRef passName = pic.getPassNameForClassName(className);
        return passName.empty() ? className : passName;
      });
      outs() << "\n";
    }
    mpm.run(devM, mam);

    return true;
  }
};

/// Optimize a module for NVPTX.
class EmbOptimizeCuda : public EmbOptimize {
protected:
  PipelineTuningOptions
  getPipelineTuningOptions(OptimizationLevel optLevel) override final {
    unsigned speedupLevel = optLevel.getSpeedupLevel();
    PipelineTuningOptions pto;
    pto.LoopUnrolling = speedupLevel > 1;
    pto.LoopInterleaving = speedupLevel > 2;
    pto.LoopStripmine = speedupLevel > 2;
    pto.LoopVectorization = false;
    pto.SLPVectorization = false;

    return pto;
  }

public:
  EmbOptimizeCuda(const KitsuneTTOptions &tto) : EmbOptimize(TTID::Cuda, tto) {}
};

/// Optimize a module for AMDGPU.
class EmbOptimizeHip : public EmbOptimize {
protected:
  PipelineTuningOptions
  getPipelineTuningOptions(OptimizationLevel optLevel) override final {
    unsigned speedupLevel = optLevel.getSpeedupLevel();
    PipelineTuningOptions pto;
    pto.LoopUnrolling = speedupLevel > 1;
    pto.LoopInterleaving = speedupLevel > 2;
    pto.LoopStripmine = speedupLevel > 2;
    pto.LoopVectorization = false;
    pto.SLPVectorization = false;

    return pto;
  }

public:
  EmbOptimizeHip(const KitsuneTTOptions &tto) : EmbOptimize(TTID::Hip, tto) {}
};

} // namespace

namespace llvm {

bool EmbOptimizePass::run(TTID tt, Module &devM, Module &hostM,
                          ModuleAnalysisManager &hostMAM) {
  const TapirTargetInfo &tgi = hostMAM.getResult<TapirTargetAnalysis>(hostM);
  const KitsuneTTOptions &tto = tgi.getOptions();

  switch (tt) {
  case TTID::Cuda:
    return EmbOptimizeCuda(tto).run(devM);
  case TTID::Hip:
    return EmbOptimizeHip(tto).run(devM);
  default:
    llvm_unreachable("EmbOptimizePass::run: TTID not handled");
  }
}

} // namespace llvm
