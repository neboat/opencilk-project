//===- KernelProperties.cpp - Kernel function properties ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Definitions of functions to help with calculating and saving the properties
// of kernel functions that are used by Kitsune's runtime.
//
//===----------------------------------------------------------------------===//

#include "kitsune/Core/KernelProperties.h"
#include "kitsune/Common/Types.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

#include <map>
#include <memory>
#include <set>

using namespace llvm;

namespace {

/// Helper class to calculate the instruction kinds in a function. We need this
/// for two reasons:
///
///   1. To avoid redundant computations of the instruction mix of functions
///      that have already been encountered. These are functions that are
///      reachable from the kernel function.
///
///   2. To help ensure that we don't fall into an infinite recursion in case
///      the callgraph contains recursive functions.
///
class InstMixCalculator {
private:
  /// The instruction mixes for the functions seen so far. The value here is a
  /// unique pointer so we can return a reference to the object instead of
  /// returning by value. We need a unique_ptr in this case, otherwise, adding
  /// another pair to the map risks invalidating the reference.
  ///
  /// NOTE: The values here are for just the function alone, not including the
  /// the values of the instructions mixes for any functions reachable from it.
  /// This trades off a bit of efficiency but keeps the implementation simple.
  std::map<const Function *, std::unique_ptr<KernelInstMixData>> instMixes;

private:
  /// Find the functions with definitions that are reachable from the given
  /// function. A set of functions that have already been seen are provided to
  /// ensure that recursive functions (self, or mutual) are only processed once.
  void reachable(const Function &f, std::set<const Function *> &seen) {
    // insert returns a pair, the second element of which is true if insertion
    // took place, false otherwise. If f has already been seen, the second
    // element will be false indicating that the function has already been seen
    // and we should not process it.
    if (not seen.insert(&f).second)
      return;

    std::set<const Function *> calledFuncs;
    for (const_inst_iterator i = inst_begin(f), e = inst_end(f); i != e; ++i) {
      if (auto *call = dyn_cast<CallBase>(&*i))
        // In some cases, we see inline assembly being called here, especially
        // when code from device-specific bitcode modules is inlined, so don't
        // assume that we will always get a function.
        if (Function *cf = call->getCalledFunction())
          if (cf->size())
            calledFuncs.insert(cf);
    }

    for (const Function *cf : calledFuncs)
      reachable(*cf, seen);
  }

  std::set<const Function *> reachable(const Function &f) {
    std::set<const Function *> seen;
    reachable(f, seen);
    return seen;
  }

  const KernelInstMixData &calculate(const Function &f) {
    if (instMixes.find(&f) == instMixes.end()) {
      auto instMix = std::make_unique<KernelInstMixData>();
      for (const_inst_iterator i = inst_begin(f), e = inst_end(f); i != e;
           ++i) {
        // For now, we only treat certain instructions as memory ops. This may
        // need to be updated depending on how sophisticated we wish to make
        // this calculator.
        if (isa<LoadInst>(*i) or isa<StoreInst>(*i) or isa<AtomicRMWInst>(*i) or
            isa<AtomicCmpXchgInst>(*i)) {
          instMix->memOps += 1;
        } else if (i->isUnaryOp() or i->isBinaryOp()) {
          if (i->getType()->isFPOrFPVectorTy())
            instMix->fpOps += 1;
          else if (i->getType()->isIntegerTy())
            instMix->intOps += 1;
          else
            instMix->otherOps += 1;
        }
      }
      instMixes.emplace(&f, std::move(instMix));
    }
    return *instMixes.at(&f);
  }

public:
  /// Calculate the instruction mix for the given function. The result will be
  /// the sum of the instruction mixes of all functions reachable from it. The
  /// calculation is very naive since it does not take into account loops, so
  /// if a function is called within a loop several times (where the trip count
  /// is unknown), we assume that it will be called just the once. The same
  /// applies if any of the callees are recursive (the function itself is
  /// unlikely to be recursive since it will have been outlined from a tapir
  /// loop).
  KernelInstMixData run(const Function &f) {
    KernelInstMixData res;
    // The set of reachable functions will contain the given function.
    for (const Function *f : reachable(f))
      res += calculate(*f);
    return res;
  }
};

} // namespace

StructType *llvm::getKernelPropertiesType(LLVMContext &ctx) {
  Type *i64 = Type::getInt64Ty(ctx);
  return StructType::get(i64,  // number of memory ops
                         i64,  // number of floating point ops
                         i64,  // number of integer ops
                         i64); // number of other ops
}

GlobalVariable *llvm::createKernelPropertiesGlobal(StringRef kernelName,
                                                   TTID tt, Module &m) {
  LLVMContext &ctx = m.getContext();
  StructType *type = getKernelPropertiesType(ctx);
  Constant *init = Constant::getNullValue(type);
  auto *g = new GlobalVariable(m, type, /*IsConstant=*/true,
                               GlobalValue::PrivateLinkage, init);

  g->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
  // g->addAttribute(Attribute::getWithTTID(ctx, tt));
  // g->addAttribute(Attribute::getWithKernelProps(ctx, kernelName));
  NamedMDNode *Annotations = m.getOrInsertNamedMetadata("kit.annotations");
  Annotations->addOperand(MDNode::get(
      ctx, {
               ValueAsMetadata::get(g),
               MDString::get(ctx, "kprops"),
               ConstantAsMetadata::get(ConstantInt::get(
                   Type::getInt32Ty(ctx), static_cast<uint32_t>(tt))),
               MDString::get(ctx, kernelName),
           }));

  return g;
}

ConstantStruct *llvm::getKernelPropertiesConstant(const Function &f) {
  LLVMContext &ctx = f.getContext();
  Type *i64 = Type::getInt64Ty(ctx);
  StructType *propertiesType = getKernelPropertiesType(ctx);

  KernelInstMixData instMix = InstMixCalculator().run(f);

  return cast<ConstantStruct>(
      ConstantStruct::get(propertiesType, ConstantInt::get(i64, instMix.memOps),
                          ConstantInt::get(i64, instMix.fpOps),
                          ConstantInt::get(i64, instMix.intOps),
                          ConstantInt::get(i64, instMix.otherOps)));
}
