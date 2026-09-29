//=- CommandLineOptions.cpp - Command line options for Kitsune tools --------=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Definitions of core command line options for Kitsune's tools.
//
//===----------------------------------------------------------------------===//

#include "kitsune/Core/CommandLineOptions.h"

using namespace llvm;

cl::OptionCategory cl::catKitClOpts("Kitsune Options");
cl::OptionCategory cl::catKitClDevOpts("Kitsune Developer Options");
