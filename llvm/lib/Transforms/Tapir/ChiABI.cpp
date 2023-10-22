//===- ChiABI.cpp - Generic interface to various runtime systems--------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the Chi ABI to convert Tapir instructions to calls
// into a generic runtime system to operates on spawned computations as lambdas.
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Tapir/ChiABI.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/TapirTaskInfo.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Tapir/Outline.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/EscapeEnumerator.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/Transforms/Utils/TapirUtils.h"
#include <memory>

using namespace llvm;

#define DEBUG_TYPE "chiabi"

static const std::string CHIABI_PREFIX = "__chiabi";

extern cl::opt<bool> DebugABICalls;

static cl::opt<std::string>
    ClHostBCPath("chi-runtime-bc-path", cl::init(""),
                    cl::desc("Path to the bitcode file for the runtime ABI"),
                    cl::Hidden);

/// Keep the intermediate files around after compilation.
static cl::opt<bool> ClKeepIntermediateFiles(
    "chiabi-keep-files", cl::init(false), cl::Hidden,
    cl::desc("Keep all the intermediate files on disk after"
             "successsful completion of the transforms "
             "various steps."));

/// Use the Target-specific outline processor based on loop hints.
static cl::opt<bool> ClProcessAllLoops(
    "chiabi-process-all-loops", cl::init(false), cl::Hidden,
    cl::desc("Use the ChiABI target-specific loop-outline processor to process "
             "all loops.  Setting this to false means that the target-specific "
             "loop-outline processor will only process loops with the "
             "appropriate loop-hint metadata."));

/// Place all kernels into a single module.
static cl::opt<bool> ClUseSingleKernelModule(
    "chiabi-use-single-kernel-module", cl::init(true), cl::Hidden,
    cl::desc("Place all kernels into a single kernel module."));

static const StringRef StackFrameName = "__rts_sf";

namespace {

// Custom DiagnosticInfo for linking the Chi ABI bitcode file.
class ChiABILinkDiagnosticInfo : public DiagnosticInfo {
  const Module *SrcM;
  const Twine &Msg;

public:
  ChiABILinkDiagnosticInfo(DiagnosticSeverity Severity, const Module *SrcM,
                              const Twine &Msg)
      : DiagnosticInfo(DK_Lowering, Severity), SrcM(SrcM), Msg(Msg) {}
  void print(DiagnosticPrinter &DP) const override {
    DP << "linking module '" << SrcM->getModuleIdentifier() << "': " << Msg;
  }
};

// Custom DiagnosticHandler to handle diagnostics arising when linking the
// Chi ABI bitcode file.
class ChiABIDiagnosticHandler final : public DiagnosticHandler {
  const Module *SrcM;
  DiagnosticHandler *OrigHandler;

public:
  ChiABIDiagnosticHandler(const Module *SrcM, DiagnosticHandler *OrigHandler)
      : SrcM(SrcM), OrigHandler(OrigHandler) {}

  bool handleDiagnostics(const DiagnosticInfo &DI) override {
    if (DI.getKind() != DK_Linker)
      return OrigHandler->handleDiagnostics(DI);

    std::string MsgStorage;
    {
      raw_string_ostream Stream(MsgStorage);
      DiagnosticPrinterRawOStream DP(Stream);
      DI.print(DP);
    }
    return OrigHandler->handleDiagnostics(
        ChiABILinkDiagnosticInfo(DI.getSeverity(), SrcM, MsgStorage));
  }
};

// Structure recording information about runtime ABI functions.
struct RTSFnDesc {
  StringRef FnName;
  FunctionType *FnType;
  FunctionCallee &FnCallee;
};
} // namespace

// Example callbacks to process inputs, for testing.

// This callback does nothing.
static void nullInputsCallback(Function &F, const ValueSet &Inputs,
                               ValueSet &Args, ValueToValueMapTy &InputMap,
                               IRBuilder<> &StoreBuilder,
                               IRBuilder<> &LoadBuilder,
                               IRBuilder<> &StaticAllocaInserter) {
  LLVM_DEBUG(dbgs() << "nullInputsCallback called!\n");
}

// This callback marshals the inputs.
static void marshalInputsCallback(Function &F, const ValueSet &Inputs,
                                  ValueSet &Args, ValueToValueMapTy &InputMap,
                                  IRBuilder<> &StoreBuilder,
                                  IRBuilder<> &LoadBuilder,
                                  IRBuilder<> &StaticAllocaInserter) {
  LLVM_DEBUG(dbgs() << "marshalInputsCallback called!\n");
  LLVMContext &Ctx = F.getContext();
  // Get vectors of inputs and their types to define argument structure.
  SmallVector<Value *, 8> StructInputs;
  SmallVector<Type *, 8> StructIT;
  for (Value *V : Inputs) {
    StructInputs.push_back(V);
    StructIT.push_back(V->getType());
  }
  // Derive type of argument structure.
  StructType *STy = StructType::get(Ctx, StructIT);
  // Allocate the argument structure.
  AllocaInst *Closure = StaticAllocaInserter.CreateAlloca(STy);
  for (unsigned i = 0; i < StructInputs.size(); ++i) {
    // Populate the argument structure.
    StoreBuilder.CreateStore(
        StructInputs[i], StoreBuilder.CreateConstGEP2_32(STy, Closure, 0, i));
    // Load argument from the argument structure.  Store the load into InputMap
    // so that uses of the argument can be remapped to use the loaded value
    // instead.
    InputMap[StructInputs[i]] = LoadBuilder.CreateLoad(
        StructIT[i], LoadBuilder.CreateConstGEP2_32(STy, Closure, 0, i));
  }

  // Modify Args to contain just the argument structure.
  Args.clear();
  Args.insert(Closure);
}

static void nullLoopLaunchCallback(CallBase &CB, SyncInst *LaunchSync) {
  LLVM_DEBUG(dbgs() << "nullLoopLaunchCallback called!\n  call " << CB << "\n");
  if (LaunchSync) {
    LLVM_DEBUG(dbgs() << "  sync found for call in block "
                      << LaunchSync->getParent()->getName() << "\n");
  }
}

void ChiABI::setOptions(const TapirTargetOptions &Options) {
  if (!isa<ChiABIOptions>(Options))
    return;

  const ChiABIOptions &OptionsCast = cast<ChiABIOptions>(Options);

  // Get bitcode file paths and callbacks.
  UseSingleKernelModule = OptionsCast.useSingleKernelModule();
  HostBCPath = OptionsCast.getHostBCPath();
  DeviceBCPath = OptionsCast.getDeviceBCPath();
  InputsCallback = OptionsCast.getInputsCallback();
  LoopLaunchCallback = OptionsCast.getLoopLaunchCallback();
}

ChiABI::ChiABI(Module &M)
    : TapirTarget(M),
      KernelModule(
          Twine(CHIABI_PREFIX + sys::path::filename(M.getName())).str(),
          M.getContext()) {}

