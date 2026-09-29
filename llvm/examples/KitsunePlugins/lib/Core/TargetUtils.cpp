//===- TargetUtils.cpp - Helper functions for targets/machines ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Utilities for targets and target machines.
//
//===----------------------------------------------------------------------===//

#include "kitsune/Core/TargetUtils.h"
#include "kitsune/Core/KitsuneTTOptions.h"
#include "kitsune/Support/OptznLevelUtils.h"
// #include "kitsune/Support/ToString.h"
#include "llvm/ADT/Twine.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Host.h"

using namespace llvm;

static TargetMachine *createAMDGPUTargetMachine(const KitsuneTTOptions &tto,
                                                CodeGenOptLevel cgOptLevel) {
  Triple triple(Twine("amdgcn"), Twine("amd"), Twine("amdhsa"));

  std::string err;
  const Target *target = TargetRegistry::lookupTarget("", triple, err);
  assert(target && "Unable to find registered AMDGPU target");

  CodeModel::Model codeModel = CodeModel::Small;
  Reloc::Model relocModel = Reloc::PIC_;
  TargetOptions opts;
  opts.UseInitArray = true;
  opts.EmitAddrsig = true;
  opts.AllowFPOpFusion = tto.getFPOpFusionMode();

  return target->createTargetMachine(triple, tto.getHipArch(),
                                     tto.getHipTargetFeatures(), opts,
                                     relocModel, codeModel, cgOptLevel);
}

static TargetMachine *createPTXTargetMachine(const KitsuneTTOptions &tto,
                                             CodeGenOptLevel cgOptLevel) {
  Triple triple(Twine("nvptx64"), Twine("nvidia"), Twine("cuda"));

  std::string err;
  const Target *target = TargetRegistry::lookupTarget("", triple, err);
  assert(target && "Unable to find registered NVPTX target");

  CodeModel::Model codeModel = CodeModel::Small;
  Reloc::Model relocModel = Reloc::PIC_;
  TargetOptions opts;
  opts.AllowFPOpFusion = tto.getFPOpFusionMode();

  return target->createTargetMachine(triple, tto.getCudaArch(),
                                     tto.getCudaTargetFeatures(), opts,
                                     relocModel, codeModel, cgOptLevel);
}

TargetMachine *llvm::createTargetMachine(TTID tt, const KitsuneTTOptions &tto) {
  CodeGenOptLevel cgOptLevel = createCodeGenOptLevelFrom(tto.getOptznLevel());
  return createTargetMachine(tt, tto, cgOptLevel);
}

TargetMachine *llvm::createTargetMachine(TTID tt, const KitsuneTTOptions &tto,
                                         CodeGenOptLevel cgOptLevel) {
  switch (tt) {
  case TTID::Cuda:
    return createPTXTargetMachine(tto, cgOptLevel);
  // case TTID::Hip:
  //   return createAMDGPUTargetMachine(tto, cgOptLevel);
  default:
    llvm_unreachable("createTargetMachine: TTID not handled");
  }
}

TargetMachine *llvm::createHostTargetMachine(const KitsuneTTOptions &tto) {
  CodeGenOptLevel cgOptLevel = createCodeGenOptLevelFrom(tto.getOptznLevel());
  Triple triple(sys::getDefaultTargetTriple());

  std::string err;
  const Target *target = TargetRegistry::lookupTarget("", triple, err);
  assert(target && "Unable to find registered target");

  CodeModel::Model codeModel = CodeModel::Small;
  Reloc::Model relocModel = Reloc::PIC_;
  TargetOptions opts;
  opts.AllowFPOpFusion = tto.getFPOpFusionMode();

  return target->createTargetMachine(triple, "", "", opts, relocModel,
                                     codeModel, cgOptLevel);
}

#define OPT_BOOL(NAME)                                                         \
  do {                                                                         \
    os << "  " << #NAME << ": " << ((opts.NAME) ? "true" : "false") << "\n";   \
  } while (0)

#define OPT_ENUM(NAME)                                                         \
  do {                                                                         \
    os << "  " << #NAME << ": " << opts.NAME << "\n";                          \
  } while (0)

#define OPT_INT(NAME)                                                          \
  do {                                                                         \
    os << "  " << #NAME << ": " << opts.NAME << "\n";                          \
  } while (0)

#define OPT_STR(NAME)                                                          \
  do {                                                                         \
    os << "  " << #NAME << ": " << opts.NAME << "\n";                          \
  } while (0)

