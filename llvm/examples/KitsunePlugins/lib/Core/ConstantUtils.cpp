//===- ConstantUtils.cpp - Helper functions for constants -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Helper functions for constants.
//
//===----------------------------------------------------------------------===//

#include "kitsune/Core/ConstantUtils.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Module.h"

using namespace llvm;

std::optional<TTID> llvm::createTTIDFrom(const ConstantInt &cint) {
  return createTTIDFrom(cint.getZExtValue());
}

ConstantInt *llvm::createConstInt(TTID tt, LLVMContext &ctx) {
  return ConstantInt::get(Type::getInt32Ty(ctx), int(tt), false);
}

GlobalVariable *llvm::createConstString(StringRef s, Module &m,
                                        StringRef name) {
  for (GlobalVariable &g : m.globals())
    if (g.isConstant() and g.hasInitializer())
      if (auto *cda = dyn_cast<ConstantDataArray>(g.getInitializer()))
        if (cda->isCString() and cda->getAsCString() == s)
          return &g;

  LLVMContext &ctx = m.getContext();
  Constant *init = ConstantDataArray::getString(ctx, s, true);
  Type *type = init->getType();
  auto *g = new GlobalVariable(m, type, /*IsConstant=*/true,
                               GlobalValue::PrivateLinkage, init, name);
  g->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
  g->setAlignment(Align(1));

  return g;
}