static void linkExternalBitcode(Module &M, StringRef BitCodePath,
                                Linker::Flags Flags = Linker::Flags::None) {
  LLVM_DEBUG(dbgs() << "Using external bitcode file for Chi ABI: "
                    << BitCodePath << "\n");
  LLVMContext &C = M.getContext();
  SMDiagnostic SMD;

  // Parse the bitcode file.  This call imports structure definitions, but not
  // function definitions.
  if (std::unique_ptr<Module> ExternalModule =
          parseIRFile(BitCodePath, SMD, C)) {
    // Get the original DiagnosticHandler for this context.
    std::unique_ptr<DiagnosticHandler> OrigDiagHandler =
        C.getDiagnosticHandler();

    // Setup an ChiABIDiagnosticHandler for this context, to handle
    // diagnostics that arise from linking ExternalModule.
    C.setDiagnosticHandler(std::make_unique<ChiABIDiagnosticHandler>(
        ExternalModule.get(), OrigDiagHandler.get()));

    // Link the external module into the current module, copying over global
    // values.
    //
    // TODO: Consider restructuring the import process to use
    // Linker::Flags::LinkOnlyNeeded to copy over only the necessary contents
    // from the external module.
    bool Fail = Linker::linkModules(
        M, std::move(ExternalModule), Flags,
        [](Module &M, const StringSet<> &GVS) {
          for (StringRef GVName : GVS.keys()) {
            LLVM_DEBUG(dbgs() << "Linking global value " << GVName << "\n");
            if (Function *Fn = M.getFunction(GVName)) {
              if (!Fn->isDeclaration() && !Fn->hasComdat())
                // We set the function's linkage as available_externally, so
                // that subsequent optimizations can remove these definitions
                // from the module.  We don't want this module redefining any of
                // these symbols, even if they aren't inlined, because the
                // Chi runtime library will provide those definitions later.
                Fn->setLinkage(Function::AvailableExternallyLinkage);
            } else if (GlobalVariable *G = M.getGlobalVariable(GVName)) {
              if (!G->isDeclaration() && !G->hasComdat())
                G->setLinkage(GlobalValue::AvailableExternallyLinkage);
            }
          }
        });
    if (Fail)
      C.emitError("ChiABI: Failed to link bitcode ABI file: " +
                  Twine(BitCodePath));

    // Restore the original DiagnosticHandler for this context.
    C.setDiagnosticHandler(std::move(OrigDiagHandler));
  } else {
    C.emitError("ChiABI: Failed to parse bitcode ABI file: " +
                Twine(BitCodePath));
  }
}

void ChiABI::prepareModule(bool ProcessingTapirLoops) {
  LLVMContext &C = M.getContext();
  const DataLayout &DL = DestM.getDataLayout();
  Type *Int8Ty = Type::getInt8Ty(C);
  Type *Int16Ty = Type::getInt16Ty(C);
  Type *Int32Ty = Type::getInt32Ty(C);
  Type *Int64Ty = Type::getInt64Ty(C);

  if (ProcessingTapirLoops) {
    // TODO: Figure out if we want to link in any bitcode file at this point
    // when processing Tapir loops.

    // Get the set of runtime functions to get the current iteration index.
    FunctionType *GetIteration8FnTy =
        FunctionType::get(Int8Ty, {Int8Ty, Int8Ty}, false);
    FunctionType *GetIteration16FnTy =
        FunctionType::get(Int16Ty, {Int16Ty, Int16Ty}, false);
    FunctionType *GetIteration32FnTy =
        FunctionType::get(Int32Ty, {Int32Ty, Int32Ty}, false);
    FunctionType *GetIteration64FnTy =
        FunctionType::get(Int64Ty, {Int64Ty, Int64Ty}, false);

    // Create an array of RTS functions, with their associated types and
    // FunctionCallee member variables in the ChiABI class.
    RTSFnDesc RTSFunctions[] = {
        {"__rts_get_iteration_8", GetIteration8FnTy, RTSGetIteration8},
        {"__rts_get_iteration_16", GetIteration16FnTy, RTSGetIteration16},
        {"__rts_get_iteration_32", GetIteration32FnTy, RTSGetIteration32},
        {"__rts_get_iteration_64", GetIteration64FnTy, RTSGetIteration64},
    };

    // Add attributes to internalized functions.
    for (RTSFnDesc FnDesc : RTSFunctions) {
      assert(!FnDesc.FnCallee && "Redefining RTS function");
      FnDesc.FnCallee =
          KernelModule.getOrInsertFunction(FnDesc.FnName, FnDesc.FnType);
      assert(isa<Function>(FnDesc.FnCallee.getCallee()) &&
             "Runtime function is not a function");
      Function *Fn = cast<Function>(FnDesc.FnCallee.getCallee());
      Fn->setDoesNotThrow();
    }
    return;
  }

  // If a runtime bitcode path is given via the command line, use it.
  if ("" != ClHostBCPath)
    HostBCPath = ClHostBCPath;

  if ("" != HostBCPath)
    linkExternalBitcode(M, HostBCPath);

  // Get or create local definitions of RTS structure types.
  const char *StackFrameName = "struct.__rts_stack_frame";
  StackFrameTy = StructType::lookupOrCreate(C, StackFrameName);

  PointerType *StackFramePtrTy = PointerType::getUnqual(StackFrameTy);
  Type *VoidTy = Type::getVoidTy(C);
  Type *VoidPtrTy = Type::getInt8PtrTy(C);

  // Define the types of the RTS functions.
  FunctionType *RTSFnTy = FunctionType::get(VoidTy, {StackFramePtrTy}, false);
  SpawnBodyFnArgTy = VoidPtrTy;
  Type *IntPtrTy = DL.getIntPtrType(C);
  SpawnBodyFnArgSizeTy = IntPtrTy;
  SpawnBodyFnTy = FunctionType::get(VoidTy, {SpawnBodyFnArgTy}, false);
  FunctionType *SpawnFnTy =
      FunctionType::get(VoidTy,
                        {StackFramePtrTy, PointerType::getUnqual(SpawnBodyFnTy),
                         SpawnBodyFnArgTy, SpawnBodyFnArgSizeTy, IntPtrTy},
                        false);
  FunctionType *Grainsize8FnTy = FunctionType::get(Int8Ty, {Int8Ty}, false);
  FunctionType *Grainsize16FnTy = FunctionType::get(Int16Ty, {Int16Ty}, false);
  FunctionType *Grainsize32FnTy = FunctionType::get(Int32Ty, {Int32Ty}, false);
  FunctionType *Grainsize64FnTy = FunctionType::get(Int64Ty, {Int64Ty}, false);
  FunctionType *WorkerInfoTy = FunctionType::get(Int32Ty, {}, false);

  // Create an array of RTS functions, with their associated types and
  // FunctionCallee member variables in the ChiABI class.
  RTSFnDesc RTSFunctions[] = {
      {"__rts_enter_frame", RTSFnTy, RTSEnterFrame},
      {"__rts_spawn", SpawnFnTy, RTSSpawn},
      {"__rts_leave_frame", RTSFnTy, RTSLeaveFrame},
      {"__rts_sync", RTSFnTy, RTSSync},
      {"__rts_sync_nothrow", RTSFnTy, RTSSyncNoThrow},
      {"__rts_loop_grainsize_8", Grainsize8FnTy, RTSLoopGrainsize8},
      {"__rts_loop_grainsize_16", Grainsize16FnTy, RTSLoopGrainsize16},
      {"__rts_loop_grainsize_32", Grainsize32FnTy, RTSLoopGrainsize32},
      {"__rts_loop_grainsize_64", Grainsize64FnTy, RTSLoopGrainsize64},
      {"__rts_get_num_workers", WorkerInfoTy, RTSGetNumWorkers},
      {"__rts_get_worker_id", WorkerInfoTy, RTSGetWorkerID},
  };

  // Add attributes to internalized functions.
  for (RTSFnDesc FnDesc : RTSFunctions) {
    assert(!FnDesc.FnCallee && "Redefining RTS function");
    FnDesc.FnCallee = M.getOrInsertFunction(FnDesc.FnName, FnDesc.FnType);
    assert(isa<Function>(FnDesc.FnCallee.getCallee()) &&
           "Runtime function is not a function");
    Function *Fn = cast<Function>(FnDesc.FnCallee.getCallee());

    Fn->setDoesNotThrow();

    // // Unless we're debugging, mark the function as always_inline.  This
    // // attribute is required for some functions, but is helpful for all
    // // functions.
    // if (!DebugABICalls)
    //   Fn->addFnAttr(Attribute::AlwaysInline);
    // else
    //   Fn->removeFnAttr(Attribute::AlwaysInline);

    if (!Fn->isDeclaration()) {
      if (Fn->getName() == "__rts_get_num_workers" ||
          Fn->getName() == "__rts_get_worker_id") {
        Fn->setLinkage(Function::InternalLinkage);
      }
    }
  }

  // If no valid bitcode file was found fill in the missing pieces.
  // An error should have been emitted already unless the user
  // set DebugABICalls.

  if (StackFrameTy->isOpaque()) {
    // TODO: Figure out better handling of this potential error.
    LLVM_DEBUG(dbgs() << "ChiABI: Failed to find __rts_stack_frame type.\n");
    // Create a dummy __rts_stack_frame structure
    StackFrameTy->setBody(Int64Ty);
  }
  // Create declarations of all RTS functions, and add basic attributes to those
  // declarations.
  for (RTSFnDesc FnDesc : RTSFunctions) {
    if (FnDesc.FnCallee)
      continue;
    FnDesc.FnCallee = M.getOrInsertFunction(FnDesc.FnName, FnDesc.FnType);
    assert(isa<Function>(FnDesc.FnCallee.getCallee()) &&
           "RTS function is not a function");
    Function *Fn = cast<Function>(FnDesc.FnCallee.getCallee());

    Fn->setDoesNotThrow();
  }
}

