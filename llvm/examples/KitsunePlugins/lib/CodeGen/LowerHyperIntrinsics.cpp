//===- LowerHyperIntrinsics.cpp - Lower hyperobject intrinsics ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Lower Kitsune's hyperobject intrinsics
//
//===----------------------------------------------------------------------===//

#include "kitsune/CodeGen/LowerHyperIntrinsics.h"
#include "kitsune/Analysis/TapirTargetAnalysis.h"
#include "kitsune/Core/IntrinsicUtils.h"
#include "kitsune/Core/Tapir.h"
#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"
#include "llvm/Transforms/Utils/BuildLibCalls.h"
#include "llvm/Transforms/Utils/TapirUtils.h"

#define DEBUG_TYPE "kit-lower-hyper"

using namespace llvm;

namespace {

/// Main implementation class to lower Kitsune intrinsics.
class LowerHyperIntrinsics {
private:
  const TapirTargetInfo &TGI;

  using HostDeviceViewMapT =
      DenseMap<Value *, std::tuple<Value *, Value *, Value*>>;
  HostDeviceViewMapT HDViews;

  SmallVector<CallInst *, 8> ToErase;

  bool lowerReducerSetup(CallInst &Call, bool IsHolder) {
    LLVMContext &Ctx = Call.getContext();
    Module &M = *Call.getModule();
    Type *I64Ty = Type::getInt64Ty(Ctx);
    PointerType *VoidPtrTy = PointerType::getUnqual(Ctx);
    PointerType *VoidPtrPtrTy = VoidPtrTy;

    BasicBlock &EntryBB = Call.getParent()->getParent()->getEntryBlock();
    IRBuilder<> EntryBuilder(EntryBB.getTerminator());
    IRBuilder<> Builder(&Call);

    FunctionCallee KitCudaMemAllocAndCopyToDevFn =
        M.getOrInsertFunction("__kitcuda_mem_alloc_and_copy_to_device",
                              VoidPtrTy,    // return the stream
                              VoidPtrTy,    // host pointer
                              I64Ty,        // number of bytes to copy
                              VoidPtrPtrTy, // device pointer
                              VoidPtrTy);   // opaque stream

    // Extract information about this reducer.
    Value *Reducer = Call.getArgOperand(0);
    Value *SizeArg = Call.getArgOperand(1);
    // size_t ViewSize = 0;
    // if (auto *ConstSize = dyn_cast<ConstantInt>(SizeArg))
    //   ViewSize = ConstSize->getZExtValue();
    bool AllocateMutex =
        !IsHolder && (!isa<ConstantInt>(SizeArg) ||
                      (dyn_cast<ConstantInt>(SizeArg)->getZExtValue() > 8));
    // For larger views, need to allocate an additional 4 bytes for a mutex.
    // size_t AllocSize = AllocateMutex ? ViewSize + 4 : ViewSize;
    Value *AllocSize =
        !AllocateMutex ? SizeArg
                       : Builder.CreateAdd(SizeArg, ConstantInt::get(I64Ty, 4));
    Function *IdFn = dyn_cast<Function>(Call.getArgOperand(2));
    // Function *MergeFn = dyn_cast<Function>(Call.getArgOperand(3));
    Value *Stream = Call.getArgOperand(4);

    // Allocate a host-side temporary for this reducer view.
    // Type *ViewTy = ArrayType::get(Type::getInt8Ty(Ctx), AllocSize);
    Type *ViewTy = Type::getInt8Ty(Ctx);
    // AllocaInst *HostViewBasePtr = EntryBuilder.CreateAlloca(
    //     ViewTy, nullptr, Reducer->getName() + ".host_view");
    Value *StackSave = Builder.CreateStackSave();
    AllocaInst *HostViewBasePtr = Builder.CreateAlloca(
        Type::getInt8Ty(Ctx), AllocSize, Reducer->getName() + ".host_view");
    // HostViewBasePtr->setAlignment(
    //     Align{std::min(bit_ceil(ViewSize), size_t{8})});
    HostViewBasePtr->setAlignment(Align{8});

    // Allocate storage for the pointer to the device view.
    Value *DeviceViewPtrPtr = EntryBuilder.CreateAlloca(
        VoidPtrTy, nullptr, Reducer->getName() + ".device_view");

    // Allocate the device view.
    // CallInst *NewSPtr =
    //     Builder.CreateCall(KitCudaMemAllocAndCopyToDevFn,
    //                        {HostViewBasePtr, ConstantInt::get(I64Ty, AllocSize),
    //                         DeviceViewPtrPtr, Stream});
    CallInst *NewSPtr = Builder.CreateCall(
        KitCudaMemAllocAndCopyToDevFn,
        {HostViewBasePtr, AllocSize, DeviceViewPtrPtr, Stream});

    // Insert initialization of host temporary before the device-view allocation.
    IRBuilder<> PreCopyBuilder(NewSPtr);
    // Value *HostViewPtr = PreCopyBuilder.CreateConstInBoundsGEP2_32(
    //     ViewTy, HostViewBasePtr, 0, 0);
    Value *HostViewPtr = HostViewBasePtr;
    if (IdFn->getFunctionType()->getFunctionNumParams() > 1)
      PreCopyBuilder.CreateCall(IdFn, {Reducer, HostViewPtr});
    else
      PreCopyBuilder.CreateCall(IdFn, {HostViewPtr});
    if (AllocateMutex) {
      // Initialize extra 4 bytes for the mutex.
      // PreCopyBuilder.CreateStore(ConstantInt::get(Type::getInt32Ty(Ctx), 0),
      //                            PreCopyBuilder.CreateConstInBoundsGEP2_32(
      //                                ViewTy, HostViewBasePtr, 0, ViewSize));
      PreCopyBuilder.CreateStore(
          ConstantInt::get(Type::getInt32Ty(Ctx), 0),
          PreCopyBuilder.CreateInBoundsGEP(
              ViewTy, HostViewBasePtr, {SizeArg}));
    }

    // Get the pointer to the device view.
    Value *DeviceViewBasePtr = Builder.CreateLoad(VoidPtrTy, DeviceViewPtrPtr);
    Value *DeviceViewPtr =
        Builder.CreateConstInBoundsGEP2_32(ViewTy, DeviceViewBasePtr, 0, 0);

    // Replace all uses of the reducer in launch.kernel calls with the device view.
    Call.replaceUsesWithIf(DeviceViewPtr, [](Use &U) -> bool {
      if (CallBase *CB = dyn_cast<CallBase>(U.getUser()))
        return CB->getIntrinsicID() == Intrinsic::kit_async_launch_kernel;
      return false;
    });

    // Record the mapping to host temporary and device views for this reducer.
    HDViews[&Call] = {HostViewBasePtr, DeviceViewPtrPtr, StackSave};

    // Delay erasing these intrinsics until all other hyperobject intrinsic
    // calls have been lowered.
    ToErase.push_back(&Call);

    return true;
  }

