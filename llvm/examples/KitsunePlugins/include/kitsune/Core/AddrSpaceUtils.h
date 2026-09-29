//==- AddrSpaceUtils.h - Utilities for Kitsune's address spaces --*- C++ -*-==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Utilities related to Kitsune's address spaces.
//
//===----------------------------------------------------------------------===//

#ifndef KITSUNE_CORE_ADDR_SPACE_UTILS_H
#define KITSUNE_CORE_ADDR_SPACE_UTILS_H

namespace llvm {

class Module;

// Strip kitsune-specific address spaces from all pointers in the module. These
// values are moved into the default address space. Return true if at least one
// pointer in any of Kitsune's address spaces was found in the module and
// stripped, false if the module remains unchanged.
bool stripKitsuneAddrSpaces(Module &m);

} // namespace llvm

#endif // KITSUNE_CORE_ADDR_SPACE_UTILS_H
