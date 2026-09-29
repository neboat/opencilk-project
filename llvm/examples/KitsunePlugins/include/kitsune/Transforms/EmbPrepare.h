//===- EmbPrepare.h - Prepare embedded modules for codegen -----*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Prepare embedded modules for code generation.
//
//===----------------------------------------------------------------------===//

#ifndef KITSUNE_TRANSFORMS_EMB_PREPARE_H
#define KITSUNE_TRANSFORMS_EMB_PREPARE_H

#include "kitsune/Transforms/EmbModulePass.h"

namespace llvm {

/// Prepare the embedded bitcode for code generation. This should be run
/// relatively late in the pipeline. This will carry out any
/// architecture-specific transformations that are unrelated to optimizations
/// and that have not already been carried out by the tapir targets that created
/// this module. For instance, for AMDGPU kernels, the kernel function
/// arguments must be placed in a specific address space, as must any alloca's
/// in the kernel functions. In some cases, the calling conventions of such
/// functions must also be changed.
///
/// This is mainly intended to be carried out on functions that are *not* the
/// GPU entry points, i.e. the GPU "kernel" functions that are launched from the
/// host, although this is not strictly enforced. The idea here is that the
/// tapir target will have taken care of adding the correct attributes to the
/// kernel function, but may not have done anything for the callees.
class EmbPreparePass : public EmbModulePass<EmbPreparePass> {
public:
  bool run(TTID tt, Module &devM, Module &hostM, ModuleAnalysisManager &hostAM);

  using EmbModulePass<EmbPreparePass>::run;
};

} // namespace llvm

#endif // KITSUNE_TRANSFORMS_EMB_PREPARE_H
