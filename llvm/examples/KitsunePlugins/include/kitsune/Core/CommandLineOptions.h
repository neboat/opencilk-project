//=- CommandLineOptions.h - Command line options for Kitsune ------*- C++ -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Core command line options for Kitsune's tools.
//
//===----------------------------------------------------------------------===//

#ifndef KITSUNE_CORE_COMMAND_LINE_OPTIONS_H
#define KITSUNE_CORE_COMMAND_LINE_OPTIONS_H

#include "llvm/Support/CommandLine.h"

namespace llvm {

namespace cl {

/// Category for core Kitsune-specific command line options.
extern cl::OptionCategory catKitClOpts;

/// Category for Kitsune-specific command line options that are mainly intended
/// for Kitsune developers and power-users. These are generally options that
/// override the behavior of Kitsune's passes. Most of the options here are
/// hidden and are only visible when -help-hidden is used with opt and other
/// tools.
extern cl::OptionCategory catKitClDevOpts;

} // namespace cl

} // namespace llvm

#endif // KITSUNE_CORE_COMMAND_LINE_OPTIONS_H
