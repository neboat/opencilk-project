//=- OptznLevelUtils.h - Utilities for Kitsune's OptznLevel ========---*- C++
//-*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Utilities for Kitsune's OptznLevel.
//
//===----------------------------------------------------------------------===//

#ifndef KITSUNE_SUPPORT_OPTZN_LEVEL_UTILS_H
#define KITSUNE_SUPPORT_OPTZN_LEVEL_UTILS_H

#include "kitsune/Core/OptznLevel.h"
#include "llvm/Support/CodeGen.h"

namespace llvm {

/// Get the speedup level that the given \ref OptznLevel represents. This will
/// be an integer in [0,3]. This indicates the level of optimizations that are
/// being performed.
unsigned getSpeedupLevel(OptznLevel optLevel);

/// Get the size level that the given \ref OptznLevel represents. This will be
/// an integer in [0,2]. A non-zero value indicates that the code is being
/// optimized for size.
unsigned getSizeLevel(OptznLevel optLevel);

/// Create an OptznLevel from the given speedup and size levels. \ref
/// speedupLevel must be in [0,3]. \ref sizeLevel must be in [0,2]. It is an
/// error if either of these values is outside the allowed range.
OptznLevel createOptznLevelFrom(unsigned speedupLevel, unsigned sizeLevel = 0);

/// Map a character to an \ref OptznLevel. The character must be in { '0', '1',
/// '2', '3', 's', 'z' }.
OptznLevel createOptznLevelFrom(char level);

/// Map an optimization level to a CodeGenOptznLevel.
CodeGenOptLevel createCodeGenOptLevelFrom(OptznLevel optLevel);

} // namespace llvm

#endif // KITSUNE_SUPPORT_OPTZN_LEVEL_UTILS_H
