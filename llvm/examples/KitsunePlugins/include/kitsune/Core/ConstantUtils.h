//===- ConstantUtils.h - Helper functions for constants --------*- C++ -*--===//
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

#ifndef KITSUNE_CORE_CONSTANT_UTILS_H
#define KITSUNE_CORE_CONSTANT_UTILS_H

#include "kitsune/Core/Tapir.h"
#include "llvm/ADT/StringRef.h"

namespace llvm {

class ConstantInt;
class GlobalVariable;
class LLVMContext;
class Module;

/// Create a TTID from the given constant.
std::optional<TTID> createTTIDFrom(const ConstantInt &);

/// Generate a ConstantInt for use in Kitsune-specific intrinsics that take a
/// tapir target id as an argument.
ConstantInt *createConstInt(TTID tt, LLVMContext &ctxt);

/// Create a private string with the given initializer if one with this
/// initializer does not already exist in the module. If one does, return that.
/// This is linear in the number of global variables in the module.
///
/// @param s The string initializer
/// @param m The module in which to create the string
/// @param name If a global variable is to be created, the name to give it.
GlobalVariable *createConstString(StringRef s, Module &m, StringRef name = "");

} // namespace llvm

#endif // KITSUNE_CORE_CONSTANT_UTILS_H