  bool lowerReducerSync(CallInst &Call, bool IsHolder) {
    LLVMContext &Ctx = Call.getContext();
    Module &M = *Call.getModule();
    Type *I64Ty = Type::getInt64Ty(Ctx);
    PointerType *VoidPtrTy = PointerType::getUnqual(Ctx);

    // Extract information about this reducer.
    Value *View = Call.getArgOperand(0);
    Value *Reducer = Call.getArgOperand(1);
    Value *SizeArg = Call.getArgOperand(2);
    // size_t ViewSize = 0;
    // if (auto *ConstSize = dyn_cast<ConstantInt>(SizeArg))
    //   ViewSize = ConstSize->getZExtValue();
    bool AllocateMutex =
        !IsHolder && (!isa<ConstantInt>(SizeArg) ||
                      (dyn_cast<ConstantInt>(SizeArg)->getZExtValue() > 8));
    // For larger views, need to allocate an additional 4 bytes for a mutex.
    // size_t AllocSize = AllocateMutex ? ViewSize + 4 : ViewSize;
    Function *SizeFn = nullptr;
    if (!isa<ConstantPointerNull>(Call.getArgOperand(3)))
      SizeFn = dyn_cast<Function>(Call.getArgOperand(3));
    Function *IdentityFn = cast<Function>(Call.getArgOperand(4));
    Function *MergeFn = cast<Function>(Call.getArgOperand(5));
    Value *Stream = Call.getArgOperand(6);

    // Get the host and device views for this reducer.
    auto &[HostViewBasePtr, DeviceViewBasePtrPtr, StackSave] = HDViews[View];

    IRBuilder<> Builder(&Call);
    FunctionCallee KitCudaMemCopyAndFreeFromDevFn =
        M.getOrInsertFunction("__kitcuda_mem_copy_and_free_from_device",
                              VoidPtrTy,  // return the stream
                              VoidPtrTy,  // host-side view pointer
                              I64Ty,      // size of view
                              VoidPtrTy,  // device-side view pointer
                              VoidPtrTy); // stream
    Value *AllocSize =
        !AllocateMutex ? SizeArg
                       : Builder.CreateAdd(SizeArg, ConstantInt::get(I64Ty, 4));

    // Copy the device-view back to host storage.
    Value *DeviceViewBasePtr = Builder.CreateLoad(
        VoidPtrTy, DeviceViewBasePtrPtr, View->getName() + ".device_view_base");
    Builder.CreateCall(
                      KitCudaMemCopyAndFreeFromDevFn,
                      {HostViewBasePtr, AllocSize,
                      // SizeArg,
                      //  ConstantInt::get(I64Ty, ViewSize),
                       DeviceViewBasePtr, Stream});

    // Merge the value of the device view (in host storage) with the current
    // host view.
    if (MergeFn->getFunctionType()->getNumParams() > 2) {
      Value *HostView =
          Builder.CreateIntrinsic(VoidPtrTy, Intrinsic::hyper_lookup_0s,
                                  {
                                      Reducer,
                                      Builder.CreatePtrToInt(SizeFn, I64Ty),
                                      ConstantInt::get(I64Ty, 0),
                                      Builder.CreatePtrToInt(IdentityFn, I64Ty),
                                      ConstantInt::get(I64Ty, 0),
                                      Builder.CreatePtrToInt(MergeFn, I64Ty),
                                      ConstantInt::get(I64Ty, 0),
                                  });
      Builder.CreateCall(MergeFn, {Reducer, HostView, HostViewBasePtr});
    } else {
      Value *HostView =
          Builder.CreateIntrinsic(VoidPtrTy, Intrinsic::hyper_lookup_2,
                                  {Reducer, SizeArg, IdentityFn, MergeFn});
      Builder.CreateCall(MergeFn, {HostView, HostViewBasePtr});
    }
    Builder.CreateStackRestore(StackSave);

    // It's safe to delete this intrinsic right away.
    Call.eraseFromParent();

    return true;
  }

