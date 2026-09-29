//=- GenerateKitsuneCtorsImpl.h - Private header for ctor gen -----*- C++ -*-=//
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

#ifndef LLVM_KITSUNE_GENERATE_CTORS_IMPL_H
#define LLVM_KITSUNE_GENERATE_CTORS_IMPL_H

#include <functional>

#define DEBUG_TYPE "kit-ctors"

namespace llvm {

class Function;
class Module;
class KitsuneTTOptions;
class TargetLibraryInfo;

namespace detail {

/// Options to generate global ctors for kitsune's runtime.
struct GenerateCtorOptions {
public:
  /// Enable refining the launch parameter in the runtime.
  unsigned refineLaunches : 1;

  /// Launch kernel using Y-axis threading.
  unsigned useYLaunch : 1;
};

/// Type of function that queries the TargetLibraryInfo analysis for an LLVM
/// Function.
using GetTLI = std::function<TargetLibraryInfo &(Function &)>;

/// Type of the function that will generate a global ctor for some tapir target.
using GenerateCtorImplFn = std::function<void(
    Module &, GetTLI, const KitsuneTTOptions &, GenerateCtorOptions &)>;

/// Generate a global constructor and destructor for kitsune's cuda runtime.
void genCtorCuda(Module &m, GetTLI getTLI, const KitsuneTTOptions &ttOpts,
                 const GenerateCtorOptions &ctorOpts);

/// Generate a global constructor and destructor for kitsune's hip runtime.
void genCtorHip(Module &m, GetTLI getTLI, const KitsuneTTOptions &ttOpts,
                const GenerateCtorOptions &ctorOpts);

/// Generate a global constructor and destructor for Kitsune's pthreads runtime.
void genCtorPthreads(Module &m, GetTLI getTLI, const KitsuneTTOptions &ttOpts,
                     const GenerateCtorOptions &ctorOpts);

} // namespace detail

} // namespace llvm

#endif // LLVM_KITSUNE_GENERATE CTORS_IMPL_H
