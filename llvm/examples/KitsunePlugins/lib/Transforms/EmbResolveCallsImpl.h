//=- EmbResolveCallsImpl.h - Private header for libdevice calls -*- C++ -*---=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implementation shared between resolvers for libdevice functions for different
// targets.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EMB_RESOLVE_CALLS_IMPL_H
#define LLVM_EMB_RESOLVE_CALLS_IMPL_H

#include "llvm/ADT/StringRef.h"

#include <functional>

namespace llvm {

class Function;
class Module;
class KitsuneTTOptions;

namespace detail {

using GetLibDeviceFunc = std::function<std::string(StringRef f, bool fast)>;

/// Resolve calls to library functions for which an implementation exists in
/// cuda's libdevice module.
bool resolveLibDeviceCallsCuda(Module &devM, const KitsuneTTOptions &tto);

/// Resolve calls to library functions for which an implementation exists in
/// hip's libdevice modules.
bool resolveLibDeviceCallsHip(Module &devM, const KitsuneTTOptions &tto);

/// Resolve all calls in the given function where the callee is a library
/// function for which an implementation exists in a libdevice module for the
/// appropriate target.
bool resolveCallees(Function &f, Module &libDeviceM,
                    GetLibDeviceFunc getDeviceFunc);

} // namespace detail

} // namespace llvm

#endif // LLVM_EMB_RESOLVE_CALLS_IMPL_H
