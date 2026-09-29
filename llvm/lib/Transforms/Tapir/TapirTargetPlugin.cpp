//===- TapirTargetPlugin.cpp - Tapir target plugin API ---------*- C++ -*--===//
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

#include "llvm/Transforms/Tapir/TapirTargetPlugin.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

Expected<TapirTargetPlugin> TapirTargetPlugin::load(StringRef Filename) {
  std::string Err;
  auto Library =
      sys::DynamicLibrary::getPermanentLibrary(Filename.data(), &Err);
  if (!Library.isValid())
    return make_error<StringError>(
        join_items("", "Could not load library '", Filename, "': ", Err),
        inconvertibleErrorCode());

  TapirTargetPlugin Plugin{Filename.data(), Library};

  // llvmGetTapirTargetPluginInfo should be resolved to the definition from the
  // plugin we are currently loading.
  intptr_t GetDetailsFn =
      (intptr_t)Library.getAddressOfSymbol("llvmGetTapirTargetPluginInfo");

  if (!GetDetailsFn)
    // Error if the symbol isn't found
    return make_error<StringError>(
        join_items("", "Plugin entry point not found in '", Filename, "'"),
        inconvertibleErrorCode());

  Plugin.Info = reinterpret_cast<decltype(llvmGetTapirTargetPluginInfo) *>(
      GetDetailsFn)();

  if (Plugin.getAPIVersion() != LLVM_TAPIR_TARGET_PLUGIN_API_VERSION)
    return make_error<StringError>(
        llvm::join_items(
            "", "Wrong API version on plugin '", Filename, "'. Got version ",
            std::to_string(Plugin.getAPIVersion()), ", supported version is ",
            std::to_string(LLVM_TAPIR_TARGET_PLUGIN_API_VERSION)),
        inconvertibleErrorCode());

  if (!Plugin.Info.MakeTapirTarget)
    return createStringError(join_items(
        "", "Missing constructor callback in plugin '", Filename, "'"));

  // if (!Plugin.Info.MakeCustomTargetOptions)
  //   return createStringError(join_items(
  //       "", "Missing target-options constructor callback in plugin '", Filename,
  //       "'"));

  if (!Plugin.Info.RegisterPassBuilderCallbacks)
    return make_error<StringError>(Twine("Empty entry callback in plugin '") +
                                       Filename + "'.'",
                                   inconvertibleErrorCode());

  // if (!Plugin.Info.GetCompilerOptions)
  //   return createStringError(join_items(
  //       "", "Missing compiler options callback in plugin '", Filename, "'"));

  // if (!Plugin.Info.GetLinkerOptions)
  //   return createStringError(join_items(
  //       "", "Missing linker options callback in plugin '", Filename, "'"));

  return Plugin;
}