void ChiABI::addHelperAttributes(Function &Helper) {
  // Inlining the helper function is not legal.
  Helper.removeFnAttr(Attribute::AlwaysInline);
  Helper.addFnAttr(Attribute::NoInline);
  // If the helper uses an argument structure, then it is not a write-only
  // function.
  if (getArgStructMode() != ArgStructMode::None) {
    Helper.removeFnAttr(Attribute::WriteOnly);
    Helper.removeFnAttr(Attribute::ArgMemOnly);
    Helper.removeFnAttr(Attribute::InaccessibleMemOrArgMemOnly);
  }
  // Note that the address of the helper is unimportant.
  Helper.setUnnamedAddr(GlobalValue::UnnamedAddr::Global);

  // The helper is internal to this module.  We use internal linkage, rather
  // than private linkage, so that tools can still reference the helper
  // function.
  Helper.setLinkage(GlobalValue::InternalLinkage);
}

// Check whether the allocation of a __rts_stack_frame can be inserted after
// instruction \p I.
static bool skipInstruction(const Instruction &I) {
  if (isa<AllocaInst>(I))
    return true;

  if (isa<DbgInfoIntrinsic>(I))
    return true;

  if (const IntrinsicInst *II = dyn_cast<IntrinsicInst>(&I)) {
    // Skip simple intrinsics
    switch (II->getIntrinsicID()) {
    case Intrinsic::annotation:
    case Intrinsic::assume:
    case Intrinsic::sideeffect:
    case Intrinsic::invariant_start:
    case Intrinsic::invariant_end:
    case Intrinsic::launder_invariant_group:
    case Intrinsic::strip_invariant_group:
    case Intrinsic::is_constant:
    case Intrinsic::lifetime_start:
    case Intrinsic::lifetime_end:
    case Intrinsic::objectsize:
    case Intrinsic::ptr_annotation:
    case Intrinsic::var_annotation:
    case Intrinsic::experimental_gc_result:
    case Intrinsic::experimental_gc_relocate:
    case Intrinsic::experimental_noalias_scope_decl:
    case Intrinsic::syncregion_start:
    case Intrinsic::taskframe_create:
      return true;
    default:
      return false;
    }
  }

  return false;
}

// Scan the basic block \p B to find a point to insert the allocation of a
// __rts_stack_frame.
static Instruction *getStackFrameInsertPt(BasicBlock &B) {
  BasicBlock::iterator BI(B.getFirstInsertionPt());
  BasicBlock::const_iterator BE(B.end());

  // Scan the basic block for the first instruction we should not skip.
  while (BI != BE) {
    if (!skipInstruction(*BI)) {
      return &*BI;
    }
    ++BI;
  }

  // We reached the end of the basic block; return the terminator.
  return B.getTerminator();
}

/// Create the __rts_stack_frame for the spawning function.
Value *ChiABI::CreateStackFrame(Function &F) {
  const DataLayout &DL = F.getParent()->getDataLayout();
  Type *SFTy = StackFrameTy;

  IRBuilder<> B(getStackFrameInsertPt(F.getEntryBlock()));
  AllocaInst *SF = B.CreateAlloca(SFTy, DL.getAllocaAddrSpace(),
                                  /*ArraySize*/ nullptr,
                                  /*Name*/ StackFrameName);

  SF->setAlignment(StackFrameAlign);

  return SF;
}

Value *ChiABI::GetOrCreateStackFrame(Function &F) {
  if (DetachCtxToStackFrame.count(&F))
    return DetachCtxToStackFrame[&F];

  Value *SF = CreateStackFrame(F);
  DetachCtxToStackFrame[&F] = SF;

  return SF;
}

