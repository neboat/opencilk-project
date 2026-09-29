#include "kitsune/Analysis/TapirTargetAnalysis.h"
#include "kitsune/Transforms/Tapir/CudaABI.h"
#include "kitsune/Core/KitsuneTTOptions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Transforms/Tapir/LoweringUtils.h"
#include "llvm/Transforms/Tapir/TapirTargetPlugin.h"

#include "kitsune/Analysis/TapirTargetAnalysis.h"
#include "kitsune/CodeGen/CodeGenFatBinaries.h"
#include "kitsune/CodeGen/LowerHyperIntrinsics.h"
#include "kitsune/CodeGen/LowerKitsuneIntrinsics.h"
// #include "kitsune/CodeGen/StripKitsuneAddrSpaces.h"
#include "kitsune/Transforms/EmbLinkLibDeviceBitcode.h"
#include "kitsune/Transforms/EmbOptimize.h"
#include "kitsune/Transforms/EmbPrepare.h"
#include "kitsune/Transforms/EmbResolveLibDeviceCalls.h"
#include "kitsune/Transforms/GenerateCtors.h"
#include "kitsune/Transforms/MemOpsReplacement.h"
#include "kitsune/Transforms/Prefetching.h"
#include "kitsune/Transforms/RecomputeKernelProperties.h"

using namespace llvm;

// This externally visible function with C linkage is required. It is the
// well-known entry point required by LLVM.
//
// The compiler and linker options that are returned have been chosen to be
// relatively "innocuous". This plugin is used in Kitsune's core tests, so we
// do need something that is unlikely to unexpectedly change the compiler's
// output.
extern "C" ::llvm::TapirTargetPluginInfo LLVM_ATTRIBUTE_WEAK
llvmGetTapirTargetPluginInfo() {
  return {
      LLVM_TAPIR_TARGET_PLUGIN_API_VERSION,
      "TTPluginCuda",
      "1.0",
      [](Module &HostM, const TapirTargetOptions &TTO,
         ModuleAnalysisManager &AM) -> TapirTarget * {
        return new CudaABI(
            HostM, KitsuneTTOptions::createPluginOptions(TTID::Cuda), AM);
      },
      // []() -> CustomTargetOptionsBase * {},
      [](PassBuilder &PB) {
        PB.registerAnalysisRegistrationCallback([](ModuleAnalysisManager &MAM) {
          MAM.registerPass([&] {
            return TapirTargetAnalysis(
                KitsuneTTOptions::createPluginOptions(TTID::Cuda));
          });
        });
        PB.registerTapirLoopEndEPCallback(
            [](llvm::ModulePassManager &PM, OptimizationLevel Level) {
              PM.addPass(PrefetchingPass());
              PM.addPass(EmbResolveLibDeviceCallsPass());
              PM.addPass(EmbPreparePass());
              PM.addPass(EmbLinkLibDeviceBitcodePass());
              PM.addPass(EmbOptimizePass());
              PM.addPass(LowerHyperIntrinsicsPass());
              PM.addPass(MemOpsReplacementPass());
              PM.addPass(RecomputeKernelPropertiesPass());
              PM.addPass(GenerateCtorsPass());

              PM.addPass(LowerKitsuneIntrinsicsPass());
              PM.addPass(CodeGenFatBinariesPass());
            });
      },
      // []() -> TapirTargetPlugin::ExtraArgsList { return {"-O"}; },
      // []() -> TapirTargetPlugin::ExtraArgsList {
      //   return {"-L/path/to/something/that/does/not/exist"};
      // }
  };
}