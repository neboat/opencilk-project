//=- GenerateCtors.h - Generate global ctors for Kitsune ----------*- C++ -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Generate Kitsune global constructors.
//
//===----------------------------------------------------------------------===//

#ifndef KITSUNE_TRANSFORMS_GENERATE_CTORS_H
#define KITSUNE_TRANSFORMS_GENERATE_CTORS_H

#include "llvm/IR/PassManager.h"

namespace llvm {

/// Generates global constructors and destructors needed by Kitsune.
///
/// These will initialize and finalize kitsune's runtime(s). They will do the
/// same for any other runtimes (such as cuda and hip). These may involve
/// registering global variables and fat binaries with the underlying
/// GPU-specific runtime, setting environment variables etc. Not all tapir
/// targets require Kitsune's runtime, but this pass will always be run when
/// tapir is enabled.
///
/// In addition to creating the constructors and destructors, this pass will
/// also create any any global variables needed by the global ctor. In the case
/// of the GPU tapir targets and associated runtimes, these include globals for
/// the fat binary, the bundle that wraps the fat binary etc.
///
/// This pass should only be run once per module and should be run as late as
/// possible to ensure that all tapir targets have been run already.
class GenerateCtorsPass : public PassInfoMixin<GenerateCtorsPass> {
public:
  PreservedAnalyses run(Module &m, ModuleAnalysisManager &mam);

  static bool isRequired() { return true; }
};

} // namespace llvm

#endif // KITSUNE_TRANSFORMS_GENERATE_CTORS_H
