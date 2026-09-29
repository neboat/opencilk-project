#ifndef KIT_TRANSFORMS_BUILDKITSUNELIBCALLS_H
#define KIT_TRANSFORMS_BUILDKITSUNELIBCALLS_H

#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Support/Compiler.h"

namespace llvm {

enum KitFunc : unsigned {
#define TLI_DEFINE_ENUM
#include "kitsune/Analysis/KitsuneLibraryInfo.def"

  NumKitFuncs,
  NotKitFunc
};

class KitsuneLibraryInfo {
  const TargetLibraryInfo &TLI;

  LLVM_ABI static StringLiteral const StandardNames[NumKitFuncs];

public:
  KitsuneLibraryInfo(const TargetLibraryInfo &TLI) : TLI(TLI) {}

  bool getKitFunc(StringRef funcName, KitFunc &F) const;
  bool getKitFunc(const Function &FDecl, KitFunc &F) const;

  FunctionType *getKitFuncType(KitFunc F, const Module &M,
                               const TargetLibraryInfo &TLI) const;

  StringRef getName(KitFunc F) const { return StandardNames[F]; }

  const TargetLibraryInfo &getTLI() const { return TLI; }
};

/// Analyze the name and prototype of the given function and set any
/// applicable attributes. Note that this merely helps optimizations on an
/// already existing function but does not consider mandatory attributes.
///
/// If the library function is unavailable, this doesn't modify it.
///
/// Returns true if any attributes were set and false otherwise.
LLVM_ABI bool inferNonMandatoryKitFuncAttrs(Module *M, StringRef Name,
                                            const KitsuneLibraryInfo &KLI);
LLVM_ABI bool inferNonMandatoryKitFuncAttrs(Function &F,
                                            const KitsuneLibraryInfo &KLI);

/// Calls getOrInsertFunction() and then makes sure to add mandatory
/// argument attributes.
LLVM_ABI FunctionCallee getOrInsertKitFunc(Module *M,
                                           const KitsuneLibraryInfo &KLI,
                                           KitFunc TheKitFunc, FunctionType *T,
                                           AttributeList AttributeList);
LLVM_ABI FunctionCallee getOrInsertKitFunc(Module *M,
                                           const KitsuneLibraryInfo &KLI,
                                           KitFunc TheKitFunc, FunctionType *T);
LLVM_ABI FunctionCallee getOrInsertKitFunc(Module *M,
                                           const KitsuneLibraryInfo &KLI,
                                           KitFunc TheKitFunc);
template <typename... ArgsTy>
FunctionCallee
getOrInsertKitFunc(Module *M, const KitsuneLibraryInfo &TLI, KitFunc TheKitFunc,
                   AttributeList AttributeList, Type *RetTy, ArgsTy... Args) {
  SmallVector<Type *, sizeof...(ArgsTy)> ArgTys{Args...};
  return getOrInsertKitFunc(M, TLI, TheKitFunc,
                            FunctionType::get(RetTy, ArgTys, false),
                            AttributeList);
}
/// Same as above, but without the attributes.
template <typename... ArgsTy>
FunctionCallee getOrInsertKitFunc(Module *M, const KitsuneLibraryInfo &TLI,
                                  KitFunc TheKitFunc, Type *RetTy,
                                  ArgsTy... Args) {
  return getOrInsertKitFunc(M, TLI, TheKitFunc, AttributeList{}, RetTy,
                            Args...);
}
// Avoid an incorrect ordering that'd otherwise compile incorrectly.
template <typename... ArgsTy>
FunctionCallee
getOrInsertKitFunc(Module *M, const KitsuneLibraryInfo &TLI, KitFunc TheKitFunc,
                   AttributeList AttributeList, FunctionType *Invalid,
                   ArgsTy... Args) = delete;
} // namespace llvm

#endif