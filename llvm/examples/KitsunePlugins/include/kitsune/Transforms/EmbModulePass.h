//==- EmbModulePass.h - Base class for embedded module passes ----*- C++ -*-==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Base class for passes that operate on embedded modules. These typically
// perform transformations on the embedded modules and update the global
// variables in the parent module that contain them.
//
//===----------------------------------------------------------------------===//

#ifndef KITSUNE_TRANSFORMS_EMB_MODULE_PASS_H
#define KITSUNE_TRANSFORMS_EMB_MODULE_PASS_H

// #include "kitsune/Analysis/TapirTargetAnalysis.h"
#include "kitsune/Core/EmbUtils.h"
#include "kitsune/Core/Tapir.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"

namespace llvm {

class GlobalVariable;

namespace detail {

// SFINAE helper classes to determine if the embedded bitcode pass needs a
// module analysis manager to operate on the embedded bitcode module.
template <typename T, typename = void>
struct needs_analysis_manager : std::false_type {};

template <typename T>
struct needs_analysis_manager<
    T, std::void_t<decltype(std::declval<T>().*
                            std::declval<bool(
                                T::*(TTID, Module &, ModuleAnalysisManager &,
                                     Module &, ModuleAnalysisManager &))>())>>
    : std::true_type {};

template <typename T>
static constexpr bool needsAnalysisManager = needs_analysis_manager<T>::value;

} // namespace detail

/// CRTP base class for embedded bitcode passes. These are passes that may
/// transform the embedded bitcode in some way. The base class provides an
/// implementation for the run() method required by the PassInfoMixin.
///
/// The classes that inherit from this base class are the actual passes that
/// will operate on the embedded bitcode module. These must provide *exactly*
/// one function named run() with either of the following signatures:
///
///     bool run(TTID,
///              Module& m,
///              Module& hostModule,
///              ModuleAnalysisManager& hostMAM);
///
///   OR
///
///     bool run(TTID,
///              Module& m,
///              ModuleAnalysisManager& mam,
///              Module& hostModule,
///              ModuleAnalysisManager& hostMAM);
///
/// The former is used if the pass does not require any analyses on the
/// embedded bitcode module, while the latter is used if the pass may need
/// analyses. While it is safe to always use the latter, it may be inefficient
/// since it will involve the creation and initialization of several analysis
/// managers unnecessarily.
///
/// NOTE: Embedded bitcode passes *MUST NOT* modify the host module. They
/// may examine the host module and request analyses from it. The only
/// modification these passes may make to the host is to update the initializer
/// of the global variable containing the bitcode that was modified.
///
template <typename DerivedT>
class EmbModulePass : public PassInfoMixin<DerivedT> {
public:
  PreservedAnalyses run(Module &hostM, ModuleAnalysisManager &hostMAM) {
    // If no primary tapir target has been set, the tapir target options will
    // not have been set, so there is nothing that we can do.
    // const TapirTargetInfo &tgi = hostMAM.getResult<TapirTargetAnalysis>(hostM);
    // if (not tgi.hasTTID())
    //   return PreservedAnalyses::all();

    // Calling resetEmbeddedBC() will delete the global variable whose
    // initializer is being reset. Obviously, we can't iterate over the globals
    // while running passes on them, so collect the globals first, then run the
    // pass on each.
    std::vector<std::tuple<GlobalVariable *, TTID>> gs;
    // for (GlobalVariable &g : hostM.globals()) {
    //   if (g.hasAttribute(Attribute::KitBC)) {
    //     assert(g.hasAttribute(Attribute::KitTT) &&
    //            "Attribute 'kit_bc' requires 'kit_tt");
    //     gs.emplace_back(&g, g.getAttribute(Attribute::KitTT).getTTID());
    //   }
    // }
    {
      if (const NamedMDNode *Annotations =
              hostM.getNamedMetadata("kit.annotations"))
        for (auto *Entry : Annotations->operands()) {
          if (auto *Type = dyn_cast<MDString>(Entry->getOperand(1))) {
            if (Type->getString() == "bc") {
              if (ConstantInt *EntryTT =
                      mdconst::dyn_extract_or_null<ConstantInt>(
                          Entry->getOperand(2)))
                gs.emplace_back(mdconst::dyn_extract_or_null<GlobalVariable>(
                                    Entry->getOperand(0)),
                                static_cast<TTID>(EntryTT->getZExtValue()));
            }
          }
        }
    }

    auto *pass = static_cast<DerivedT *>(this);
    for (auto &tup : gs) {
      bool changed = false;
      GlobalVariable *g = std::get<0>(tup);
      TTID tt = std::get<1>(tup);
      std::unique_ptr<Module> km = parseEmbBCGlobal(*g);
      if constexpr (detail::needsAnalysisManager<DerivedT>) {
        LoopAnalysisManager lam;
        FunctionAnalysisManager fam;
        CGSCCAnalysisManager cgam;
        ModuleAnalysisManager mam;
        PassBuilder pb;

        pb.registerModuleAnalyses(mam);
        pb.registerCGSCCAnalyses(cgam);
        pb.registerFunctionAnalyses(fam);
        pb.registerLoopAnalyses(lam);
        pb.crossRegisterProxies(lam, fam, cgam, mam);

        changed = pass->run(tt, *km, mam, hostM, hostMAM);
      } else {
        changed = pass->run(tt, *km, hostM, hostMAM);
      }

      if (changed)
        resetEmbBCGlobal(*km, *g);
    }

    // This will preserve all analyses because the only thing that may have
    // changed are the types and initializers of one or more global variables.
    return PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};

} // namespace llvm

#endif // KITSUNE_TRANSFORMS_EMB_MODULE_PASS_H