  bool lowerScannerSetup(CallInst &Call) {
    LLVMContext &Ctx = Call.getContext();
    Module &M = *Call.getModule();
    Type *VoidTy = Type::getVoidTy(Ctx);
    Type *I64Ty = Type::getInt64Ty(Ctx);
    PointerType *VoidPtrTy = PointerType::getUnqual(Ctx);

    BasicBlock &EntryBB = Call.getParent()->getParent()->getEntryBlock();
    IRBuilder<> EntryBuilder(EntryBB.getTerminator());
    IRBuilder<> Builder(&Call);

    FunctionCallee KitCudaAllocScanner = M.getOrInsertFunction(
        "__kitcuda_alloc_scanner", VoidTy, VoidPtrTy, VoidPtrTy, I64Ty,
        VoidPtrTy, I64Ty, VoidPtrTy, VoidPtrTy, VoidPtrTy, VoidPtrTy);

    // Extract information about this scanner.
    Value *FatBin = Call.getArgOperand(0);
    Value *KName = Call.getArgOperand(1);
    Value *TripCount = Call.getArgOperand(2);
    Value *InstMix = Call.getArgOperand(3);
    Value *Size = Call.getArgOperand(4);
    Value *Aggregate = Call.getArgOperand(5);
    Value *InclusivePrefix = Call.getArgOperand(6);
    Value *ScanState = Call.getArgOperand(7);
    Value *Stream = Call.getArgOperand(8);

    // Allocate storage for pointers to scanner objects.
    Value *AggregatePtr = EntryBuilder.CreateAlloca(VoidPtrTy);
    Value *InclusivePrefixPtr = EntryBuilder.CreateAlloca(VoidPtrTy);
    Value *ScanStatePtr = EntryBuilder.CreateAlloca(VoidPtrTy);

    // Allocate scanner objects.
    Builder.CreateCall(KitCudaAllocScanner,
                       {FatBin, KName, TripCount, InstMix, Size, AggregatePtr,
                        InclusivePrefixPtr, ScanStatePtr, Stream});

    // Replace all uses of the scanner objects with new pointers.
    Aggregate->replaceAllUsesWith(Builder.CreateLoad(VoidPtrTy, AggregatePtr));
    InclusivePrefix->replaceAllUsesWith(
        Builder.CreateLoad(VoidPtrTy, InclusivePrefixPtr));
    ScanState->replaceAllUsesWith(Builder.CreateLoad(VoidPtrTy, ScanStatePtr));

    // Erase this call.
    Call.eraseFromParent();

    return true;
  }