// Insert a call in Function F to __rts_enter_frame to initialize the
// __rts_stack_frame in F.  If TaskFrameCreate is nonnull, the call to
// __rts_enter_frame is inserted at TaskFrameCreate.
CallInst *ChiABI::InsertStackFramePush(Function &F,
                                          Instruction *TaskFrameCreate,
                                          bool Helper) {
  Instruction *SF = cast<Instruction>(GetOrCreateStackFrame(F));

  BasicBlock::iterator InsertPt = ++SF->getIterator();
  IRBuilder<> B(&(F.getEntryBlock()), InsertPt);
  if (TaskFrameCreate)
    B.SetInsertPoint(TaskFrameCreate);
  if (!B.getCurrentDebugLocation()) {
    // Try to find debug information later in this block for the ABI call.
    BasicBlock::iterator BI = B.GetInsertPoint();
    BasicBlock::const_iterator BE(B.GetInsertBlock()->end());
    while (BI != BE) {
      if (DebugLoc Loc = BI->getDebugLoc()) {
        B.SetCurrentDebugLocation(Loc);
        break;
      }
      ++BI;
    }
  }

  Value *Args[1] = {SF};
  return B.CreateCall(RTSEnterFrame, Args);
}

// Insert a call in Function F to pop the stack frame.
//
// PromoteCallsToInvokes dictates whether call instructions that can throw are
// promoted to invoke instructions prior to inserting the epilogue-function
// calls.
void ChiABI::InsertStackFramePop(Function &F, bool PromoteCallsToInvokes,
                                    bool InsertPauseFrame, bool Helper) {
  Value *SF = GetOrCreateStackFrame(F);
  SmallPtrSet<ReturnInst *, 8> Returns;
  SmallPtrSet<ResumeInst *, 8> Resumes;

  // Add eh cleanup that returns control to the runtime
  EscapeEnumerator EE(F, "rts_cleanup", PromoteCallsToInvokes);
  while (IRBuilder<> *Builder = EE.Next()) {
    if (ResumeInst *RI = dyn_cast<ResumeInst>(Builder->GetInsertPoint())) {
      if (!RI->getDebugLoc())
        // Attempt to set the debug location of this resume to match one of the
        // preceeding terminators.
        for (const BasicBlock *Pred : predecessors(RI->getParent()))
          if (const DebugLoc &Loc = Pred->getTerminator()->getDebugLoc()) {
            RI->setDebugLoc(Loc);
            break;
          }
      Resumes.insert(RI);
    } else if (ReturnInst *RI = dyn_cast<ReturnInst>(Builder->GetInsertPoint()))
      Returns.insert(RI);
  }

  for (ReturnInst *RI : Returns) {
    CallInst::Create(RTSLeaveFrame, {SF}, "", RI)
        ->setDebugLoc(RI->getDebugLoc());
  }
}

/// Lower a call to get the grainsize of a Tapir loop.
Value *ChiABI::lowerGrainsizeCall(CallInst *GrainsizeCall) {
  Value *Limit = GrainsizeCall->getArgOperand(0);
  IRBuilder<> Builder(GrainsizeCall);

  // Select the appropriate __rts_grainsize function, based on the type.
  FunctionCallee RTSGrainsizeCall;
  if (GrainsizeCall->getType()->isIntegerTy(8))
    RTSGrainsizeCall = RTSLoopGrainsize8;
  else if (GrainsizeCall->getType()->isIntegerTy(16))
    RTSGrainsizeCall = RTSLoopGrainsize16;
  else if (GrainsizeCall->getType()->isIntegerTy(32))
    RTSGrainsizeCall = RTSLoopGrainsize32;
  else if (GrainsizeCall->getType()->isIntegerTy(64))
    RTSGrainsizeCall = RTSLoopGrainsize64;
  else
    llvm_unreachable("No RTSGrainsize call matches type for Tapir loop.");

  Value *Grainsize = Builder.CreateCall(RTSGrainsizeCall, Limit);

  // Replace uses of grainsize intrinsic call with this grainsize value.
  GrainsizeCall->replaceAllUsesWith(Grainsize);
  return Grainsize;
}

// Lower a sync instruction SI.
void ChiABI::lowerSync(SyncInst &SI) {
  Function &Fn = *SI.getFunction();
  if (!DetachCtxToStackFrame[&Fn])
    // If we have not created a stackframe for this function, then we don't need
    // to handle the sync.
    return;

  Value *SF = GetOrCreateStackFrame(Fn);
  Value *Args[] = {SF};
  assert(Args[0] && "sync used in function without frame!");

  Instruction *SyncUnwind = nullptr;
  BasicBlock *SyncCont = SI.getSuccessor(0);
  BasicBlock *SyncUnwindDest = nullptr;
  // Determine whether a sync.unwind immediately follows SI.
  if (InvokeInst *II =
          dyn_cast<InvokeInst>(SyncCont->getFirstNonPHIOrDbgOrLifetime())) {
    if (isSyncUnwind(II)) {
      SyncUnwind = II;
      SyncCont = II->getNormalDest();
      SyncUnwindDest = II->getUnwindDest();
    }
  }

  CallBase *CB;
  if (!SyncUnwindDest) {
    if (Fn.doesNotThrow())
      CB = CallInst::Create(RTSSyncNoThrow, Args, "",
                            /*insert before*/ &SI);
    else
      CB = CallInst::Create(RTSSync, Args, "", /*insert before*/ &SI);

    BranchInst::Create(SyncCont, CB->getParent());
  } else {
    CB = InvokeInst::Create(RTSSync, SyncCont, SyncUnwindDest, Args, "",
                            /*insert before*/ &SI);
    for (PHINode &PN : SyncCont->phis())
      PN.addIncoming(PN.getIncomingValueForBlock(SyncUnwind->getParent()),
                     SI.getParent());
    for (PHINode &PN : SyncUnwindDest->phis())
      PN.addIncoming(PN.getIncomingValueForBlock(SyncUnwind->getParent()),
                     SI.getParent());
  }
  CB->setDebugLoc(SI.getDebugLoc());
  SI.eraseFromParent();

  // Mark this function as stealable.
  Fn.addFnAttr(Attribute::Stealable);
}

bool ChiABI::preProcessFunction(Function &F, TaskInfo &TI,
                                   bool ProcessingTapirLoops) {
  return false;
}
void ChiABI::postProcessFunction(Function &F, bool ProcessingTapirLoops) {}
void ChiABI::postProcessHelper(Function &F) {}

void ChiABI::preProcessOutlinedTask(Function &F, Instruction *DetachPt,
                                       Instruction *TaskFrameCreate,
                                       bool IsSpawner, BasicBlock *TFEntry) {
  if (IsSpawner)
    InsertStackFramePush(F, TaskFrameCreate, /*Helper*/ true);
}

