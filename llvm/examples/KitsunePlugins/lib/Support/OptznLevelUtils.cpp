//===- OptznLevelUtils.cpp - Utilities for LLVM's OptimizationLevel's
//-------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Utilities for LLVM's OptimizationLevel objects
//
//===----------------------------------------------------------------------===//

#include "kitsune/Support/OptznLevelUtils.h"
#include "llvm/Support/Error.h"

using namespace llvm;

unsigned llvm::getSpeedupLevel(OptznLevel optLevel) {
  switch (optLevel) {
  case OptznLevel::O0:
  case OptznLevel::Os:
  case OptznLevel::Oz:
    return 0;
  case OptznLevel::O1:
    return 1;
  case OptznLevel::O2:
    return 2;
  case OptznLevel::O3:
    return 3;
  }
  llvm_unreachable("getSpeedupLevel: OptznLevel not handled");
}

unsigned getSizeLevel(OptznLevel optLevel) {
  switch (optLevel) {
  case OptznLevel::O0:
  case OptznLevel::O1:
  case OptznLevel::O2:
  case OptznLevel::O3:
    return 0;
  case OptznLevel::Os:
    return 1;
  case OptznLevel::Oz:
    return 2;
  }
  llvm_unreachable("getSizeLevel: OptznLevel not handled");
}

OptznLevel llvm::createOptznLevelFrom(unsigned speedupLevel,
                                      unsigned sizeLevel) {
  switch (sizeLevel) {
  case 0:
    switch (speedupLevel) {
    case 0:
      return OptznLevel::O0;
    case 1:
      return OptznLevel::O1;
    case 2:
      return OptznLevel::O2;
    case 3:
      return OptznLevel::O3;
    default:
      llvm_unreachable("createOptznLevelFrom: invalid speedup level");
    }
  case 1:
    return OptznLevel::Os;
  case 2:
    return OptznLevel::Oz;
  default:
    llvm_unreachable("createOptznLevelFrom: invalid size level");
  }
}

OptznLevel llvm::createOptznLevelFrom(char level) {
  switch (level) {
  case '0':
    return OptznLevel::O0;
  case '1':
    return OptznLevel::O1;
  case '2':
    return OptznLevel::O2;
  case '3':
    return OptznLevel::O3;
  case 's':
    return OptznLevel::Os;
  case 'z':
    return OptznLevel::Oz;
  default:
    llvm_unreachable("createOptznLevelFrom: invalid optimization level");
  }
}

CodeGenOptLevel llvm::createCodeGenOptLevelFrom(OptznLevel optznLevel) {
  switch (optznLevel) {
  case OptznLevel::O0:
  case OptznLevel::Os:
  case OptznLevel::Oz:
    return CodeGenOptLevel::None;
  case OptznLevel::O1:
    return CodeGenOptLevel::Less;
  case OptznLevel::O2:
    return CodeGenOptLevel::Default;
  case OptznLevel::O3:
    return CodeGenOptLevel::Aggressive;
  }
  llvm_unreachable("createCodeGenOptznLevelFrom: OptznLevel not handled");
}