  bool lowerScannerSync(CallInst &Call) {
    LLVMContext &Ctx = Call.getContext();
    Module &M = *Call.getModule();
    Type *VoidTy = Type::getVoidTy(Ctx);
    PointerType *VoidPtrTy = PointerType::getUnqual(Ctx);

    // Extract information about this reducer.
    Value *Aggregate = Call.getArgOperand(0);
    Value *InclusivePrefix = Call.getArgOperand(1);
    Value *ScanState = Call.getArgOperand(2);
    Value *Stream = Call.getArgOperand(3);

    IRBuilder<> Builder(&Call);

    FunctionCallee KitCudaFreeScannerFn =
        M.getOrInsertFunction("__kitcuda_free_scanner", VoidTy, VoidPtrTy,
                              VoidPtrTy, VoidPtrTy, VoidPtrTy);

    Builder.CreateCall(KitCudaFreeScannerFn,
                       {Aggregate, InclusivePrefix, ScanState, Stream});

    // Erase this call.
    Call.eraseFromParent();

    return true;
  }

  /// The given call instruction is a call to a kitsune intrinsic. This may
  /// lower it (in some cases, the instruction will not be lowered - for
  /// instance if the primary tapir target is one that does not permit
  /// lowering). Returns true if the call to the instrinsic was replaced, false
  /// otherwise.
  bool lowerIntrinsic(CallInst &Call) {
    bool Changed = false;

    switch (Call.getIntrinsicID()) {
    case Intrinsic::kit_holder_setup:
    case Intrinsic::kit_reducer_setup:
      Changed |= lowerReducerSetup(Call, Call.getIntrinsicID() ==
                                             Intrinsic::kit_holder_setup);
      break;
    case Intrinsic::kit_holder_sync:
    case Intrinsic::kit_reducer_sync:
      Changed |= lowerReducerSync(Call, Call.getIntrinsicID() ==
                                            Intrinsic::kit_holder_sync);
      break;
    case Intrinsic::kit_scanner_setup:
      Changed |= lowerScannerSetup(Call);
      break;
    case Intrinsic::kit_scanner_sync:
      Changed |= lowerScannerSync(Call);
      break;
    }

    return Changed;
  }

public:
  LowerHyperIntrinsics(const TapirTargetInfo &TGI, TargetLibraryInfo &TLI)
      : TGI(TGI) {}