void ChiABI::postProcessOutlinedTask(Function &F, Instruction *DetachPt,
                                        Instruction *TaskFrameCreate,
                                        bool IsSpawner, BasicBlock *TFEntry) {
  if (IsSpawner)
    InsertStackFramePop(F, /*PromoteCallsToInvokes*/ true,
                        /*InsertPauseFrame*/ true, /*Helper*/ true);
}

void ChiABI::preProcessRootSpawner(Function &F, BasicBlock *TFEntry) {
  InsertStackFramePush(F);
}

void ChiABI::postProcessRootSpawner(Function &F, BasicBlock *TFEntry) {
  InsertStackFramePop(F, /*PromoteCallsToInvokes*/ false,
                      /*InsertPauseFrame*/ false, /*Helper*/ false);
}

void ChiABI::processSubTaskCall(TaskOutlineInfo &TOI, DominatorTree &DT) {
  const DataLayout &DL = DestM.getDataLayout();
  CallBase *ReplCall = cast<CallBase>(TOI.ReplCall);

  Function &F = *ReplCall->getFunction();
  Value *SF = DetachCtxToStackFrame[&F];
  assert(SF && "No frame found for spawning task");

  // Get the alignment of the helper arguments.  The bitcode-ABI functions may
  // use the alignment to align the shared variables in the storage allocated by
  // the OpenMP runtime, especially to accommodate vector arguments.
  AllocaInst *ArgAlloca = cast<AllocaInst>(ReplCall->getArgOperand(0));
  uint64_t Alignment = DL.getPrefTypeAlignment(ArgAlloca->getAllocatedType());

  IRBuilder<> B(ReplCall);
  Value *FnCast = B.CreateBitCast(ReplCall->getCalledFunction(),
                                  PointerType::getUnqual(SpawnBodyFnTy));
  Value *ArgCast =
      B.CreateBitOrPointerCast(ReplCall->getArgOperand(0), SpawnBodyFnArgTy);
  auto ArgSize =
      cast<AllocaInst>(ReplCall->getArgOperand(0))->getAllocationSizeInBits(DL);
  assert(ArgSize &&
         "Could not determine size of compiler-generated ArgStruct.");
  Value *ArgSizeVal = ConstantInt::get(SpawnBodyFnArgSizeTy, *ArgSize / 8);

  if (InvokeInst *II = dyn_cast<InvokeInst>(ReplCall)) {
    B.CreateInvoke(RTSSpawn, II->getNormalDest(), II->getUnwindDest(),
                   {SF, FnCast, ArgCast, ArgSizeVal, B.getInt64(Alignment)});
  } else {
    B.CreateCall(RTSSpawn,
                 {SF, FnCast, ArgCast, ArgSizeVal, B.getInt64(Alignment)});
  }

  ReplCall->eraseFromParent();
}

LoopOutlineProcessor *
ChiABI::getLoopOutlineProcessor(const TapirLoopInfo *TL) {
  Loop *TheLoop = TL->getLoop();
  if (!ClProcessAllLoops) {
    // Check metadata for whether ChiLoop should handle this Tapir loop.
    TapirLoopHints Hints(TheLoop);
    if (TapirLoopHints::ST_TGT != Hints.getStrategy())
      // Use default loop-outline processor.
      return nullptr;
  }

  // Create a Chi loop outline processor for transforming parallel Tapir loops
  // into suitable bitcode to transform for a device.  We hand the outliner the
  // kernel module (KernelModule) as the destination for all generated
  // (device-side) code.
  LLVM_DEBUG(dbgs() << "chiabi: creating loop outline processor\n");

  ChiLoop *Outliner;
  if (ClUseSingleKernelModule && UseSingleKernelModule) {
    Outliner = new ChiLoop(M, KernelModule, // KernelName,
                           this);
  } else {
    Outliner = new ChiLoop(
        M,
        std::make_unique<Module>(
            Twine(CHIABI_PREFIX + sys::path::filename(M.getName())).str(),
            M.getContext()),
        this);
  }
  // Outliner->setInputsCallback(nullInputsCallback);
  // Outliner->setInputsCallback(marshalInputsCallback);
  // Outliner->LoopLaunchCallback = nullLoopLaunchCallback;
  if (InputsCallback)
    Outliner->setInputsCallback(InputsCallback);
  if (LoopLaunchCallback)
    Outliner->setLoopLaunchCallback(LoopLaunchCallback);

  return Outliner;
}

/// Static ID for kernel naming -- each encountered kernel (loop)
/// during compilation will receive a unique ID.  TODO: This is
/// a not so great naming mechanism and certainly not thread safe...
// unsigned ChiLoop::NextKernelID = 0;

ChiLoop::ChiLoop(Module &M, Module &KernelModule, ChiABI *TT,
                 bool MakeUniqueName)
    : LoopOutlineProcessor(M, KernelModule), TTarget(TT),
      KernelModule(KernelModule) {

  LLVM_DEBUG(dbgs() << "chiabi: creating loop outliner:\n"
                    << "\tmodule     : " << KernelModule.getName() << "\n\n");

  RTSGetIteration8 = TTarget->RTSGetIteration8;
  RTSGetIteration16 = TTarget->RTSGetIteration16;
  RTSGetIteration32 = TTarget->RTSGetIteration32;
  RTSGetIteration64 = TTarget->RTSGetIteration64;
}

ChiLoop::ChiLoop(Module &M, std::unique_ptr<Module> LocalModule, ChiABI *TT,
                 bool MakeUniqueName)
    : LoopOutlineProcessor(M, *LocalModule), TTarget(TT),
      LocalKernelModule(std::move(LocalModule)),
      KernelModule(*LocalKernelModule) {

  LLVM_DEBUG(
      dbgs() << "chiabi: creating loop outliner with per-loop kernel module:\n"
             << "\tmodule     : " << KernelModule.getName() << "\n\n");

  // TODO: Figure out if we want to link in any bitcode file at this point
  // when processing Tapir loops.

  // When using separate kernel modules per loop, add the generic rts functions
  // to each kernel.
  LLVMContext &C = KernelModule.getContext();
  Type *Int8Ty = Type::getInt8Ty(C);
  Type *Int16Ty = Type::getInt16Ty(C);
  Type *Int32Ty = Type::getInt32Ty(C);
  Type *Int64Ty = Type::getInt64Ty(C);

  // Get the set of runtime functions to get the current iteration index.
  FunctionType *GetIteration8FnTy =
      FunctionType::get(Int8Ty, {Int8Ty, Int8Ty}, false);
  FunctionType *GetIteration16FnTy =
      FunctionType::get(Int16Ty, {Int16Ty, Int16Ty}, false);
  FunctionType *GetIteration32FnTy =
      FunctionType::get(Int32Ty, {Int32Ty, Int32Ty}, false);
  FunctionType *GetIteration64FnTy =
      FunctionType::get(Int64Ty, {Int64Ty, Int64Ty}, false);

  // Create an array of RTS functions, with their associated types and
  // FunctionCallee member variables in the ChiABI class.
  RTSFnDesc RTSFunctions[] = {
      {"__rts_get_iteration_8", GetIteration8FnTy, RTSGetIteration8},
      {"__rts_get_iteration_16", GetIteration16FnTy, RTSGetIteration16},
      {"__rts_get_iteration_32", GetIteration32FnTy, RTSGetIteration32},
      {"__rts_get_iteration_64", GetIteration64FnTy, RTSGetIteration64},
  };

  // Add attributes to internalized functions.
  for (RTSFnDesc FnDesc : RTSFunctions) {
    assert(!FnDesc.FnCallee && "Redefining RTS function");
    FnDesc.FnCallee =
        KernelModule.getOrInsertFunction(FnDesc.FnName, FnDesc.FnType);
    assert(isa<Function>(FnDesc.FnCallee.getCallee()) &&
           "Runtime function is not a function");
    Function *Fn = cast<Function>(FnDesc.FnCallee.getCallee());
    Fn->setDoesNotThrow();
  }
}

