//===- KernelProperties.h - Kernel function properties ---------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Kitsune computes the properties of kernel functions in order to determine
// better kernel launch parameters and to tweak GPU code generation. This
// contains helper functions and types for this purpose.
//
//===----------------------------------------------------------------------===//

#ifndef KITSUNE_CORE_KERNEL_PROPERTIES_H
#define KITSUNE_CORE_KERNEL_PROPERTIES_H

#include "kitsune/Core/Tapir.h"
#include "llvm/ADT/StringRef.h"

namespace llvm {

class ConstantStruct;
class Function;
class GlobalVariable;
class LLVMContext;
class Module;
class StructType;

/// Get the LLVM type for the instruction mix data.
StructType *getKernelPropertiesType(LLVMContext &ctx);

/// Calculate the kernel properties for the given kernel function and return a
/// Constant that can be used as the initializer of the kernel properties
/// global variable.
ConstantStruct *getKernelPropertiesConstant(const Function &f);

/// Create a global variable whose initializer will eventually contain the
/// properties of a kernel function. These properties currently captures counts
/// for various instruction kinds in the function, but could be expanded to
/// other data as well. This global is passed to the kernel launch functions.
/// \param kernelName The name of the kernel function
/// \param tt The tapir target that generated the kernel function
/// \param m The host module
GlobalVariable *createKernelPropertiesGlobal(StringRef kernelName, TTID tt,
                                             Module &m);

} // namespace llvm

#endif // KITSUNE_CORE_KERNEL_PROPERTIES_H