  bool run(Function &F) {
    bool Changed = false;
    std::optional<TTID> TT = TGI.getTTIDOrNull();
    if (not TT or *TT == TTID::Nolo)
      return Changed;

    // Replace all setup intrinsics before replacing all sync intrinsics.
    for (Intrinsic::ID ID :
         {Intrinsic::kit_reducer_setup,  Intrinsic::kit_reducer_sync,
          Intrinsic::kit_holder_setup,  Intrinsic::kit_holder_sync,
          Intrinsic::kit_scanner_setup, Intrinsic::kit_scanner_sync}) {
      // Find all intrinsics to lower.
      std::vector<CallInst *> Calls;
      for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I)
        if (auto *Call = dyn_cast<CallInst>(&*I))
          if (ID == Call->getIntrinsicID())
            Calls.push_back(Call);

      // Lower intrinsics.
      for (CallInst *Call : Calls)
        Changed |= lowerIntrinsic(*Call);
    }

    // Erase processed intrinsics whose erasure needed to be delayed.
    for (CallInst *Call : ToErase)
      Call->eraseFromParent();

    return Changed;
  }
};

// /// Legacy pass to compile the embedded bitcode to fat binaries.
// class LowerHyperIntrinsicsLegacyPass : public ModulePass {
// public:
//   LowerHyperIntrinsicsLegacyPass() : ModulePass(ID) {
//     initializeLowerHyperIntrinsicsLegacyPassPass(
//         *PassRegistry::getPassRegistry());
//   }

//   StringRef getPassName() const override {
//     return "Lower Kitsune hyperobject intrinsics";
//   }

//   void getAnalysisUsage(AnalysisUsage &AU) const override {
//     AU.addRequired<TapirTargetAnalysisWrapperPass>();
//     AU.addRequired<TargetLibraryInfoWrapperPass>();
//   }

//   bool runOnModule(Module &M) override {
//     const TapirTargetInfo &TGI =
//         getAnalysis<TapirTargetAnalysisWrapperPass>().getResult();

//     bool Changed = false;
//     for (Function &F : M) {
//       TargetLibraryInfo &TLI =
//           getAnalysis<TargetLibraryInfoWrapperPass>().getTLI(F);

//       Changed |= LowerHyperIntrinsics(TGI, TLI).run(F);
//     }

//     return Changed;
//   }

// public:
//   static char ID;
// };

} // namespace

// char LowerHyperIntrinsicsLegacyPass::ID = 0;

// INITIALIZE_PASS_BEGIN(LowerHyperIntrinsicsLegacyPass, DEBUG_TYPE,
//                       "Lower Hyperobject intrinsics", false, false)
// INITIALIZE_PASS_DEPENDENCY(TapirTargetAnalysisWrapperPass)
// INITIALIZE_PASS_END(LowerHyperIntrinsicsLegacyPass, DEBUG_TYPE,
//                     "Lower Hyperobject intrinsics", false, false)

// ModulePass *llvm::createLowerHyperIntrinsicsLegacyPass() {
//   return new LowerHyperIntrinsicsLegacyPass();
// }

PreservedAnalyses LowerHyperIntrinsicsPass::run(Module &M,
                                                ModuleAnalysisManager &MAM) {
  auto &FAM = MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
  const TapirTargetInfo &TGI = MAM.getResult<TapirTargetAnalysis>(M);

  bool Changed = false;
  for (Function &F : M) {
    TargetLibraryInfo &TLI = FAM.getResult<TargetLibraryAnalysis>(F);

    Changed |= LowerHyperIntrinsics(TGI, TLI).run(F);
  }

  // If any kitsune intrinsics were replaced, the call graph will have changed,
  // but other analyses will not have been invalidated.
  if (Changed) {
    PreservedAnalyses PA;
    PA.preserve<FunctionAnalysisManagerCGSCCProxy>();
    PA.preserveSet<AllAnalysesOn<Function>>();
    return PA;
  }
  return PreservedAnalyses::all();
}
