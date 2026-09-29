#include "kitsune/Transforms/BuildKitsuneLibCalls.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Transforms/Utils/BuildLibCalls.h"

using namespace llvm;

#define DEBUG_TYPE "build-kit-libcalls"

StringLiteral const KitsuneLibraryInfo::StandardNames[KitFunc::NumKitFuncs] = {
#define TLI_DEFINE_STRING
#include "kitsune/Analysis/KitsuneLibraryInfo.def"
};

// Copied and adapted from TargetLibraryInfo.

// Recognized types of library function arguments and return types.
enum FuncArgTypeID : char {
  Void = 0, // Must be zero.
  Bool,     // 8 bits on all targets
  Int16,
  Int32,
  Int,
  IntPlus, // Int or bigger.
  Long,    // Either 32 or 64 bits.
  IntX,    // Any integer type.
  Int64,
  LLong,    // 64 bits on all targets.
  SizeT,    // size_t.
  SSizeT,   // POSIX ssize_t.
  Flt,      // IEEE float.
  Dbl,      // IEEE double.
  LDbl,     // Any floating type (TODO: tighten this up).
  Floating, // Any floating type.
  Ptr,      // Any pointer type.
  Struct,   // Any struct type.
  Ellip,    // The ellipsis (...).
  Same,     // Same argument type as the previous one.
};

typedef std::array<FuncArgTypeID, 10> FuncProtoTy;

static const FuncProtoTy Signatures[] = {
#define TLI_DEFINE_SIG
#include "kitsune/Analysis/KitsuneLibraryInfo.def"
};

static_assert(sizeof Signatures / sizeof *Signatures == KitFunc::NumKitFuncs,
              "Missing Kitsune library function signatures");

static Type *getType(FuncArgTypeID ArgTy, const Module &M,
                     const TargetLibraryInfo &TLI) {
  LLVMContext &Ctx = M.getContext();
  switch (ArgTy) {
  case Void:
    return Type::getVoidTy(Ctx);
  case Bool:
    return Type::getInt8Ty(Ctx);
  case Int16:
    return Type::getInt16Ty(Ctx);
  case Int32:
    return Type::getInt32Ty(Ctx);
  case Int:
  case IntX:
    return IntegerType::get(Ctx, TLI.getIntSize());
  case Int64:
    return Type::getInt64Ty(Ctx);
  case LLong:
    return Type::getInt64Ty(Ctx);
  case SizeT:
  case SSizeT:
    return IntegerType::get(Ctx, TLI.getSizeTSize(M));
  case Flt:
    return Type::getFloatTy(Ctx);
  case Dbl:
    return Type::getDoubleTy(Ctx);
  case Ptr:
    return PointerType::getUnqual(Ctx);
  default:
    break;
  }
  llvm_unreachable("BuildKitsuneLibCalls::getType: Type not yet supported");
}

FunctionType *
KitsuneLibraryInfo::getKitFuncType(KitFunc F, const Module &M,
                                   const TargetLibraryInfo &TLI) const {
  std::vector<Type *> Args;

  // The only way to detect the end of value types in Signatures[F] is to check
  // for FuncArgTyID == Void (which is defined to be 0).
  const FuncProtoTy &TyIDs = Signatures[F];
  for (size_t Idx = 1; Idx < TyIDs.size() && TyIDs[Idx]; ++Idx)
    Args.push_back(getType(TyIDs[Idx], M, TLI));

  Type *RetTy = getType(TyIDs[0], M, TLI);
  return FunctionType::get(RetTy, Args, false);
}

static StringRef sanitizeFunctionName(StringRef funcName) {
  // Filter out empty names and names containing null bytes, those can't be in
  // our table.
  if (funcName.empty() || funcName.contains('\0'))
    return StringRef();

  // Check for \01 prefix that is used to mangle __asm declarations and
  // strip it if present.
  return GlobalValue::dropLLVMManglingEscape(funcName);
}

static DenseMap<StringRef, KitFunc>
buildIndexMap(ArrayRef<StringLiteral> StandardNames) {
  DenseMap<StringRef, KitFunc> Indices;
  unsigned Idx = 0;
  Indices.reserve(KitFunc::NumKitFuncs);
  for (const auto &Func : StandardNames)
    Indices[Func] = static_cast<KitFunc>(Idx++);
  return Indices;
}

