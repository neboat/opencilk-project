//==- EmbResolveCallsCuda.cpp - Resolve calls to cuda libdevice functions --==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Resolve calls to library functions for which implementations exist in cuda's
// libdevice module.
//
//===----------------------------------------------------------------------===//

#include "EmbResolveCallsImpl.h"
#include "kitsune/Core/ConstantUtils.h"
#include "kitsune/Core/TTUtils.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

#include "llvm/IR/Verifier.h"

using namespace llvm;

static const StringMap<StringRef> devFuncs = {
    {"acos", "acos"},
    {"acosf", "acosf"},
    {"acosh", "acosh"},
    {"acoshf", "acoshf"},
    {"asin", "asin"},
    {"asinf", "asinf"},
    {"asinh", "asinh"},
    {"asinhf", "asinhf"},
    {"atan2", "atan2"},
    {"atan2f", "atan2f"},
    {"atan", "atan"},
    {"atanf", "atahnf"},
    {"atanh", "atanh"},
    {"atanhf", "atanhf"},
    {"cbrt", "cbrt"},
    {"cbrtf", "cbrtf"},
    {"cos", "cos"},
    {"cosf", "cosf"},
    {"cosh", "cosh"},
    {"coshf", "coshf"},
    {"erfc", "erfc"},
    {"erfcf", "erfcf"},
    {"erf", "erf"},
    {"erff", "erff"},
    {"exp2", "exp2"},
    {"exp2f", "exp2f"},
    {"exp", "exp"},
    {"expf", "expf"},
    {"expm1", "expm1"},
    {"expm1f", "expm1f"},
    {"fmodf", "fmodf"},
    {"fmod", "fmod"},
    {"hypotf", "hypotf"},
    {"hypot", "hypot"},
    {"lgammaf", "lgammaf"},
    {"lgamma", "lgamma"},
    {"llvm.acos.f32", "acosf"},
    {"llvm.acos.f64", "acos"},
    {"llvm.asin.f32", "asinf"},
    {"llvm.asin.f64", "asin"},
    {"llvm.atan.f32", "atanf"},
    {"llvm.atan.f64", "atan"},
    {"llvm.cos.f32", "cosf"},
    {"llvm.cos.f64", "cos"},
    {"llvm.exp.f32", "expf"},
    {"llvm.exp.f64", "exp"},
    {"llvm.fabs.f32", "fabsf"},
    {"llvm.fabs.f64", "fabs"},
    {"llvm.fmod.f32", "fmodf"},
    {"llvm.fmod.f64", "fmod"},
    {"llvm.maxnum.f32", "fmaxf"}, // TODO: Check if this is correct?
    {"llvm.maxnum.f64", "fmax"},  // TODO: Check if this is correct?
    {"llvm.minnum.f32", "fminf"}, // TODO: Check if this is correct?
    {"llvm.minnum.f64", "fmin"},  // TODO: Check if this is correct?
    {"llvm.pow.f32", "powf"},
    {"llvm.pow.f64", "pow"},
    {"llvm.sincos.f32", "sincosf"},
    {"llvm.sincos.f64", "sincos"},
    {"llvm.sin.f32", "sinf"},
    {"llvm.sin.f64", "sin"},
    {"llvm.sqrt.f32", "sqrtf"},
    {"llvm.sqrt.f64", "sqrt"},
    {"llvm.tan.f32", "tanf"},
    {"llvm.tan.f64", "tan"},
    {"llvm.tanh.f32", "tanhf "},
    {"llvm.tanh.f64", "tanh"},
    {"log10f", "log10f"},
    {"log10", "log10"},
    {"log1pf", "log1pf"},
    {"log1p", "log1p"},
    {"log2f", "log2f"},
    {"log2", "log2"},
    {"logf", "logf"},
    {"log", "log"},
    {"powf", "powf"},
    {"pow", "pow"},
    {"sincosf", "sincosf"},
    {"sincos", "sincos"},
    {"sinf", "sinf"},
    {"sinhf", "sinhf"},
    {"sinh", "sinh"},
    {"sin", "sin"},
    {"sqrtf", "sqrtf"},
    {"sqrt", "sqrt"},
    {"tanf", "tanf"},
    {"tanhf", "tanhf"},
    {"tanh", "tanh"},
    {"tan", "tan"},
    {"tgammaf", "tgammaf"},
    {"tgamma", "tgamma"},
};

static std::string getDeviceFunc(StringRef f, bool fast) {
  if (devFuncs.find(f) == devFuncs.end())
    return "";

  StringRef pfx = fast ? "__nv_fast_" : "__nv_";
  return join_items("", pfx, devFuncs.at(f));
}

