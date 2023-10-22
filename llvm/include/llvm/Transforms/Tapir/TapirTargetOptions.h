//===- TapirTargetOptions.h - Options to pass to Tapir targets -*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file enumerates the available option structures for Tapir targets.
//
//===----------------------------------------------------------------------===//

#ifndef TAPIR_TARGET_OPTIONS_H_
#define TAPIR_TARGET_OPTIONS_H_

#include "llvm/ADT/StringRef.h"
// #include "llvm/IR/IRBuilder.h"
#include "llvm/Support/Casting.h"
#include "llvm/Transforms/Tapir/LoweringUtils.h"

namespace llvm {

// Options for OpenCilkABI Tapir target.
class OpenCilkABIOptions : public TapirTargetOptions {
  std::string RuntimeBCPath;

  OpenCilkABIOptions() = delete;

public:
  OpenCilkABIOptions(StringRef Path)
      : TapirTargetOptions(TTO_OpenCilk), RuntimeBCPath(Path) {}

  StringRef getRuntimeBCPath() const {
    return RuntimeBCPath;
  }

  static bool classof(const TapirTargetOptions *TTO) {
    return TTO->getKind() == TTO_OpenCilk;
  }

protected:
  friend TapirTargetOptions;

  OpenCilkABIOptions *cloneImpl() const {
    return new OpenCilkABIOptions(RuntimeBCPath);
  }
};

// Options for LambdaABI Tapir target.
class LambdaABIOptions : public TapirTargetOptions {
  std::string RuntimeBCPath;

  LambdaABIOptions() = delete;

public:
  LambdaABIOptions(StringRef Path)
      : TapirTargetOptions(TTO_Lambda), RuntimeBCPath(Path) {}

  StringRef getRuntimeBCPath() const {
    return RuntimeBCPath;
  }

  static bool classof(const TapirTargetOptions *TTO) {
    return TTO->getKind() == TTO_Lambda;
  }

protected:
  friend TapirTargetOptions;

  LambdaABIOptions *cloneImpl() const {
    return new LambdaABIOptions(RuntimeBCPath);
  }
};

// Options for ChiABI Tapir target.
class ChiABIOptions : public TapirTargetOptions {
  std::string HostBCPath = "";
  std::string DeviceBCPath = "";
  InputsCallbackTy InputsCallback;
  LoopLaunchCallbackTy LoopLaunchCallback;
  bool SingleKernelModule = true;

  ChiABIOptions() = delete;

public:
  ChiABIOptions(StringRef HostBCPath, StringRef DeviceBCPath,
                InputsCallbackTy InputsCallback,
                LoopLaunchCallbackTy LoopLaunchCallback,
                bool SingleKernelModule = true)
      : TapirTargetOptions(TTO_Chi), HostBCPath(HostBCPath),
        DeviceBCPath(DeviceBCPath), InputsCallback(InputsCallback),
        LoopLaunchCallback(LoopLaunchCallback),
        SingleKernelModule(SingleKernelModule) {}

  bool useSingleKernelModule() const { return SingleKernelModule; }
  StringRef getHostBCPath() const { return HostBCPath; }
  StringRef getDeviceBCPath() const { return DeviceBCPath; }
  InputsCallbackTy getInputsCallback() const { return InputsCallback; }
  LoopLaunchCallbackTy getLoopLaunchCallback() const {
    return LoopLaunchCallback;
  }

  static bool classof(const TapirTargetOptions *TTO) {
    return TTO->getKind() == TTO_Chi;
  }

protected:
  friend TapirTargetOptions;

  ChiABIOptions *cloneImpl() const {
    return new ChiABIOptions(HostBCPath, DeviceBCPath, InputsCallback,
                             LoopLaunchCallback);
  }
};

} // end namespace llvm

#endif