bool KitsuneLibraryInfo::getKitFunc(StringRef funcName, KitFunc &F) const {
  funcName = sanitizeFunctionName(funcName);
  if (funcName.empty())
    return false;

  static const DenseMap<StringRef, KitFunc> Indices =
      buildIndexMap(StandardNames);

  if (auto Loc = Indices.find(funcName); Loc != Indices.end()) {
    F = Loc->second;
    return true;
  }
  return false;
}

bool KitsuneLibraryInfo::getKitFunc(const Function &FDecl, KitFunc &F) const {
  // Intrinsics don't overlap w/libcalls; if our module has a large number of
  // intrinsics, this ends up being an interesting compile time win since we
  // avoid string normalization and comparison.
  if (FDecl.isIntrinsic())
    return false;

  const Module *M = FDecl.getParent();
  assert(M && "Expecting FDecl to be connected to a Module.");

  getKitFunc(FDecl.getName(), F);

  // return isValidProtoForKitFunc(*FDecl.getFunctionType(), F, *M);
  return true;
}

static bool setMemoryEffects(Function &F, MemoryEffects ME) {
  MemoryEffects OrigME = F.getMemoryEffects();
  MemoryEffects NewME = OrigME & ME;
  if (OrigME == NewME)
    return false;
  F.setMemoryEffects(NewME);
  return true;
}

static bool setDoesNotThrow(Function &F) {
  if (F.doesNotThrow())
    return false;
  F.setDoesNotThrow();
  return true;
}

static bool setNonLazyBind(Function &F) {
  if (F.hasFnAttribute(Attribute::NonLazyBind))
    return false;
  F.addFnAttr(Attribute::NonLazyBind);
  return true;
}

static bool setOnlyAccessesInaccessibleMemOrArgMem(Function &F) {
  if (!setMemoryEffects(F, MemoryEffects::inaccessibleOrArgMemOnly()))
    return false;
  return true;
}

static bool setWillReturn(Function &F) {
  if (F.hasFnAttribute(Attribute::WillReturn))
    return false;
  F.addFnAttr(Attribute::WillReturn);
  return true;
}

bool llvm::inferNonMandatoryKitFuncAttrs(Module *M, StringRef Name,
                                         const KitsuneLibraryInfo &KLI) {
  Function *F = M->getFunction(Name);
  if (!F)
    return false;
  return inferNonMandatoryKitFuncAttrs(*F, KLI);
}

bool llvm::inferNonMandatoryKitFuncAttrs(Function &F,
                                         const KitsuneLibraryInfo &KLI) {
  KitFunc TheKitFunc;
  if (!KLI.getKitFunc(F, TheKitFunc))
    return false;

  bool Changed = false;

  if (F.getParent() != nullptr && F.getParent()->getRtLibUseGOT())
    Changed |= setNonLazyBind(F);

  switch (TheKitFunc) {
  case KitFunc_kitpthr_launch:
  case KitFunc_kitpthr_sync:
  case KitFunc_kitcuda_enable_refine_launches:
  case KitFunc_kitcuda_finalize:
  case KitFunc_kitcuda_get_thread_stream:
  case KitFunc_kitcuda_initialize:
  case KitFunc_kitcuda_prefetch_dtoh:
  case KitFunc_kitcuda_prefetch_htod:
  case KitFunc_kitcuda_set_fixed_tpb:
  case KitFunc_kitcuda_set_max_tpb:
  case KitFunc_kitcuda_symbol_device_ptr:
  case KitFunc_kitcuda_symbol_memcpy_dtoh:
  case KitFunc_kitcuda_symbol_memcpy_htod:
  case KitFunc_kitcuda_sync_stream:
  case KitFunc_kithip_enable_xnack:
  case KitFunc_kithip_enable_y_axis_launches:
  case KitFunc_kithip_finalize:
  case KitFunc_kithip_get_thread_stream:
  case KitFunc_kithip_initialize:
  case KitFunc_kithip_prefetch_dtoh:
  case KitFunc_kithip_prefetch_htod:
  case KitFunc_kithip_set_fixed_tpb:
  case KitFunc_kithip_set_max_tpb:
  case KitFunc_kithip_symbol_device_ptr:
  case KitFunc_kithip_symbol_memcpy_dtoh:
  case KitFunc_kithip_symbol_memcpy_htod:
  case KitFunc_kithip_sync_stream:
  case KitFunc_kitrt_enable_verbose:
    // Although most Kitsune library functions can be marked as accessing only
    // inaccessible or argument memory, that's not true for launching kernels.
    Changed |= setOnlyAccessesInaccessibleMemOrArgMem(F);
    [[fallthrough]];
  case KitFunc_kitcuda_launch_kernel:
  case KitFunc_kithip_launch_kernel:
    Changed |= setDoesNotThrow(F);
    Changed |= setWillReturn(F);
    break;
  default:
    // FIXME: It'd be really nice to cover all the library functions we're
    // aware of here.
    break;
  }

  return Changed;
}

