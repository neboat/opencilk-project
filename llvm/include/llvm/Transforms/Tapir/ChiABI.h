//===- ChiABI.h - Generic interface to runtime systems -------*- C++ -*--=//
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
#ifndef CHI_ABI_H_
#define CHI_ABI_H_

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Transforms/Tapir/LoweringUtils.h"
#include "llvm/Transforms/Tapir/TapirLoopInfo.h"
#include "llvm/Transforms/Tapir/TapirTargetOptions.h"
#include "llvm/Support/ToolOutputFile.h"
#include <memory>

namespace llvm {
class Value;
class TapirLoopInfo;
class ChiLoop;

class ChiABI final : public TapirTarget {
  friend class ChiLoop;
  ValueToValueMapTy DetachCtxToStackFrame;

  // User-defined options
  bool UseSingleKernelModule = true;
  StringRef HostBCPath = "";
  StringRef DeviceBCPath = "";
  LoopLaunchCallbackTy LoopLaunchCallback;

  // Separate kernel module
  Module KernelModule;

  // Runtime stack structure
  StructType *StackFrameTy = nullptr;
  FunctionType *SpawnBodyFnTy = nullptr;
  Type *SpawnBodyFnArgTy = nullptr;
  Type *SpawnBodyFnArgSizeTy = nullptr;

  // Runtime functions
  FunctionCallee RTSEnterFrame = nullptr;
  FunctionCallee RTSEnterHelperFrame = nullptr;
  FunctionCallee RTSSpawn = nullptr;
  FunctionCallee RTSLeaveFrame = nullptr;
  FunctionCallee RTSLeaveHelperFrame = nullptr;
  FunctionCallee RTSSync = nullptr;
  FunctionCallee RTSSyncNoThrow = nullptr;

  FunctionCallee RTSLoopGrainsize8 = nullptr;
  FunctionCallee RTSLoopGrainsize16 = nullptr;
  FunctionCallee RTSLoopGrainsize32 = nullptr;
  FunctionCallee RTSLoopGrainsize64 = nullptr;

  FunctionCallee RTSGetIteration8 = nullptr;
  FunctionCallee RTSGetIteration16 = nullptr;
  FunctionCallee RTSGetIteration32 = nullptr;
  FunctionCallee RTSGetIteration64 = nullptr;

  FunctionCallee RTSGetNumWorkers = nullptr;
  FunctionCallee RTSGetWorkerID = nullptr;

  Align StackFrameAlign{8};

  Value *CreateStackFrame(Function &F);
  Value *GetOrCreateStackFrame(Function &F);

  CallInst *InsertStackFramePush(Function &F,
                                 Instruction *TaskFrameCreate = nullptr,
                                 bool Helper = false);
  void InsertStackFramePop(Function &F, bool PromoteCallsToInvokes,
                           bool InsertPauseFrame, bool Helper);

public:
  ChiABI(Module &M);
  ~ChiABI() { DetachCtxToStackFrame.clear(); }

  void setOptions(const TapirTargetOptions &Options) override final;

  void prepareModule(bool ProcessingTapirLoops) override final;
  void postProcessModule() override final;

  Value *lowerGrainsizeCall(CallInst *GrainsizeCall) override final;
  void lowerSync(SyncInst &SI) override final;
  // void lowerReducerOperation(CallBase *CI) override;

  ArgStructMode getArgStructMode() const override final {
    return ArgStructMode::None;
  }
  void addHelperAttributes(Function &F) override final;

  bool preProcessFunction(Function &F, TaskInfo &TI,
                          bool ProcessingTapirLoops) override final;
  void postProcessFunction(Function &F,
                           bool ProcessingTapirLoops) override final;
  void postProcessHelper(Function &F) override final;

  void preProcessOutlinedTask(Function &F, Instruction *DetachPt,
                              Instruction *TaskFrameCreate, bool IsSpawner,
                              BasicBlock *TFEntry) override final;
  void postProcessOutlinedTask(Function &F, Instruction *DetachPt,
                               Instruction *TaskFrameCreate, bool IsSpawner,
                               BasicBlock *TFEntry) override final;
  void preProcessRootSpawner(Function &F, BasicBlock *TFEntry) override final;
  void postProcessRootSpawner(Function &F, BasicBlock *TFEntry) override final;
  void processSubTaskCall(TaskOutlineInfo &TOI,
                          DominatorTree &DT) override final;

  LoopOutlineProcessor *getLoopOutlineProcessor(const TapirLoopInfo *TL)
                          override final;
};

class ChiLoop : public LoopOutlineProcessor {
  friend class ChiABI;

  ChiABI *TTarget = nullptr;

  std::unique_ptr<Module> LocalKernelModule = nullptr;

  // static unsigned NextKernelID;    // Give the generated kernel a unique ID.
  // unsigned KernelID;               // Unique ID for this transformed loop.
  // std::string KernelName;          // A unique name for the kernel.
  Module &KernelModule;               // External module holds the generated kernels.

  // Runtime functions
  FunctionCallee RTSGetIteration8 = nullptr;
  FunctionCallee RTSGetIteration16 = nullptr;
  FunctionCallee RTSGetIteration32 = nullptr;
  FunctionCallee RTSGetIteration64 = nullptr;

  // FunctionCallee RTSGetIteration = nullptr;

public:
  ChiLoop(Module &M,   // Input module (host side)
          Module &KM,  // Target module for device code
          // const std::string &KernelName, // Kernel name
          ChiABI *TT, // Target
          bool MakeUniqueName = true);
  ChiLoop(Module &M,  // Input module (host side)
          std::unique_ptr<Module> LocalModule,
          ChiABI *TT, // Target
          bool MakeUniqueName = true);
  ~ChiLoop();

  // std::string getKernelName() const { return KernelName; }
  // unsigned getKernelID() const { return KernelID; }

  void preProcessTapirLoop(TapirLoopInfo &TL,
                           ValueToValueMapTy &VMap) override;
  void postProcessOutline(TapirLoopInfo &TL, TaskOutlineInfo & Out,
                          ValueToValueMapTy &VMap) override final;
  void processOutlinedLoopCall(TapirLoopInfo &TL, TaskOutlineInfo & TOI,
                               DominatorTree &DT) override final;
};
} // namespace llvm

#endif // CHI_ABI_H
