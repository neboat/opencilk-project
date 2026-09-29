//=- CGFB.h - Private header for codegen fat binaries pass --------*- C++ -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Private header shared by the fat binary code generators.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_KITSUNE_CGFB_IMPL_H
#define LLVM_KITSUNE_CGFB_IMPL_H

#include "kitsune/Core/OptznLevel.h"
#include "llvm/Support/CodeGen.h"

#define DEBUG_TYPE "kit-cgfb"

namespace llvm {

class GlobalVariable;
class KitsuneTTOptions;
class TargetMachine;
class ToolOutputFile;
class raw_ostream;

namespace detail {

/// Options to control code generation of the fat binaries. These are specific
/// to the way the kit-cgfb pass operates. Target-specific options will be set
/// in LLVM's \ref TargetOptions object from the \ref TTOptions.
struct CGFBOptions {
  /// The optimization level to use with the target machine. This will be set to
  /// the value in \ref TTOptions if the -cgfb-O<N> option is not
  /// provided. If it is, that values takes precedence.
  CodeGenOptLevel cgOptLevel;

  /// The optimization level to use for ptxas. This will be set to the value in
  /// TTOptions if the -cgfb-ptxas-O<N> option is not used. If it is,
  /// that value takes precedence.
  OptznLevel ptxasOptLevel;

  /// Don't delete any intermediate files that were generated in the course of
  /// fat binary generation.
  unsigned keepFiles : 1;

  /// Print the command lines for any external tools that are called during
  /// fat binary generation. This is only really useful for debugging. These are
  /// printed to stderr.
  unsigned debugCommandLines : 1;

  /// Print the \ref MCTargetOptions to stderr.
  unsigned debugMCTargetOptions : 1;

  /// Print some properties of the \ref TargetMachine to stderr.
  unsigned debugTargetMachine : 1;

  /// Print the \ref TargetOptions to stderr.
  unsigned debugTargetOptions : 1;
};

/// Print some properties of the target machine to the given output stream.
void debugTargetMachine(const TargetMachine &tm, raw_ostream &os);

/// Copy the bytes in the given file containing a fat binary containing into the
/// initializer of the given global variable.
void embedFatBinary(ToolOutputFile &fatbinFile, GlobalVariable &g);

/// Codegen a fat binary for NVIDIA GPU's. \ref gfb is the global variable into
/// which the fat binary will be saved. \ref gbc is the global variable
/// containing the bitcode from which the fat binary will be generated. If
/// \ref keepFiles is true, the intermediate files generated during the
/// compilation process will not be deleted after use.
bool cgfbCuda(GlobalVariable &gfb, const GlobalVariable &gbc,
              const KitsuneTTOptions &tto, const CGFBOptions &cgfbOpts);

/// Codegen a fat binary for AMD GPU's. \ref gfb is the global variable into
/// which the fat binary will be saved. \ref gbc is the global variable
/// containing the bitcode from which the fat binary will be generated. If
/// \ref keepFiles is true, the intermediate files generated during the
/// compilation process will not be deleted after use.
bool cgfbHip(GlobalVariable &gfb, const GlobalVariable &gbc,
             const KitsuneTTOptions &tto, const CGFBOptions &cgfbOpts);

} // namespace detail

} // namespace llvm

#endif // LLVM_KITSUNE_CGFB_IMPL_H