static void setArgExtAttr(Function &F, unsigned ArgNo,
                          const TargetLibraryInfo &TLI, bool Signed = true) {
  Attribute::AttrKind ExtAttr = TLI.getExtAttrForI32Param(Signed);
  if (ExtAttr != Attribute::None && !F.hasParamAttribute(ArgNo, ExtAttr))
    F.addParamAttr(ArgNo, ExtAttr);
}

FunctionCallee llvm::getOrInsertKitFunc(Module *M,
                                        const KitsuneLibraryInfo &KLI,
                                        KitFunc TheKitFunc, FunctionType *T,
                                        AttributeList AttributeList) {
  StringRef Name = KLI.getName(TheKitFunc);
  FunctionCallee C = M->getOrInsertFunction(Name, T, AttributeList);

  auto TLI = KLI.getTLI();
  // Make sure any mandatory argument attributes are added.

  // Any outgoing i32 argument should be handled with setArgExtAttr() which
  // will add an extension attribute if the target ABI requires it. Adding
  // argument extensions is typically done by the front end but when an
  // optimizer is building a library call on its own it has to take care of
  // this. Each such generated function must be handled here with sign or
  // zero extensions as needed.  F is retreived with cast<> because we demand
  // of the caller to have called isLibFuncEmittable() first.
  Function *F = cast<Function>(C.getCallee());

  switch (TheKitFunc) {
  case KitFunc_kitpthr_launch:
  case KitFunc_kitpthr_sync:
    setArgExtAttr(*F, 0, TLI);
    setArgExtAttr(*F, 1, TLI);
    break;

  case KitFunc_kitcuda_enable_refine_launches:
  case KitFunc_kitcuda_managed_malloc:
  case KitFunc_kithip_managed_malloc:
  case KitFunc_kitcuda_set_fixed_tpb:
  case KitFunc_kitcuda_set_max_tpb:
  case KitFunc_kithip_set_fixed_tpb:
  case KitFunc_kithip_set_max_tpb:
    setArgExtAttr(*F, 0, TLI);
    break;

  case KitFunc_kitcuda_managed_realloc:
  case KitFunc_kithip_managed_realloc:
    setArgExtAttr(*F, 1, TLI);
    break;

  case KitFunc_kitcuda_managed_memcpy:
  case KitFunc_kithip_managed_memcpy:
    setArgExtAttr(*F, 2, TLI);
    break;

  case KitFunc_kitcuda_symbol_memcpy_dtoh:
  case KitFunc_kitcuda_symbol_memcpy_htod:
  case KitFunc_kithip_symbol_memcpy_dtoh:
  case KitFunc_kithip_symbol_memcpy_htod:
    setArgExtAttr(*F, 2, TLI);
    break;

  case KitFunc_kitcuda_launch_kernel:
  case KitFunc_kithip_launch_kernel:
    setArgExtAttr(*F, 4, TLI);
    break;

  case KitFunc_cuda_register_managed_var:
  case KitFunc_cuda_register_var:
  case KitFunc_hip_register_var:
    setArgExtAttr(*F, 4, TLI);
    setArgExtAttr(*F, 6, TLI);
    setArgExtAttr(*F, 7, TLI);
    break;

  case KitFunc_hip_register_managed_var:
    setArgExtAttr(*F, 5, TLI);
    break;

  default:
#ifndef NDEBUG
    for (unsigned i = 0; i < T->getNumParams(); i++)
      assert(!isa<IntegerType>(T->getParamType(i)) &&
             "Unhandled integer argument.");
#endif
    break;
  }

  markRegisterParameterAttributes(F);

  return C;
}

FunctionCallee llvm::getOrInsertKitFunc(Module *M,
                                        const KitsuneLibraryInfo &KLI,
                                        KitFunc TheKitFunc, FunctionType *T) {
  return getOrInsertKitFunc(M, KLI, TheKitFunc, T, AttributeList());
}

FunctionCallee llvm::getOrInsertKitFunc(Module *M,
                                        const KitsuneLibraryInfo &KLI,
                                        KitFunc KitFunc) {
  return getOrInsertKitFunc(M, KLI, KitFunc,
                            KLI.getKitFuncType(KitFunc, *M, KLI.getTLI()));
}