static Function *getVPrintf(Module &m) {
  LLVMContext &ctx = m.getContext();
  Type *i32 = Type::getInt32Ty(ctx);
  PointerType *ptr = PointerType::getUnqual(ctx);

  Type *params[] = {ptr, ptr};
  FunctionType *fty = FunctionType::get(i32, params, false);

  // If the function already exists, make sure that it has the correct
  // signature. It should not be present unless we have added it, and we are
  // guaranteed to have added it with the correct signature.
  if (Function *f = m.getFunction("vprintf")) {
    assert(f->getFunctionType() == fty);
    return f;
  }

  return Function::Create(fty, GlobalValue::ExternalLinkage, "vprintf", &m);
}

// Emit a call to vprintf. This function takes two arguments, a format string
// and a pointer to a buffer containing the (variable number of) arguments to
// printf. The buffer is allocated on the stack.
static Value *emitPrintfCall(Value *fmt, ArrayRef<Value *> args,
                             CallBase &callToReplace) {
  LLVMContext &ctx = callToReplace.getContext();
  Type *i32 = Type::getInt32Ty(ctx);
  Type *i64 = Type::getInt64Ty(ctx);

  Constant *c0 = ConstantInt::get(i64, 0);

  std::vector<Type *> argTypes;
  for (Value *arg : args)
    argTypes.push_back(arg->getType());
  StructType *packType = StructType::create(ctx, argTypes, "vprintf_pack");

  Function &f = *callToReplace.getParent()->getParent();
  Module &m = *f.getParent();
  BasicBlock &bbEntry = f.getEntryBlock();
  AllocaInst *pack = new AllocaInst(packType, 0, "", bbEntry.begin());

  // Construct and fill the args buffer that we'll pass to vprintf.
  for (unsigned i = 0; i < args.size(); ++i) {
    Constant *cidx = ConstantInt::get(i32, i);
    Value *idxs[] = {c0, cidx};
    GetElementPtrInst *packOff = GetElementPtrInst::Create(
        packType, pack, idxs, "", callToReplace.getIterator());
    (void)new StoreInst(args[i], packOff, callToReplace.getIterator());
  }

  std::vector<Value *> vprintfArgs = {fmt, pack};
  Function *vprintf = getVPrintf(m);

  return CallInst::Create(vprintf, vprintfArgs, "",
                          callToReplace.getIterator());
}

// Calls to puts have to be handled as a special case. The frontend will have
// stripped one trailing newline from any string literal passed to puts()
// since puts() will always append a newline after emitting the string. When
// resolving this to cuda's vprintf, we have to ensure that the newline is
// added back. To ensure this, we replace calls to puts as follows:
//
//     puts(s);
//
//  becomes
//
//     printf("%s\n", s);
//
// Returns true if at least one call to puts was replaced, false otherwise.
static bool resolvePutsCalls(Function &f) {
  std::vector<CallBase *> calls;
  for (inst_iterator i = inst_begin(f); i != inst_end(f); ++i)
    if (auto *call = dyn_cast<CallBase>(&*i))
      if (Function *callee = call->getCalledFunction())
        if (callee->getName() == "puts")
          calls.push_back(call);

  if (calls.size()) {
    Module &m = *f.getParent();
    GlobalVariable *fmt = createConstString("%s\n", m);
    for (CallBase *call : calls) {
      std::vector<Value *> args;
      for (Value *arg : call->args())
        args.push_back(arg);
      emitPrintfCall(fmt, args, *call);
    }

    for (CallBase *call : calls)
      call->eraseFromParent();
  }

  return calls.size();
}

static bool resolvePrintfCalls(Function &f) {
  std::vector<CallBase *> calls;
  for (inst_iterator i = inst_begin(f); i != inst_end(f); ++i)
    if (auto *call = dyn_cast<CallBase>(&*i))
      if (Function *callee = call->getCalledFunction())
        if (callee->getName() == "printf")
          calls.push_back(call);

  if (calls.size()) {
    for (CallBase *call : calls) {
      assert(call->arg_size() > 0 && "printf must have at least one argument");
      std::vector<Value *> args;
      for (size_t i = 1; i < call->arg_size(); ++i)
        args.push_back(call->getArgOperand(i));
      emitPrintfCall(call->getArgOperand(0), args, *call);
    }

    for (CallBase *call : calls)
      call->eraseFromParent();
  }

  return calls.size();
}

bool llvm::detail::resolveLibDeviceCallsCuda(Module &devM,
                                             const KitsuneTTOptions &tto) {
  LLVMContext &ctx = devM.getContext();
  Expected<OwnedModule> mOrErr = getLibDeviceModule(TTID::Cuda, tto, ctx);
  assert(mOrErr && "Expected libDevice module");

  bool changed = false;
  OwnedModule libDeviceM = std::move(*mOrErr);
  for (Function &f : devM.functions()) {
    if (f.size()) {
      changed |= resolvePrintfCalls(f);
      changed |= resolvePutsCalls(f);
      changed |= resolveCallees(f, *libDeviceM, getDeviceFunc);
    }
  }
  return changed;
}
