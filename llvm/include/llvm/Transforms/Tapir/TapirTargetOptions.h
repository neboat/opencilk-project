//===- TapirTargetIDs.h - Tapir target ID's --------------------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file enumerates the Tapir lowering targets.
//
//===----------------------------------------------------------------------===//

#ifndef TAPIR_TARGET_OPTIONS_H_
#define TAPIR_TARGET_OPTIONS_H_

#include "llvm/Transforms/Tapir/TapirTargetIDs.h"
#include "llvm/Transforms/Tapir/TapirTargetPlugin.h"
#include <optional>

namespace llvm {

// Virtual base class for Target-specific options.
class TapirTargetOptions {
private:
  const TapirTargetID ID;

public:
  TapirTargetID getID() const { return ID; }

  TapirTargetOptions(TapirTargetID ID) : ID(ID) {}
  TapirTargetOptions(const TapirTargetOptions &) = delete;
  TapirTargetOptions &operator=(const TapirTargetOptions &) = delete;
  virtual ~TapirTargetOptions() {}

  // Top-level method for cloning TapirTargetOptions.  Defined in
  // TargetLibraryInfo.
  TapirTargetOptions *clone();
};

// Options for OpenCilkABI Tapir target.
class OpenCilkABIOptions : public TapirTargetOptions {
  std::string RuntimeBCPath;

  OpenCilkABIOptions() = delete;

public:
  OpenCilkABIOptions(StringRef Path)
      : TapirTargetOptions(TapirTargetID::OpenCilk), RuntimeBCPath(Path) {}

  StringRef getRuntimeBCPath() const { return RuntimeBCPath; }

  static bool classof(const TapirTargetOptions *TTO) {
    return TTO->getID() == TapirTargetID::OpenCilk;
  }

protected:
  friend TapirTargetOptions;

  OpenCilkABIOptions *cloneImpl() const {
    return new OpenCilkABIOptions(RuntimeBCPath);
  }
};

// Options for Custom Tapir target.
class TapirTargetPluginOptions : public TapirTargetOptions {
  std::optional<TapirTargetPlugin> TTPlugin = std::nullopt;

  TapirTargetPluginOptions(const std::optional<TapirTargetPlugin> &TTPlugin)
      : TapirTargetOptions(TapirTargetID::Custom), TTPlugin(TTPlugin) {}

public:
  TapirTargetPluginOptions(TapirTargetPlugin &&TTPlugin)
      : TapirTargetOptions(TapirTargetID::Custom), TTPlugin(TTPlugin) {}

  std::optional<TapirTargetPlugin> getPlugin() const { return TTPlugin; }

  static bool classof(const TapirTargetOptions *TTO) {
    return TTO->getID() == TapirTargetID::Custom;
  }

protected:
  friend TapirTargetOptions;

  TapirTargetPluginOptions *cloneImpl() {
    return new TapirTargetPluginOptions(TTPlugin);
  }
};
} // namespace llvm

#endif