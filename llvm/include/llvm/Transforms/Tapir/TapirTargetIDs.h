//===- TapirTargetIDs.h - Tapir target ID's --------------------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file enumerates the available Tapir lowering targets.
//
//===----------------------------------------------------------------------===//

#ifndef TAPIR_TARGET_IDS_H_
#define TAPIR_TARGET_IDS_H_

namespace llvm {

enum class TapirTargetID {
  None,     // Perform no lowering
  Serial,   // Lower to serial projection
  Cheetah,  // Lower to the Cheetah ABI
  CilkPlus, // Lower to the Cilk Plus ABI
  Lambda,   // Lower to generic Lambda ABI
  OMPTask,  // Lower to OpenMP task ABI
  OpenCilk, // Lower to OpenCilk ABI
  Qthreads, // Lower to Qthreads
  Last_TapirTargetID
};
} // end namespace llvm

#endif
