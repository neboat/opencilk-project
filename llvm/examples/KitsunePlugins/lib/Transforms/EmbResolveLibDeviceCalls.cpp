//==- EmbResolveLibDeviceCalls.cpp - Resolve calls to libdevice functions --==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Embedded modules may contain calls to library functions for which
// device-specific implementations exist. This resolves calls to such functions
// in embedded modules to use the device-specific implementations.
//
//===----------------------------------------------------------------------===//

#include "kitsune/Transforms/EmbResolveLibDeviceCalls.h"
#include "EmbResolveCallsImpl.h"
#include "kitsune/Analysis/TapirTargetAnalysis.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"

#define DEBUG_TYPE "emb-resolve-libdevice-calls"

using namespace llvm;

namespace llvm::detail {

static Function *resolveCallee(Function *f, bool fast, Module &libDeviceM,
                               GetLibDeviceFunc getDeviceFunc) {
  std::string devFnName = getDeviceFunc(f->getName(), fast);
  if (not devFnName.empty()) {
    if (Function *devFn = libDeviceM.getFunction(devFnName)) {
      LLVM_DEBUG(dbgs() << "resolve-libdevice-calls: mapped function '"
                        << f->getName() << "' to '" << devFnName << "'\n");
      // If the device function has already been declared in the kernel module,
      // create a declaration for the device function with the correct
      // attributes. We may need to fix the linkage type here because some
      // linkages are not allowed on declarations. The
      // emb-link-libdevice-bitcode pass will provide definitions for these
      // functions.
      Module *m = f->getParent();
      Function *fdecl = m->getFunction(devFnName);
      if (not fdecl) {
        FunctionType *ftype = devFn->getFunctionType();
        GlobalValue::LinkageTypes linkage = devFn->getLinkage();
        if (not GlobalValue::isValidDeclarationLinkage(linkage))
          linkage = GlobalValue::ExternalLinkage;

        fdecl = Function::Create(ftype, linkage, devFnName, m);
        fdecl->setAttributes(devFn->getAttributes());
        fdecl->setCallingConv(devFn->getCallingConv());
      }
      return fdecl;
    }
    // If a device function was not found, we can't resolve anything. This is an
    // error, but we don't deal with it here.
    LLVM_DEBUG(dbgs() << "resolve-libdevice-calls: WARNING: Mapped function '"
                      << devFnName << "' not in libdevice module" << "\n");
  }

  // Returning null to the caller indicates that the callee was not found in the
  // libdevice module and does not need to be replaced. This is not necessarily
  // an error. For instance, if the function is a target intrinsic or is already
  // a function from the libdevice module, there is no need to replace it.
  // However, if it is a function that should have been handled, this will
  // result in an error later in the compilation process.
  LLVM_DEBUG(dbgs() << "resolve-libdevice-calls: Not resolving '"
                    << f->getName() << "'\n");
  return nullptr;
}

bool resolveCallees(Function &f, Module &libDeviceM,
                    GetLibDeviceFunc getDeviceFunc) {
  LLVM_DEBUG(dbgs() << "resolve-libdevice-calls: In '" << f.getName() << "'\n");

  bool changed = false;
  for (inst_iterator i = inst_begin(f), e = inst_end(f); i != e; ++i) {
    if (auto *call = dyn_cast<CallBase>(&*i)) {
      if (Function *cf = call->getCalledFunction()) {
        if (cf->isDeclaration()) {
          bool fast = false;
          if (auto *fpo = dyn_cast<FPMathOperator>(call))
            fast = fpo->isFast();

          if (Function *df =
                  resolveCallee(cf, fast, libDeviceM, getDeviceFunc)) {
            call->setCalledFunction(df);
            changed |= true;
          }
        }
      }
    }
  }
  return changed;
}

} // namespace llvm::detail

bool EmbResolveLibDeviceCallsPass::run(TTID tt, Module &devM, Module &hostM,
                                       ModuleAnalysisManager &hostMAM) {
  const TapirTargetInfo &tgi = hostMAM.getResult<TapirTargetAnalysis>(hostM);
  const KitsuneTTOptions &tto = tgi.getOptions();

  switch (tt) {
  case TTID::Cuda:
    return detail::resolveLibDeviceCallsCuda(devM, tto);
  case TTID::Hip:
    return detail::resolveLibDeviceCallsHip(devM, tto);
  default:
    llvm_unreachable("ResolveLibDeviceCallsPass: TTID not handled");
  }
}