static std::set<GlobalValue *> &collect(Constant &C,
                                        std::set<GlobalValue *> &Seen);

static std::set<GlobalValue *> &collect(BasicBlock &BB,
                                        std::set<GlobalValue *> &Seen) {
  for (auto &Inst : BB)
    for (auto &Op : Inst.operands())
      if (auto *C = dyn_cast<Constant>(&Op))
        collect(*C, Seen);
  return Seen;
}

static std::set<GlobalValue *> &collect(Function &F,
                                        std::set<GlobalValue *> &Seen) {
  Seen.insert(&F);

  for (auto &BB : F)
    collect(BB, Seen);
  return Seen;
}

static std::set<GlobalValue *> &collect(GlobalVariable &G,
                                        std::set<GlobalValue *> &Seen) {
  Seen.insert(&G);

  if (G.hasInitializer())
    collect(*G.getInitializer(), Seen);
  return Seen;
}

static std::set<GlobalValue *> &collect(GlobalIFunc &G,
                                        std::set<GlobalValue *> &Seen) {
  Seen.insert(&G);

  llvm_unreachable("chiabi: GNU IFUNC not yet supported");
  return Seen;
}

static std::set<GlobalValue *> &collect(GlobalAlias &G,
                                        std::set<GlobalValue *> &Seen) {
  Seen.insert(&G);

  llvm_unreachable("chiabi: GlobalAlias not yet supported");
  return Seen;
}

static std::set<GlobalValue *> &collect(BlockAddress &BlkAddr,
                                        std::set<GlobalValue *> &Seen) {
  if (Function *F = BlkAddr.getFunction())
    collect(*F, Seen);
  if (BasicBlock *BB = BlkAddr.getBasicBlock())
    collect(*BB, Seen);
  return Seen;
}

std::set<GlobalValue *> &collect(Constant &C, std::set<GlobalValue *> &Seen) {
  if (GlobalValue *G = dyn_cast<GlobalValue>(&C))
    if (Seen.find(G) != Seen.end())
      return Seen;

  if (auto *F = dyn_cast<Function>(&C))
    return collect(*F, Seen);
  else if (auto *G = dyn_cast<GlobalVariable>(&C))
    return collect(*G, Seen);
  else if (auto *G = dyn_cast<GlobalAlias>(&C))
    return collect(*G, Seen);
  else if (auto *G = dyn_cast<GlobalIFunc>(&C))
    return collect(*G, Seen);
  else if (auto *BlkAddr = dyn_cast<BlockAddress>(&C))
    return collect(*BlkAddr, Seen);
  else
    for (auto &Op : C.operands())
      if (auto *COp = dyn_cast<Constant>(Op))
        collect(*COp, Seen);
  return Seen;
}

void ChiLoop::preProcessTapirLoop(TapirLoopInfo &TL, ValueToValueMapTy &VMap) {
  LLVM_DEBUG(dbgs() << "\tchiabi: preprocessing " << TL.getLoop()
                    << "  in module '" << KernelModule.getName() << "'.\n");

  // Collect the top-level entities (Function, GlobalVariable, GlobalAlias
  // and GlobalIFunc) that are used in the outlined loop. Since the outlined
  // loop will live in the KernelModule, any GlobalValue's used in it will
  // need to be cloned into the KernelModule.
  LLVM_DEBUG(dbgs() << "\t\t- gathering and analyzing global values...\n");
  std::set<GlobalValue *> UsedGlobalValues;
  Loop &L = *TL.getLoop();
  for (Loop *SL : L)
    for (BasicBlock *BB : SL->blocks())
      collect(*BB, UsedGlobalValues);

  for (BasicBlock *BB : L.blocks())
    collect(*BB, UsedGlobalValues);

  // Clone global variables (TODO: and aliases).
  for (GlobalValue *V : UsedGlobalValues) {
    if (GlobalVariable *GV = dyn_cast<GlobalVariable>(V)) {
      // TODO: Make sure this logic makes sense.
      // We don't necessarily need a device-side clone of a global variable --
      // instead we need a location where we can copy symbol information over
      // from the host.
      GlobalVariable *NewGV = nullptr;
      // If GV is a constant we can clone the entire variable over, including
      // the initalizer details, and deal with it as an internal variable (i.e.,
      // no need to coordinate with host).
      // TODO: make sure this is sound!
      if (GV->isConstant()) {
        NewGV = new GlobalVariable(
            KernelModule, GV->getValueType(), /* isConstant*/ true,
            GlobalValue::InternalLinkage, GV->getInitializer(),
            GV->getName() + "_devvar", (GlobalVariable *)nullptr,
            GlobalValue::NotThreadLocal);
      } else {
        // If GV is non-constant we will need to create a device-side version
        // that will have the host-side value copied over prior to launching the
        // cooresponding kernel.
        NewGV = new GlobalVariable(
            KernelModule, GV->getValueType(), /* isConstant*/ false,
            GlobalValue::ExternalWeakLinkage,
            (Constant *)Constant::getNullValue(GV->getValueType()),
            GV->getName() + "_devvar", (GlobalVariable *)nullptr,
            GlobalValue::NotThreadLocal);
      }
      NewGV->setAlignment(GV->getAlign());
      VMap[GV] = NewGV;
      LLVM_DEBUG(dbgs() << "\t\t\tcreated device-side global variable '"
                        << NewGV->getName() << "'.\n");
    } else if (dyn_cast<GlobalAlias>(V))
      llvm_unreachable("chiabi: fatal error, GlobalAlias not implemented!");
  }

  // Create declarations for all functions first. These may be needed in the
  // global variables and aliases.
  for (GlobalValue *G : UsedGlobalValues) {
    if (Function *F = dyn_cast<Function>(G)) {
      Function *DeviceF = KernelModule.getFunction(F->getName());
      if (not DeviceF) {
        LLVM_DEBUG(dbgs() << "\tanalyzing missing (device-side) function '"
                          << F->getName() << "'.\n");
        // Function *LF = resolveLibDeviceFunction(F);
        // if (LF && not KernelModule.getFunction(LF->getName())) {
        //   LLVM_DEBUG(dbgs() << "\ttransformed to libdevice function '"
        //                     << LF->getName() << "'.\n");
        //   DeviceF = Function::Create(LF->getFunctionType(), F->getLinkage(),
        //                              LF->getName(), KernelModule);
        // } else {
          LLVM_DEBUG(dbgs() << "\tcreated device function '" << F->getName()
                            << "'.\n");
          DeviceF = Function::Create(F->getFunctionType(), F->getLinkage(),
                                     F->getName(), KernelModule);
        // }
      }

      for (size_t i = 0; i < F->arg_size(); i++) {
        Argument *Arg = F->getArg(i);
        Argument *NewA = DeviceF->getArg(i);
        NewA->setName(Arg->getName());
        VMap[Arg] = NewA;
      }
      VMap[F] = DeviceF;
    }
  }

  // FIXME: Support GlobalIFunc at some point. This is a GNU extension, so we
  // may not want to support it at all, but just in case, this is here.
  for (GlobalValue *V : UsedGlobalValues) {
    if (isa<GlobalIFunc>(V)) {
      llvm_unreachable("chiabi: GlobalIFunc not yet supported.");
    }
  }

  // Now clone any function bodies that need to be cloned. This should be
  // done as late as possible so that the VMap is populated with any other
  // global values that need to be remapped.
  for (GlobalValue *V : UsedGlobalValues) {
    if (Function *F = dyn_cast<Function>(V)) {
      if (F->size() && not F->isIntrinsic()) {
        SmallVector<ReturnInst *, 8> Returns;
        Function *DeviceF = cast<Function>(VMap[F]);
        LLVM_DEBUG(dbgs() << "chiabi: cloning device function '"
                          << DeviceF->getName() << "' into kernel module.\n");
        CloneFunctionInto(DeviceF, F, VMap,
                          CloneFunctionChangeType::DifferentModule, Returns);
      }
    }
  }

  LLVM_DEBUG(dbgs() << "\tfinished preprocessing tapir loop.\n");
  if (ClKeepIntermediateFiles) {
    std::error_code EC;
    std::unique_ptr<ToolOutputFile> PreLoopIRFile;
    SmallString<255> IRFileName("preprocess-loop.ll");
    PreLoopIRFile = std::make_unique<ToolOutputFile>(
        IRFileName, EC, sys::fs::OpenFlags::OF_None);
    M.print(PreLoopIRFile->os(), nullptr);
    PreLoopIRFile->keep();
  }
}

