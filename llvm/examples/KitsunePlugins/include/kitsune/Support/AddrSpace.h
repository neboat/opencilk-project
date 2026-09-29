//===- AddrSpace.h - Utilities for Kitsune's address spaces ------*- C++ -*_-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Utilities for Kitsune's address spaces
//
//===----------------------------------------------------------------------===//

#ifndef KITSUNE_SUPPORT_ADDR_SPACE_H
#define KITSUNE_SUPPORT_ADDR_SPACE_H

namespace llvm {

namespace KitAS {

/// Address spaces used by Kitsune. The values of each address space must be
/// chosen carefully to avoid conflicting with the values used by other
/// languages/targets supported by LLVM (even if those are not necessarily
/// supported by Kitsune).
enum {
  /// Address space for mobile buffers. These are regions of memory whose
  /// contents may migrate between CPU and GPU. These are typically allocated
  /// using a UVM allocator, but they need not be. The alternative is for the
  /// compiler to explicitly generate memory copy calls to and from host and
  /// device.
  Mobile = 67,
};

} // namespace KitAS

// LLVM has a maximum of 24-bits for the address space, clang has 22 (23
// usually, but we stole a bit in Kitsune for the mobile attribute). Just in
// case, don't use more than 8 bits for any of these.
static_assert(KitAS::Mobile <= 0xFF &&
              "Kitsune's address spaces must occupy no more than 8 bits");

} // namespace llvm

#endif // KITSUNE_SUPPORT_ADDR_SPACE_H
