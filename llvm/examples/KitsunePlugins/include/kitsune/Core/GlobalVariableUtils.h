//===- GlobalVariableUtils.h - Utilities for global variables --*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Utilities to deal with global variables
//
//===----------------------------------------------------------------------===//

#ifndef KITSUNE_CORE_GLOBAL_VARIABLE_UTILS_H
#define KITSUNE_CORE_GLOBAL_VARIABLE_UTILS_H

#include "kitsune/Core/Tapir.h"
#include "llvm/IR/Attributes.h"

#include <optional>

namespace llvm {

class GlobalVariable;

/// Check if the global variable has an attribute of the given kind. If it
/// return the value of the attribute which should be a tapir target. Otherwise,
/// return std::nullopt.
std::optional<TTID> getAttrValueAsTTID(const GlobalVariable &g,
                                       Attribute::AttrKind attr);

} // namespace llvm

#endif // KITSUNE_CORE_GLOBAL_VARIABLE_UTILS_H