void ChiLoop::postProcessOutline(TapirLoopInfo &TLI, TaskOutlineInfo &Out,
                                 ValueToValueMapTy &VMap) {
  Task *T = TLI.getTask();
  Loop *TL = TLI.getLoop();

  BasicBlock *Entry = cast<BasicBlock>(VMap[TL->getLoopPreheader()]);
  BasicBlock *Header = cast<BasicBlock>(VMap[TL->getHeader()]);
  BasicBlock *Exit = cast<BasicBlock>(VMap[TLI.getExitBlock()]);
  PHINode *PrimaryIV = cast<PHINode>(VMap[TLI.getPrimaryInduction().first]);
  Value *PrimaryIVInput = PrimaryIV->getIncomingValueForBlock(Entry);

  // We no longer need the cloned sync region.
  Instruction *ClonedSyncReg =
      cast<Instruction>(VMap[T->getDetach()->getSyncRegion()]);
  ClonedSyncReg->eraseFromParent();

  Function *KernelF = Out.Outline;
  LLVM_DEBUG(dbgs() << "Post-processing outlined function "
                    << KernelF->getName() << "\n"
                    << KernelModule);

  // // Get the kernel function for this loop and clean up
  // // any stray (target related) attributes that were
  // // attached as part of the host-side target that
  // // occurred before outlining.
  // KernelF->removeFnAttr("target-cpu");
  // KernelF->removeFnAttr("target-features");
  // KernelF->removeFnAttr("personality");
  // KernelF->addFnAttr("target-cpu", GPUArch);
  // KernelF->addFnAttr("target-features",
  //                    PTXVersionFromCudaVersion() + "," + GPUArch);
  // NamedMDNode *Annotations =
  //     KernelModule.getOrInsertNamedMetadata("nvvm.annotations");
  // SmallVector<Metadata *, 3> AV;
  // AV.push_back(ValueAsMetadata::get(KernelF));
  // AV.push_back(MDString::get(Ctx, "kernel"));
  // AV.push_back(
  //     ValueAsMetadata::get(ConstantInt::get(Type::getInt32Ty(Ctx), 1)));
  // Annotations->addOperand(MDNode::get(Ctx, AV));

  // Verify that the Thread ID corresponds to a valid iteration.  Because
  // Tapir loops use canonical induction variables, valid iterations range
  // from 0 to the loop limit with stride 1.  The End argument encodes the
  // loop limit. Get end and grainsize arguments
  Argument *End;
  Value *Grainsize;
  {
    // TODO: We really only want a grainsize of 1 for now...
    auto OutlineArgsIter = KernelF->arg_begin();
    // End argument is the first LC arg.
    End = &*(OutlineArgsIter +
             getLimitArgIndex(*Entry->getParent(), Out.InputSet));

    // Get the grainsize value, which is either constant or the third LC
    // arg.
    Grainsize = ConstantInt::get(PrimaryIV->getType(), 1);
  }

  IRBuilder<> B(Entry->getTerminator());

  FunctionCallee RTSGetIterationCall;
  if (PrimaryIV->getType()->isIntegerTy(8))
    RTSGetIterationCall = RTSGetIteration8;
  else if (PrimaryIV->getType()->isIntegerTy(16))
    RTSGetIterationCall = RTSGetIteration16;
  else if (PrimaryIV->getType()->isIntegerTy(32))
    RTSGetIterationCall = RTSGetIteration32;
  else if (PrimaryIV->getType()->isIntegerTy(64))
    RTSGetIterationCall = RTSGetIteration64;
  else
    llvm_unreachable("No RTSGetIteration call matches type for Tapir loop");

  Value *RTSIteration =
      B.CreateCall(RTSGetIterationCall, {PrimaryIVInput, Grainsize});
  Value *RTSEnd = B.CreateAdd(RTSIteration, Grainsize, "__rts_end");
  Value *Cond = B.CreateICmpUGE(RTSIteration, RTSEnd, "__rts_end_cond");
  ReplaceInstWithInst(Entry->getTerminator(),
                      BranchInst::Create(Exit, Header, Cond));

  PrimaryIVInput->replaceAllUsesWith(RTSIteration);
  
  // Update cloned loop condition to use the thread-end value.
  unsigned TripCountIdx = 0;
  ICmpInst *ClonedCond = cast<ICmpInst>(VMap[TLI.getCondition()]);
  if (ClonedCond->getOperand(0) != End)
    ++TripCountIdx;
  assert(ClonedCond->getOperand(TripCountIdx) == End &&
         "End argument not used in condition!");
  ClonedCond->setOperand(TripCountIdx, RTSEnd);

  if (LocalKernelModule) {
    if ("" != TTarget->DeviceBCPath) {
      LLVM_DEBUG(dbgs() << "\t- linking into kernel module device bitcode: "
                        << TTarget->DeviceBCPath << "\n");
      linkExternalBitcode(KernelModule, TTarget->DeviceBCPath,
                          Linker::LinkOnlyNeeded);
    }

    // If creating a separate kernel module for each loop, write that kernel
    // module's bitcode to a global variable in the host module now.
    SmallString<256 * 1024> Buffer;
    raw_svector_ostream OS(Buffer);
    WriteBitcodeToFile(KernelModule, OS);
    Constant *KMArray = ConstantDataArray::getRaw(
        Buffer, Buffer.size(), Type::getInt8Ty(M.getContext()));
    GlobalVariable *KMGV = new GlobalVariable(
        M, KMArray->getType(), true, GlobalValue::InternalLinkage, KMArray,
        Twine("__chiabi_kernel_module." + KernelF->getName()).str());
    appendToCompilerUsed(M, KMGV);
  }

  if (ClKeepIntermediateFiles) {
    std::error_code EC;
    std::unique_ptr<ToolOutputFile> PostLoopIRFile;
    SmallString<255> IRFileName("post-loop.ll");
    PostLoopIRFile = std::make_unique<ToolOutputFile>(
        IRFileName, EC, sys::fs::OpenFlags::OF_None);
    KernelModule.print(PostLoopIRFile->os(), nullptr);
    PostLoopIRFile->keep();
  }
}

