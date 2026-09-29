//===- OptznLevel.h - (Yet another) optimization level object --*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Kitsune-specific representation of the optimization levels known to the
// compiler.
//
//===----------------------------------------------------------------------===//

#ifndef KITSUNE_CORE_OPTZN_LEVEL_H
#define KITSUNE_CORE_OPTZN_LEVEL_H

namespace llvm {

/// The various optimization levels supported by Kitsune.
///
/// NOTE: The levels supported here are no different from the levels supported
/// Kitsune. In principle, one could extend this to have some subtle form of
/// differential optimization levels for host and device code (for the tapir
/// targets that use them - particularly the GPU-centric ones), but that is not
/// really why this class exists.
///
/// The motivation for this is to avoid the inevitable circular dependences that
/// are introduced if we use LLVM's \ref OptimizationLevel object. Instances of
/// this class are defined in libLLVMPasses. That is "reasonable" in the case of
/// LLVM since the frontends (clang, flang) don't use it for much and the
/// middle-end only uses it to construct the pass pipeline. However, Kitsune
/// records this in the \ref TTOptions object in order to use it in the nested
/// emb-optimize pass. This results in a circular dependence between LLVMPasses
/// and LLVMKitCore - something that we would really like to avoid. Moving the
/// OptimizationLevel class to a different library was attempted - LLVMSupport
/// seemed a natural place for it, but that caused more problems.
///
/// LLVMSupport is built with -Werror=global-constructors where possible. LLVM's
/// OptimizationLevel class is not intended to be instantiated on demand.
/// Instead, singleton instances of the class are provided for each of the
/// optimization levels supported by the compiler. Obviously, instantiating
/// these requires the use of global constructors. To work around this, the
/// definition of the singletons would have to be moved to a less obvious
/// library.
///
/// We have chosen, instead, to create our own representation of the
/// optimization levels and to convert them to LLVM's OptimizationLevel on
/// demand. It's annoying, but, arguably, less so than introducing yet more
/// circular dependencies between LLVM and Kitsune than already exist (see
/// llvm/CMakeLists.txt for a details).
enum class OptznLevel {
  /// No optimizations
  O0,

  /// "Some" optimizations (between -O0 and -O2)
  O1,

  /// "Most" optimizations
  O2,

  /// Like -O2, but includes those that take longer and/or produce larger code,
  /// in an attempt to make the program run faster
  O3,

  /// Like -O2 but with extra optimizations to reduce code size
  Os,

  /// Like -Os, but is more aggessive when it comes to reducing code size
  Oz,
};

} // namespace llvm

#endif // KITSUNE_CORE_OPTZN_LEVEL_H