// void llvm::dump(const TargetOptions &opts, raw_ostream &os) {
//   // This needs to be kept updated with the options in TargetOptions. These are
//   // printed in alphabetical order here, though they do not appear in this
//   // order in the TargetOptions object itself.
//   os << "Target Options:\n";
//   os << "  AllowFPOpFusion: " << opts.AllowFPOpFusion << "\n";
//   OPT_BOOL(ApproxFuncFPMath);
//   OPT_BOOL(BBAddrMap);
//   OPT_ENUM(BBSections);
//   os << "  BinutilsVersion: " << opts.BinutilsVersion.first << "."
//      << opts.BinutilsVersion.second << "\n";
//   OPT_BOOL(DataSections);
//   OPT_ENUM(DebuggerTuning);
//   OPT_BOOL(DebugStrictDwarf);
//   OPT_BOOL(DisableIntegratedAS);
//   OPT_ENUM(EABIVersion);
//   OPT_BOOL(EmitAddrsig);
//   OPT_BOOL(EmitCallSiteInfo);
//   OPT_BOOL(EmitStackSizeSection);
//   OPT_BOOL(EmulatedTLS);
//   OPT_BOOL(EnableAIXExtendedAltivecABI);
//   OPT_BOOL(EnableCFIFixup);
//   OPT_BOOL(EnableDebugEntryValues);
//   OPT_BOOL(EnableFastISel);
//   OPT_BOOL(EnableGlobalISel);
//   OPT_BOOL(EnableIPRA);
//   OPT_BOOL(EnableMachineOutliner);
//   OPT_BOOL(EnableMachineFunctionSplitter);
//   OPT_BOOL(EnableTLSDESC);
//   OPT_ENUM(FloatABIType);
//   OPT_BOOL(ForceDwarfFrameSection);
//   OPT_BOOL(FunctionSections);
//   OPT_BOOL(HonorSignDependentRoundingFPMathOption);
//   OPT_BOOL(Hotpatch);
//   OPT_ENUM(GlobalISelAbort);
//   OPT_BOOL(GuaranteedTailCallOpt);
//   OPT_BOOL(IgnoreXCOFFVisibility);
//   OPT_BOOL(JMCInstrument);
//   OPT_INT(LoopAlignment);
//   OPT_BOOL(MisExpect);
//   OPT_BOOL(NoInfsFPMath);
//   OPT_BOOL(NoNaNsFPMath);
//   OPT_BOOL(NoSignedZerosFPMath);
//   OPT_BOOL(NoTrapAfterNoreturn);
//   OPT_BOOL(NoTrappingFPMath);
//   OPT_BOOL(NoZerosInBSS);
//   OPT_BOOL(PPCGenScalarMASSEntries);
//   OPT_BOOL(SeparateNamedSections);
//   OPT_BOOL(SupportsDebugEntryValues);
//   OPT_BOOL(SupportsDefaultOutlining);
//   OPT_BOOL(StackSymbolOrdering);
//   OPT_ENUM(StackUsageOutput);
//   os << "  SwiftAsyncFramePointerMode: " << opts.SwiftAsyncFramePointer << "\n";
//   OPT_ENUM(ThreadModel);
//   OPT_BOOL(TLSSize);
//   OPT_BOOL(TrapUnreachable);
//   OPT_BOOL(UniqueBasicBlockSectionNames);
//   OPT_BOOL(UniqueSectionNames);
//   OPT_BOOL(UnsafeFPMath);
//   OPT_BOOL(UseInitArray);
//   OPT_BOOL(ValueTrackingVariableLocations);
//   OPT_BOOL(VerifyArgABICompliance);
//   OPT_BOOL(XCOFFReadOnlyPointers);
//   OPT_BOOL(XCOFFTracebackTable);
//   OPT_BOOL(XRayFunctionIndex);
// }

// void llvm::dump(const MCTargetOptions &opts, raw_ostream &os) {
//   // This needs to be kept updated with the options in MCTargetOptions. These
//   // are printed in alphabetical order here, though they do not appear in this
//   // order in the TargetOptions object itself.
//   os << "MCTarget Options:\n";
//   OPT_STR(ABIName);
//   OPT_STR(Argv0);
//   OPT_BOOL(AsmVerbose);
//   OPT_STR(AssemblyLanguage);
//   OPT_STR(AsSecureLogFile);
//   OPT_STR(CommandlineArgs);
//   OPT_ENUM(CompressDebugSections);
//   OPT_BOOL(Crel);
//   OPT_BOOL(Dwarf64);
//   OPT_INT(DwarfVersion);
//   OPT_BOOL(EmitCompactUnwindNonCanonical);
//   OPT_ENUM(EmitDwarfUnwind);
//   OPT_BOOL(FDPIC);
//   os << "  IASSearchPaths:";
//   for (StringRef s : opts.IASSearchPaths)
//     os << " " << s;
//   os << "\n";
//   OPT_BOOL(ImplicitMapSyms);
//   OPT_BOOL(MCFatalWarnings);
//   OPT_BOOL(MCIncrementalLinkerCompatible);
//   OPT_BOOL(MCNoWarn);
//   OPT_BOOL(MCNoDeprecatedWarn);
//   OPT_BOOL(MCNoExecStack);
//   OPT_BOOL(MCNoTypeCheck);
//   OPT_BOOL(MCRelaxAll);
//   OPT_BOOL(MCSaveTempLabels);
//   OPT_ENUM(MCUseDwarfDirectory);
//   os << "  OutputAsmVariant: ";
//   if (opts.OutputAsmVariant)
//     os << *opts.OutputAsmVariant;
//   os << "\n";
//   OPT_BOOL(PPCUseFullRegisterNames);
//   OPT_BOOL(PreserveAsmComments);
//   OPT_BOOL(ShowMCEncoding);
//   OPT_BOOL(ShowMCInst);
//   OPT_STR(SplitDwarfFile);
//   OPT_BOOL(X86RelaxRelocations);
//   OPT_BOOL(X86Sse2Avx);
// }
