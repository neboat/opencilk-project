//===- KitsuneTTOptions.h - Options shared by tapir targets ----*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The options objects for all the known tapir targets.
//
//===----------------------------------------------------------------------===//

#ifndef KITSUNE_CORE_KITSUNETTOPTIONS_H
#define KITSUNE_CORE_KITSUNETTOPTIONS_H

#include "kitsune/Core/OptznLevel.h"
// #include "kitsune/Core/TTPlugin.h"
#include "kitsune/Core/Tapir.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/Passes/OptimizationLevel.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Transforms/Tapir/TapirTargetOptions.h"
#include "llvm/Transforms/Tapir/TapirTargetPlugin.h"

#include <optional>

namespace llvm {

namespace driver {
class KitsuneOptions;
}

using KitsuneOptions = driver::KitsuneOptions;
using FPOpFusionMode = FPOpFusion::FPOpFusionMode;

/// Options for the tapir targets. The same options object is used for all
/// tapir targets. We could split it into types for each tapir target, but it
/// doesn't really help with anything. Having everything "under one roof" makes
/// it a shade easier to support multiple tapir targets in a single compilation.
class KitsuneTTOptions : public CustomTargetOptionsBase {
public:
  /// The default optimization level to use when one has not been specified.
  /// This is mostly useful for the tools that create this options object
  /// simply to parse the kitsune options, but don't actually care about the
  /// optimization level.
  ///
  /// Most frontends that drive the kitsune transformation pipeline should set
  /// an appropriate optimization level based on the frontend options.
  ///
  /// We could set this higher because we generally want to enable as many
  /// optimizations as possible, but we still try to hew relatively close to
  /// most compiler frontends by explicitly requiring optimizations. We could
  /// have set this to -O0, but most of the pipeline will not work in that case,
  /// so we set it to -O1 - the lowest level that is still usable.
  static constexpr OptznLevel defaultOptznLevel = OptznLevel::O1;

  /// The default fusion mode for floating point operations. This is mainly
  /// used by tools that need to create a tapir target options object, but don't
  /// necessarily need to use the fusion mode.
  static constexpr FPOpFusionMode defaultFpOpFusionMode =
      FPOpFusionMode::Standard;

private:
  /// The primary tapir target. This is guaranteed to always be set since there
  /// must always be at least one tapir target.
  TTID tt;

  /// When multiple tapir targets are fully supported, these are the secondary
  /// tapir targets. This set will *not* include the primary tapir target.
  llvm::SmallSetVector<TTID, 4> tts;

  /// Options common to all tapir targets
  /// @{
  /// Enable verbose mode on the tapir target. Not all tapir targets implement
  /// verbose mode, so there is no guarantee that this has any effect on a
  /// given tapir target.
  unsigned tapirVerbose : 1;

  /// If true, set the Kitsune runtime in verbose mode. Not all tapir targets
  /// use Kitsune's runtime. In such cases, setting this to true will no effect.
  unsigned kitrtVerbose : 1;

  /// If true, enable generation of calls to prefetch managed memory between
  /// host and GPU.
  unsigned gpuPrefetch : 1;
  /// @}

  /// The optimization level set on the command line. This level will be used
  /// for both the middle-end optimizations on the kernel functions and the
  /// backend GPU code generators (including external assemblers as needed).
  OptznLevel optLevel = defaultOptznLevel;

  /// How to use fuse floating-point operations.
  FPOpFusion::FPOpFusionMode fpOpFusionMode = defaultFpOpFusionMode;

  /// Options common to the GPU tapir targets
  /// @{
  /// If this is non-zero, this value will be used when launching all kernels
  /// which do not already have a custom FixedThreadsPerBlock value. If this is
  /// not set, the number of threads per block to use will be determined by the
  /// runtime.
  unsigned fixedThreadsPerBlock = 0;

  /// If this is non-zero, the threads per block will not be allowed to exceed
  /// this value.
  unsigned maxThreadsPerBlock = 0;

  /// The path to LLD that was built with Kitsune. If clang is invoked from the
  /// build directory, this will be the lld that is in the build directory.
  std::string lld;
  /// @}

  /// Options for the cuda tapir target
  /// @{
  /// The NVIDIA GPU architecture for which to generate code.
  std::string cudaArch;

  /// The virtual architecture (compute_*), for the @ref cudaArch.
  std::string cudaVirtArch;

  /// Cuda target features for the current @ref cudaArch. This is a string with
  /// the format: +feature1,+feature2,-feature3...
  std::string cudaTargetFeatures;

  /// The absolute path to the cuda runtime bitcode file.
  std::string cudaRuntimeBCFile;
  /// @}

//   /// Options for the 'custom' tapir target
//   /// @{
//   /// The tapir target plugin object. This contains a wrapper around the actual
//   /// dynamic library among other things.
//   std::optional<TTPlugin> ttPlugin = std::nullopt;
//   /// @}

  /// Options for the hip tapir target
  /// @{
  /// The AMD GPU architecture for which to generate code.
  std::string hipArch;

  /// The value of the sramecc feature.
  MaybeBool hipSRAMECC;

  /// The value of the xnack feature.
  MaybeBool hipXnack;

  /// The target features for the current hip GPU architecture. This is a string
  /// with the format: '+feature1,+feature2,-feature3...'.
  std::string hipTargetFeatures;