void ChiLoop::processOutlinedLoopCall(TapirLoopInfo &TL, TaskOutlineInfo &TOI,
                                      DominatorTree &DT) {
  LLVM_DEBUG(dbgs() << "\tprocessing outlined loop call for '"
                    << TOI.Outline->getName() << "\n");

  CallBase *CB = cast<CallBase>(TOI.ReplCall);
  BasicBlock *Exit = TL.getExitBlock();
  SyncInst *LoopSync = nullptr;
  // TODO: Perform a more elaborate analysis to find a sync for this loop.
  if (SyncInst *SI = dyn_cast<SyncInst>(Exit->getTerminator()))
    if (SI->getSyncRegion() == TL.getTask()->getDetach()->getSyncRegion())
      LoopSync = SI;

  // Swap our the call with a call to a locally declared function with the same
  // name and type as the function in the destination module.  This ensures that
  // this module is valid LLVM IR.
  FunctionCallee PlaceholderF = M.getOrInsertFunction(
      CB->getCalledFunction()->getName(), CB->getFunctionType());
  CB->setCalledFunction(cast<Function>(PlaceholderF.getCallee()));

  // Call the loop-launch callback, if it exists.
  if (LoopLaunchCallback) {
    LoopLaunchCallback(*CB, LoopSync);
    return;
  }

  // TODO: It's not clear what should be done about the callsite if no callback
  // is available.
}

/// @brief Wite the given module to a file as readable IR.
/// @param M - the module to save.
/// @param Filename - optional file name (empty string uses module name).
/// TODO: Move this to a common location for utilities.
static void saveModuleToFile(const Module *M,
                             const std::string &FileName = "") {
  std::error_code EC;
  SmallString<256> IRFileName;
  if (FileName.empty())
    IRFileName = Twine(sys::path::filename(M->getName())).str() + ".chiabi.ll";
  else
    IRFileName = Twine(FileName).str() + ".chiabi.ll";

  std::unique_ptr<ToolOutputFile> IRFile = std::make_unique<ToolOutputFile>(
      IRFileName, EC, sys::fs::OpenFlags::OF_None);
  if (not EC) {
    M->print(IRFile->os(), nullptr);
    IRFile->keep();
  } else
    errs() << "warning: unable to save module '" << IRFileName.c_str() << "'\n";
}

void ChiABI::postProcessModule() {
  if (!ClUseSingleKernelModule || !UseSingleKernelModule) {
    LLVM_DEBUG(dbgs() << "Local kernel modules used.  Skipping kernel-module "
                         "post-processing.\n");
    if ("" != HostBCPath) {
      LLVM_DEBUG(dbgs() << "\t- linking into host module bitcode: " << HostBCPath
                 << "\n");
      linkExternalBitcode(M, HostBCPath, Linker::LinkOnlyNeeded);
    }
    return;
  }

  // At this point, all Tapir constructs in the input module (M) have been
  // transformed (i.e., outlined) into the kernel module.
  // NOTE: postProcessModule() will not be called in cases where parallelism
  // was not discovered during loop spawning.
  LLVM_DEBUG(dbgs() << "\n\n"
                    << "chiabi: postprocessing the kernel '"
                    << KernelModule.getName() << "' and input '" << M.getName()
                    << "' modules.\n");
  LLVM_DEBUG(saveModuleToFile(&KernelModule, KernelModule.getName().str() +
                                                 ".post.unoptimized"));

  if (!KernelModule.functions().empty()) {
    if ("" != DeviceBCPath) {
      LLVM_DEBUG(dbgs() << "\t- linking into kernel module device bitcode: "
                        << DeviceBCPath << "\n");
      linkExternalBitcode(KernelModule, DeviceBCPath, Linker::LinkOnlyNeeded);
    }

    SmallString<256 * 1024> Buffer;
    raw_svector_ostream OS(Buffer);
    WriteBitcodeToFile(KernelModule, OS);
    Constant *KMArray = ConstantDataArray::getRaw(
        Buffer, Buffer.size(), Type::getInt8Ty(M.getContext()));
    GlobalVariable *KMGV = new GlobalVariable(
        M, KMArray->getType(), true, GlobalValue::InternalLinkage, KMArray,
        "__chiabi_kernel_module");
    appendToCompilerUsed(M, KMGV);
  } else {
    LLVM_DEBUG(
        dbgs() << "Skipping embedding of empty unified kernel module.\n");
  }

  if ("" != HostBCPath) {
    LLVM_DEBUG(dbgs() << "\t- linking into host module bitcode: " << HostBCPath
                      << "\n");
    linkExternalBitcode(M, HostBCPath, Linker::LinkOnlyNeeded);
  }
}
