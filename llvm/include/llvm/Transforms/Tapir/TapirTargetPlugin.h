//===- TapirTargetPlugin.h - Tapir target plugin API -----------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This defines the public entry point for tapir target plugins.
//
// Heavily based on the TTPlugin by Tarun Prabhu and the Kitsune team at LANL.
//
//===----------------------------------------------------------------------===//

#ifndef TAPIR_TARGET_PLUGIN_H
#define TAPIR_TARGET_PLUGIN_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/Error.h"

namespace llvm {

class Module;
class PassBuilder;
class TapirTarget;
class TapirTargetOptions;

/// \macro LLVM_TAPIR_TARGET_PLUGIN_API_VERSION
/// Identifies the API version understood by this plugin.
///
/// When a plugin is loaded, the driver will check it's supported plugin version
/// against that of the plugin. A mismatch is an error. The supported version
/// will be incremented for ABI-breaking changes to the \c TapirTargetPluginInfo
/// struct, i.e. when callbacks are added, removed, or reordered.
#define LLVM_TAPIR_TARGET_PLUGIN_API_VERSION 1

extern "C" {

/// Base class for Custom Tapir target options.
struct CustomTargetOptionsBase {
  /// The API version understood by this plugin, usually \c
  /// LLVM_TAPIR_TARGET_PLUGIN_API_VERSION
  uint32_t APIVersion;
};

/// Information about the plugin required to load its passes
///
/// This struct defines the core interface for pass plugins and is supposed to
/// be filled out by plugin implementors. LLVM-side users of a plugin are
/// expected to use the \c TapirTargetPlugin class below to interface with it.
struct TapirTargetPluginInfo {
  /// The API version understood by this plugin, usually \c
  /// LLVM_TAPIR_TARGET_PLUGIN_API_VERSION
  uint32_t APIVersion;

  /// A meaningful name of the plugin.
  const char *PluginName;

  /// The version of the plugin.
  const char *PluginVersion;

  /// The callback to construct the tapir target object that will be used. The
  /// caller will own the returned object.
  TapirTarget *(*MakeTapirTarget)(Module &, const TapirTargetOptions &,
                                  ModuleAnalysisManager &);

  // CustomTargetOptionsBase *(*MakeCustomTargetOptions)();

  /// The callback for registering plugin passes with a \c PassBuilder
  /// instance
  void (*RegisterPassBuilderCallbacks)(PassBuilder &);

  // /// Callback to get any options that should always be added to the compiler
  // /// (cc1/fc1) when using the plugin. If no additional compiler options are
  // /// required, return an empty string.
  // SmallVector<std::string, 4> (*GetCompilerOptions)();

  // /// Callback to get any options that should always be added to the linker when
  // /// using the plugin. If no additional linker options are required, return
  // /// an empty string.
  // SmallVector<std::string, 4> (*GetLinkerOptions)();
};

} // extern "C"

/// A loaded tapir target plugin.
///
/// An instance of this class wraps a loaded tapir target plugin and gives
/// access to its interface defined by the \c TapirTargetPluginInfo it exposes.
class TapirTargetPlugin {
public:
  // using ExtraArgsList = SmallVector<std::string, 4>;

private:
  /// The path to the dynamic shared object that is the plugin
  std::string Filename;

  /// LLVM's wrapper around the loaded dynamic library
  sys::DynamicLibrary Library;

  /// The plugin information struct
  TapirTargetPluginInfo Info;

private:
  TapirTargetPlugin(const std::string &Filename,
                    const sys::DynamicLibrary &Library)
      : Filename(Filename), Library(Library), Info() {}

public:
  /// Attempts to load a tapir target plugin from a given file.
  ///
  /// \returns Returns an error if either the library cannot be found or loaded,
  /// there is no public entry point, or the plugin implements the wrong API
  /// version.
  LLVM_ABI static Expected<TapirTargetPlugin> load(StringRef Filename);

  /// Get the name loaded plugin file.
  StringRef getFile() const { return Filename; }

  /// Get the plugin name
  StringRef getName() const { return Info.PluginName; }

  /// Get the plugin version
  StringRef getVersion() const { return Info.PluginVersion; }

  /// Get the plugin API version
  uint32_t getAPIVersion() const { return Info.APIVersion; }

  /// Construct a tapir target object. The caller will own the constructed
  /// object.
  TapirTarget *makeTapirTarget(Module &HostM, const TapirTargetOptions &TTO,
                               ModuleAnalysisManager &AM) const {
    return Info.MakeTapirTarget(HostM, TTO, AM);
  }

  /// Invoke the PassBuilder callback registration
  void registerPassBuilderCallbacks(PassBuilder &PB) const {
    Info.RegisterPassBuilderCallbacks(PB);
  }

  // /// Return any options that must always be added to the compiler (cc1/fc1)
  // /// when using this plugin.
  // ExtraArgsList getCompilerOptions() const { return Info.GetCompilerOptions(); }

  // /// Return any options that must always be added to the linker when using this
  // /// plugin.
  // ExtraArgsList getLinkerOptions() const { return Info.GetLinkerOptions(); }
};

} // namespace llvm

/// The public entry point for a Tapir target plugin.
///
/// When a plugin is loaded by the driver, it will call this entry point to
/// obtain information about this plugin. This function needs to be implemented
/// by the plugin, see the example below:
///
/// ```
/// extern "C" ::llvm::TapirTargetPluginInfo LLVM_ATTRIBUTE_WEAK
/// llvmGetTapirTargetPluginInfo() {
///   return {
///     LLVM_TAPIR_TARGET_PLUGIN_API_VERSION, "MyPlugin", "v0.1",
///     [](Module &HostM, const TapirTargetOptions &TTO, ModuleAnalysisManager &AM) {
///       // return a new tapir target
///     },
///     [](PassBuilder &PB) { ... },
///     [] { ... // return a (possibly empty) array of compiler options },
///     [] { ... // return a (possibly empty) array of linker options }
///   };
/// }
/// ```
extern "C" ::llvm::TapirTargetPluginInfo LLVM_ATTRIBUTE_WEAK
llvmGetTapirTargetPluginInfo();

#endif // TAPIR_TARGET_PLUGIN_H