  /// The hip bitcode files needed by the hip tapir target. This will be
  /// computed by the driver from either the default value of @ref cudaArch, or
  /// from the --tapir-hip-arch option if it was provided.
  std::vector<std::string> hipRuntimeBCFiles;
  /// @}

  // /// Options for the opencilk tapir target
  // /// @{
  // std::string openCilkRuntimeBCFile;
  // /// @}

private:
  KitsuneTTOptions() = default;

  /// Create an options object with the given primary tapir target.
  KitsuneTTOptions(TTID tt);

  /// Create an options object with the given primary tapir target and the given
  /// secondary tapir targets.
  KitsuneTTOptions(TTID tt, const std::vector<TTID> tts);

  Error validateCudaOptions() const;
  Error validateCustomOptions() const;
  Error validateHipOptions() const;
  Error validateOpenCilkOptions() const;

public:
  // Check the object for inconsistencies and invalid values. If none are found,
  // return Error::success(). Otherwise, return an error.
  Error validate() const;

  // Set the Kitsune-specific optimization level.
  void setOptznLevel(OptznLevel optLevel) { this->optLevel = optLevel; }
  void setOptznLevelFrom(OptimizationLevel optLevel);

  /// Get the primary tapir target ID.
  TTID getTTID() const { return tt; }

  /// @{
  /// Options common to all tapir targets.
  bool getTapirVerbose() const { return tapirVerbose; }
  bool getKitrtVerbose() const { return kitrtVerbose; }
  OptznLevel getOptznLevel() const { return optLevel; }
  FPOpFusion::FPOpFusionMode getFPOpFusionMode() const {
    return fpOpFusionMode;
  }
  /// @}

  /// @{
  /// Options common to the GPU tapir targets. In practice, some of these are
  /// only used by some tapir targets. They are here because they are not tied,
  /// in principle, to any one. For example, the path to LLD is only used by
  /// the hip tapir target.
  unsigned getFixedThreadsPerBlock() const { return fixedThreadsPerBlock; }
  unsigned getMaxThreadsPerBlock() const { return maxThreadsPerBlock; }
  bool getGPUPrefetch() const { return gpuPrefetch; }
  StringRef getLLD() const { return lld; }
  /// @}

  /// @{
  /// Options for the cuda tapir target.
  StringRef getCudaArch() const { return cudaArch; }
  StringRef getCudaVirtArch() const { return cudaVirtArch; }
  StringRef getCudaTargetFeatures() const { return cudaTargetFeatures; }
  StringRef getCudaRuntimeBCFile() const { return cudaRuntimeBCFile; }
  /// @}

//   /// @{
//   /// Options for the the 'custom' tapir target. These should only be called
//   /// when we know that the options object is valid.
//   std::optional<TTPlugin> getTTPlugin() const { return ttPlugin; }
//   /// @}

  /// @{
  /// Options for the hip tapir target.
  StringRef getHipArch() const { return hipArch; }
  MaybeBool getHipSRAMECC() const { return hipSRAMECC; }
  MaybeBool getHipXnack() const { return hipXnack; }
  StringRef getHipTargetFeatures() const { return hipTargetFeatures; }
  const std::vector<std::string> &getHipRuntimeBCFiles() const {
    return hipRuntimeBCFiles;
  }
  /// @}

  // /// @{
  // /// Options for the opencilk tapir target
  // StringRef getOpenCilkRuntimeBCFile() const { return openCilkRuntimeBCFile; }
  // /// @}

  /// Print the options object to the given output stream. If \ref all is true,
  /// all the options will be printed, otherwise only those options relevant to
  /// the primary tapir target will be printed.
  void print(llvm::raw_ostream &os, bool all = false) const;

  /// Construct an options object from the given frontend options. If a TTID
  /// is not set in the kitsune options, std::nullopt is returned.
  static std::optional<KitsuneTTOptions> create(const KitsuneOptions &kitOpts,
                                         OptznLevel optLevel,
                                         FPOpFusionMode fpOpFusionMode);

  static KitsuneTTOptions createPluginOptions(TTID ID);

  /// Construct an options object initialized from the "shared" command line
  /// options. The "shared" options are those used by one or more tools or
  /// utilities in addition to being available to opt. This is intended to be a
  /// very minimal set, and guaranteed to succeed.
  static std::optional<KitsuneTTOptions>
  createFromSharedCommandLineOptions(OptznLevel optznLevel = OptznLevel::O0);

  /// Construct an options object initialized from the command line options
  /// if the --tapir option was provided. If the --tapir option is not provided,
  /// return std::nullopt.
  static std::optional<KitsuneTTOptions> createFromCommandLine(OptznLevel optznLevel);

  /// Construct an options object initialized from the command line options
  /// if the --tapir option was provided. If the --tapir option is not provided,
  /// return std::nullopt.
  static std::optional<KitsuneTTOptions> createFromCommandLine(unsigned speedupLevel);

  /// Construct an options object initialized from the command line options
  /// with the given optimization level. \ref optLevel must be one of {0, 1, 2,
  /// 3, s, z}. It is an error if \ref optLevel is not one of these. If the
  /// --tapir option is not provided, return std::nullopt.
  static std::optional<KitsuneTTOptions> createFromCommandLine(char optLevel);
};

} // namespace llvm

#endif // KITSUNE_CORE_KITSUNETTOPTIONS_H
