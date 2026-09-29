//===- KitsuneTTOptions.cpp - Options for the tapir targets ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implementation for the KitsuneTTOptions object. Also contains any command line
// options shared by some or all tapir targets.
//
//===----------------------------------------------------------------------===//

#include "kitsune/Core/KitsuneTTOptions.h"
// #include "kitsune/Config/config.h"
#include "kitsune/Core/CommandLineOptions.h"
#include "kitsune/Core/OptznLevel.h"
#include "kitsune/Core/TTUtils.h"
#include "kitsune/Frontend/KitsuneOptions.h"
#include "kitsune/Support/OptznLevelUtils.h"
#include "kitsune/Support/ToString.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/CodeGen/CommandFlags.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/SourceMgr.h"

using namespace llvm;

// The options here are those that are used to initialize the TTOptions object
// *only*. Several tapir targets have additional command line options that can
// be used to tweak their behavior. Those are intended for experimentation. If
// any are deemed to be generally useful, they should be added here and a
// corresponding frontend option should be created for them.

// -------------------- options common to all tapir targets --------------------

static cl::opt<TTID>
    clTapir("tapir", cl::desc("The primary tapir target"), cl::init(TTID::Nolo),
            cl::value_desc("target"), cl::cat(cl::catKitClOpts),
            cl::values(clEnumValN(TTID::Nolo, "nolo", ""),
                       clEnumValN(TTID::Serial, "serial", ""),
                       clEnumValN(TTID::Cuda, "cuda", ""),
                       clEnumValN(TTID::Custom, "custom", ""),
                       // clEnumValN(TTID::GPUABI, "gpuabi", ""),
                       clEnumValN(TTID::Hip, "hip", ""),
                       clEnumValN(TTID::Lambda, "lambda", ""),
                       clEnumValN(TTID::OMPTask, "omptask", ""),
                       clEnumValN(TTID::OpenCilk, "opencilk", ""),
                       clEnumValN(TTID::OpenMP, "openmp", ""),
                       clEnumValN(TTID::Pthreads, "pthreads", ""),
                       clEnumValN(TTID::Qthreads, "qthreads", ""),
                       clEnumValN(TTID::Realm, "realm", "")));

/// This was the option originally in tapir, but in Kitsune, we prefer to use
/// --tapir instead.
static cl::alias clTapirTarget("kit-tapir-target", cl::desc("Alias for --tapir"),
                               cl::aliasopt(clTapir),
                               cl::cat(cl::catKitClOpts));

static cl::opt<bool>
    clTapirVerbose("tapir-verbose", cl::init(false),
                   cl::desc("Enable verbose mode in all tapir targets"),
                   cl::cat(cl::catKitClOpts));

static cl::opt<bool>
    clKitrtVerbose("kitrt-verbose", cl::init(false),
                   cl::desc("Enable verbose mode in kitsune's runtime"),
                   cl::cat(cl::catKitClOpts));

static cl::opt<std::string> clLLD("tapir-lld", cl::init(""),
                                  cl::desc("Path to LLD"),
                                  cl::cat(cl::catKitClOpts));

// ------------------ optimization options ------------------

static cl::opt<OptznLevel>
    clTTOptLevel("tt-opt-level", cl::init(OptznLevel::O1),
                 cl::desc("Optimization level for Tapir target"),
                 cl::values(clEnumValN(OptznLevel::O0, "O0", ""),
                            clEnumValN(OptznLevel::O1, "O1", ""),
                            clEnumValN(OptznLevel::O2, "O2", ""),
                            clEnumValN(OptznLevel::O3, "O3", ""),
                            clEnumValN(OptznLevel::Os, "Os", ""),
                            clEnumValN(OptznLevel::Oz, "Oz", "")));

static cl::opt<FPOpFusionMode> clFPOpFusionMode(
    "tt-fp-fusion-mode",
    cl::desc("Tapir target fusion mode for floating-point operations"),
    cl::init(FPOpFusionMode::Standard),
    cl::values(clEnumValN(FPOpFusionMode::Fast, "fast", ""),
               clEnumValN(FPOpFusionMode::Standard, "standard", ""),
               clEnumValN(FPOpFusionMode::Strict, "strict", "")),
    cl::cat(cl::catKitClOpts));

// ------------------ options common to the GPU tapir targets ------------------

static cl::opt<unsigned> clFixedThreadsPerBlock(
    "tapir-gpu-tpb", cl::init(0),
    cl::desc("Use a fixed number of threads per block for all GPU kernel "
             "launches unless overridden with pragmas. If this is not provided "
             "the threads per block will be calculated by Kitsune's runtime. "
             "Can be at most 1024"),
    cl::cat(cl::catKitClOpts));

static cl::opt<unsigned> clMaxThreadsPerBlock(
    "tapir-gpu-max-tpb", cl::init(0),
    cl::desc(
        "The maximum number of threads per block to launch. If this is not "
        "provided, Kitsune's runtime is free to launch as many threads per "
        "block as it sees fit"),
    cl::cat(cl::catKitClOpts));

static cl::opt<bool>
    clGPUPrefetch("tapir-gpu-prefetch",
                  cl::init(KitsuneOptions::defaultGPUPrefetch),
                  cl::desc("Enable generation of calls to prefetch managed "
                           "memory between host and device"),
                  cl::cat(cl::catKitClOpts));

// ------------------------- cuda tapir target options -------------------------

static const std::string clCudaArchHelp =
    join_items(KitsuneOptions::defaultCudaArch,
               "NVIDIA GPU architecture (default = ", ")");
static cl::opt<std::string>
    clCudaArch("tapir-cuda-arch",
               cl::init(KitsuneOptions::defaultCudaArch.str()),
               cl::desc(clCudaArchHelp), cl::cat(cl::catKitClOpts));

static cl::opt<std::string>
    clCudaVirtArch("tapir-cuda-virt-arch", cl::init(""),
                   cl::desc("NVIDIA GPU virtual architecture"),
                   cl::cat(cl::catKitClOpts));

static cl::opt<std::string> clCudaFeatures(
    "tapir-cuda-features", cl::init(""),
    cl::desc("The target features to use in the cuda tapir target"),
    cl::cat(cl::catKitClOpts));

static cl::opt<std::string>
    clCudaRuntimeBCFile("tapir-cuda-runtime-bc", cl::init(""),
                        cl::desc("Path to the cuda runtime bitcode file"),
                        cl::cat(cl::catKitClOpts));

// ----------------------- 'custom' tapir target options -----------------------

static cl::opt<std::string>
    clCustomTTPlugin("tapir-plugin", cl::init(""),
                     cl::desc("Load a plugin containing a custom tapir target "
                              "from a shared object file"),
                     cl::cat(cl::catKitClOpts));

// ------------------------- hip tapir target options -------------------------

static const std::string clHipArchHelp = join_items(
    KitsuneOptions::defaultHipArch, "AMD GPU architecture (default = ", ")");
static cl::opt<std::string>
    clHipArch("tapir-hip-arch", cl::init(KitsuneOptions::defaultHipArch.str()),
              cl::desc(clHipArchHelp), cl::cat(cl::catKitClOpts));

static const std::string clHipSRAMECCHelp = join_items(
    toString(KitsuneOptions::defaultHipSRAMECC),
    "Whether to enable the sramecc target feature (default = '", "')");
static cl::opt<MaybeBool> clHipSRAMECC(
    "tapir-hip-sramecc", cl::init(KitsuneOptions::defaultHipSRAMECC),
    cl::desc(clHipSRAMECCHelp),
    cl::values(
        clEnumValN(MaybeBool::Off, "off", "Set the sramecc- target feature"),
        clEnumValN(MaybeBool::On, "on", "Set the sramecc+ target feature"),
        clEnumValN(MaybeBool::Any, "any", "Leave the sramecc feature unset")),
    cl::cat(cl::catKitClOpts));

static const std::string clHipXnackHelp =
    join_items(toString(KitsuneOptions::defaultHipXnack),
               "Whether to enable the xnack target feature (default = '", "')");
static cl::opt<MaybeBool> clHipXnack(
    "tapir-hip-xnack", cl::init(KitsuneOptions::defaultHipXnack),
    cl::desc(clHipXnackHelp),
    cl::values(
        clEnumValN(MaybeBool::Off, "off", "Set the xnack- target feature"),
        clEnumValN(MaybeBool::On, "on", "Set the xnack+ target feature"),
        clEnumValN(MaybeBool::Any, "any", "Leave the xnack feature unset")),
    cl::cat(cl::catKitClOpts));

static cl::opt<std::string> clHipFeatures(
    "tapir-hip-features", cl::init(""),
    cl::desc("The target features to use in the hip tapir target"),
    cl::cat(cl::catKitClOpts));

static cl::list<std::string> clHipRuntimeBCFiles(
    "tapir-hip-runtime-bcs",
    cl::desc("The bitcode files to use in the hip tapir target"),
    cl::cat(cl::catKitClOpts), cl::CommaSeparated);

// // ----------------------- opencilk tapir target options -----------------------

// static cl::opt<std::string> clOpenCilkRuntimeBCFile(
//     "tapir-opencilk-runtime-bc", cl::init(""),
//     cl::desc("Path to the bitcode file for the OpenCilk runtime bitcode file"));

// static cl::alias
//     clOpenCilkRuntimeBCPath("opencilk-runtime-bc-path", cl::NotHidden,
//                             cl::aliasopt(clOpenCilkRuntimeBCFile),
//                             cl::desc("Alias for --tapir-opencilk-runtime-bc"));

// -----------------------------------------------------------------------------

#define CHECK(expr)                                                            \
  if (Error e = (expr))                                                        \
    return e;                                                                  \
  else
#define ELSE_CHECK(expr)                                                       \
  if (Error e = (expr))                                                        \
    return e;                                                                  \
  else
#define ELSE_SUCCESS return Error::success

// If a string option is provided exactly once, it must be non-empty
static Error validateStringOption(const cl::opt<std::string> &clOpt) {
  if (clOpt.getNumOccurrences() == 1)
    if (clOpt.empty())
      return createStringError(join_items("", "for the --", clOpt.ArgStr,
                                          " option: value '", clOpt,
                                          "' is invalid"));
  return Error::success();
}

// If a list option is provided exactly once, it must contain at least one
// non-empty string
static Error validateListOption(const cl::list<std::string> &clOpt) {
  auto isEmpty = [](const std::string &s) -> bool { return s.empty(); };

  if (clOpt.empty() || std::all_of(clOpt.begin(), clOpt.end(), isEmpty))
    return createStringError(
        join_items("", "for the --", clOpt.ArgStr,
                   " option: at least one valid value is required"));
  return Error::success();
}

// The given option must be provided exactly once.
static Error validateRequiredOption(const cl::Option &clOpt) {
  if (clOpt.getNumOccurrences() != 1)
    return createStringError(join_items(
        "", "the --", clOpt.ArgStr, " option must be provided exactly once"));
  return Error::success();
}

// The given string option must be provided exactly once. The value must be
// non-empty
static Error validateRequiredStringOption(const cl::opt<std::string> &clOpt) {
  CHECK(validateRequiredOption(clOpt))
  ELSE_CHECK(validateStringOption(clOpt))
  ELSE_SUCCESS();
}

// The given list option must be provided exactly once. The list must contain
// at least one non-empty string
static Error validateRequiredListOption(const cl::list<std::string> &clOpt) {
  if (clOpt.getNumOccurrences() == 0)
    return createStringError(join_items(
        "", "the --", clOpt.ArgStr, " option must be provided exactly once"));
  else if (Error e = validateListOption(clOpt))
    return e;
  else
    return Error::success();
}

// The threads-per-block options must be provided at most once with a value in
// the range [1,1024]. The options are optional.
static Error validateThreadsPerBlock(const KitsuneTTOptions &tto) {
  auto validate = [](const cl::opt<unsigned> &clOpt) -> Error {
    if (clOpt.getNumOccurrences())
      if (clOpt < 1 || clOpt > 1024)
        return createStringError(
            join_items("", "for the --", clOpt.ArgStr, " option: value '",
                       std::to_string(clOpt), "' is not in range [1,1024]"));
    return Error::success();
  };

  CHECK(validate(clFixedThreadsPerBlock))
  ELSE_CHECK(validate(clMaxThreadsPerBlock))
  ELSE_SUCCESS();
}

static Error validateSupportBCFiles(TTID tt, const KitsuneTTOptions &tto) {
  LLVMContext ctx;
  return getSupportModule(tt, tto, ctx).takeError();
}

KitsuneTTOptions::KitsuneTTOptions(TTID tt) : tt(tt) {}

Error KitsuneTTOptions::validateCudaOptions() const {
  CHECK(validateThreadsPerBlock(*this))
  ELSE_CHECK(validateRequiredStringOption(clCudaArch))
  ELSE_CHECK(validateRequiredStringOption(clCudaRuntimeBCFile))
  ELSE_CHECK(validateStringOption(clCudaVirtArch))
  ELSE_CHECK(validateStringOption(clCudaFeatures))
  ELSE_CHECK(validateSupportBCFiles(TTID::Cuda, *this))
  ELSE_SUCCESS();
}

// Error KitsuneTTOptions::validateCustomOptions() const {
//   CHECK(validateRequiredStringOption(clCustomTTPlugin))
//   ELSE_CHECK(TTPlugin::load(clCustomTTPlugin).takeError())
//   ELSE_SUCCESS();
// }

Error KitsuneTTOptions::validateHipOptions() const {
  CHECK(validateThreadsPerBlock(*this))
  ELSE_CHECK(validateRequiredStringOption(clHipArch))
  ELSE_CHECK(validateRequiredListOption(clHipRuntimeBCFiles))
  ELSE_CHECK(validateStringOption(clHipFeatures))
  ELSE_CHECK(validateSupportBCFiles(TTID::Hip, *this))
  ELSE_SUCCESS();
}

// Error KitsuneTTOptions::validateOpenCilkOptions() const {
//   CHECK(validateRequiredStringOption(clOpenCilkRuntimeBCFile))
//   ELSE_CHECK(validateSupportBCFiles(TTID::OpenCilk, *this))
//   ELSE_SUCCESS();
// }

Error KitsuneTTOptions::validate() const {
  switch (tt) {
  case TTID::Cuda:
    return validateCudaOptions();
  case TTID::Custom:
    return validateCustomOptions();
  case TTID::Hip:
    return validateHipOptions();
  case TTID::OpenCilk:
    return validateOpenCilkOptions();
  case TTID::Nolo:
  case TTID::Pthreads:
  case TTID::Serial:
    // There are no options specific to these tapir targets that need to be
    // checked.
    return Error::success();
  case TTID::Lambda:
  case TTID::OMPTask:
  case TTID::OpenMP:
  case TTID::Qthreads:
  case TTID::Realm:
    // These options are not fully supported.
    break;
  }
  llvm_unreachable("TTID not handled!");
}

void KitsuneTTOptions::setOptznLevelFrom(OptimizationLevel optLevel) {
  unsigned speedupLevel = optLevel.getSpeedupLevel();
  unsigned sizeLevel = optLevel.getSizeLevel();
  OptznLevel optznLevel = createOptznLevelFrom(speedupLevel, sizeLevel);

  setOptznLevel(optznLevel);
}

KitsuneTTOptions KitsuneTTOptions::createPluginOptions(TTID ID) {
  KitsuneTTOptions tto(ID);

  tto.tapirVerbose = clTapirVerbose;
  tto.kitrtVerbose = clTapirVerbose || clKitrtVerbose;
  tto.optLevel = clTTOptLevel;
  tto.fpOpFusionMode = clFPOpFusionMode;
  tto.lld = clLLD;
  if (clFixedThreadsPerBlock)
    tto.fixedThreadsPerBlock = clFixedThreadsPerBlock;
  if (clMaxThreadsPerBlock)
    tto.maxThreadsPerBlock = clMaxThreadsPerBlock;
  tto.gpuPrefetch = clGPUPrefetch;

  // Set cuda tapir target options
  tto.cudaArch = clCudaArch;
  tto.cudaVirtArch = clCudaVirtArch;
  tto.cudaTargetFeatures = clCudaFeatures;
  tto.cudaRuntimeBCFile = clCudaRuntimeBCFile;

  // // Set 'custom' tapir target options
  // if (clCustomTTPlugin.getNumOccurrences()) {
  //   // If the plugin could not be loaded, ignore the error here. In this case,
  //   // the validate() method will be called by users before using this object,
  //   // at which time the error will be caught and returned.
  //   if (Expected<TTPlugin> ttPlugin = TTPlugin::load(clCustomTTPlugin))
  //     tto.ttPlugin = *ttPlugin;
  //   else
  //     (void)toString(ttPlugin.takeError());
  // }

  // Set hip tapir target options
  tto.hipArch = clHipArch;
  tto.hipSRAMECC = clHipSRAMECC;
  tto.hipXnack = clHipXnack;
  tto.hipTargetFeatures = clHipFeatures;
  tto.hipRuntimeBCFiles = clHipRuntimeBCFiles;

  // // Set opencilk tapir target options
  // tto.openCilkRuntimeBCFile = clOpenCilkRuntimeBCFile;

  // FIXME: This is here purely for debugging because it was in HipABI.cpp
  // originally. It really should go away.
  if (std::optional<std::string> tpb =
          sys::Process::GetEnv("KITHIP_THREADS_PER_BLOCK")) {
    if (clFixedThreadsPerBlock)
      errs() << "kitsune[hipabi]: Note that KITHIP_THREADS_PER_BLOCK is "
             << "overriding command line args.\n";
    tto.fixedThreadsPerBlock = std::stoi(tpb.value());
  }

  return tto;
}

std::optional<KitsuneTTOptions>
KitsuneTTOptions::createFromSharedCommandLineOptions(OptznLevel optznLevel) {
  if (clTapir.getNumOccurrences())
    return KitsuneTTOptions(clTapir);
  return std::nullopt;
}

std::optional<KitsuneTTOptions>
KitsuneTTOptions::createFromCommandLine(OptznLevel optznLevel) {
  if (!clTapir.getNumOccurrences())
    return std::nullopt;

  KitsuneTTOptions tto(clTapir);

  // Set common tapir target options
  tto.tapirVerbose = clTapirVerbose;
  tto.kitrtVerbose = clTapirVerbose || clKitrtVerbose;
  tto.optLevel = optznLevel;
  tto.fpOpFusionMode = codegen::getFuseFPOps();
  tto.lld = clLLD;
  if (clFixedThreadsPerBlock)
    tto.fixedThreadsPerBlock = clFixedThreadsPerBlock;
  if (clMaxThreadsPerBlock)
    tto.maxThreadsPerBlock = clMaxThreadsPerBlock;
  tto.gpuPrefetch = clGPUPrefetch;

  // Set cuda tapir target options
  tto.cudaArch = clCudaArch;
  tto.cudaVirtArch = clCudaVirtArch;
  tto.cudaTargetFeatures = clCudaFeatures;
  tto.cudaRuntimeBCFile = clCudaRuntimeBCFile;

  // // Set 'custom' tapir target options
  // if (clCustomTTPlugin.getNumOccurrences()) {
  //   // If the plugin could not be loaded, ignore the error here. In this case,
  //   // the validate() method will be called by users before using this object,
  //   // at which time the error will be caught and returned.
  //   if (Expected<TTPlugin> ttPlugin = TTPlugin::load(clCustomTTPlugin))
  //     tto.ttPlugin = *ttPlugin;
  //   else
  //     (void)toString(ttPlugin.takeError());
  // }

  // Set hip tapir target options
  tto.hipArch = clHipArch;
  tto.hipSRAMECC = clHipSRAMECC;
  tto.hipXnack = clHipXnack;
  tto.hipTargetFeatures = clHipFeatures;
  tto.hipRuntimeBCFiles = clHipRuntimeBCFiles;

  // // Set opencilk tapir target options
  // tto.openCilkRuntimeBCFile = clOpenCilkRuntimeBCFile;

  // FIXME: This is here purely for debugging because it was in HipABI.cpp
  // originally. It really should go away.
  if (std::optional<std::string> tpb =
          sys::Process::GetEnv("KITHIP_THREADS_PER_BLOCK")) {
    if (clFixedThreadsPerBlock)
      errs() << "kitsune[hipabi]: Note that KITHIP_THREADS_PER_BLOCK is "
             << "overriding command line args.\n";
    tto.fixedThreadsPerBlock = std::stoi(tpb.value());
  }

  return tto;
}

std::optional<KitsuneTTOptions>
KitsuneTTOptions::createFromCommandLine(unsigned speedupLevel) {
  return createFromCommandLine(createOptznLevelFrom(speedupLevel));
}

std::optional<KitsuneTTOptions> KitsuneTTOptions::createFromCommandLine(char optLevel) {
  return createFromCommandLine(createOptznLevelFrom(optLevel));
}

std::optional<KitsuneTTOptions> KitsuneTTOptions::create(const KitsuneOptions &kitOpts,
                                           OptznLevel optznLevel,
                                           FPOpFusionMode fpOpFusionMode) {
  if (!kitOpts.getTTID())
    return std::nullopt;

  KitsuneTTOptions tto(*kitOpts.getTTID());

  // Set common tapir target options.
  tto.tapirVerbose = kitOpts.getTapirVerbose();
  tto.kitrtVerbose = kitOpts.getTapirVerbose() or kitOpts.getKitrtVerbose();
  tto.fpOpFusionMode = fpOpFusionMode;
  tto.optLevel = optznLevel;
  tto.lld = kitOpts.getLLD();

  // Set tapir target options shared by GPU-centric tapir targets.
  tto.fixedThreadsPerBlock = kitOpts.getFixedThreadsPerBlock();
  tto.maxThreadsPerBlock = kitOpts.getMaxThreadsPerBlock();
  tto.gpuPrefetch = kitOpts.getGPUPrefetch();

  // Set cuda tapir target options.
  tto.cudaArch = kitOpts.getCudaArch();
  tto.cudaVirtArch = kitOpts.getCudaVirtArch();
  tto.cudaTargetFeatures = kitOpts.getCudaFeatures();
  tto.cudaRuntimeBCFile = kitOpts.getCudaRuntimeBCFile();

  // // Set 'custom' tapir target options.
  // if (kitOpts.getTTPlugin().size()) {
  //   if (Expected<TTPlugin> ttPlugin = TTPlugin::load(kitOpts.getTTPlugin()))
  //     tto.ttPlugin = *ttPlugin;
  //   else
  //     llvm_unreachable("Tapir target plugin load failure not caught earlier");
  // }

  // Set hip tapir target options.
  tto.hipArch = kitOpts.getHipArch();
  tto.hipSRAMECC = kitOpts.getHipSRAMECC();
  tto.hipXnack = kitOpts.getHipXnack();
  tto.hipTargetFeatures = kitOpts.getHipFeatures();
  tto.hipRuntimeBCFiles = kitOpts.getHipRuntimeBCFiles();

  // // Set opencilk tapir target options.
  // tto.openCilkRuntimeBCFile = kitOpts.getOpenCilkRuntimeBCFile();

  return tto;
}

void KitsuneTTOptions::print(raw_ostream &os, bool all) const {
  os << "Tapir target options:\n";
  os << "  Primary:                 " << tt << "\n";
  os << "  Compiler verbose:        " << getTapirVerbose() << "\n";
  os << "  Runtime verbose:         " << getKitrtVerbose() << "\n";
  os << "  Optimization level:      " << getOptznLevel() << "\n";
  os << "  FP fusion:               " << getFPOpFusionMode() << "\n";
  if (all || tt == TTID::Cuda || tt == TTID::Hip) {
    os << "  GPU fixed threads/block: " << getFixedThreadsPerBlock() << "\n";
    os << "  GPU max threads/block:   " << getMaxThreadsPerBlock() << "\n";
    os << "  GPU prefetch:            " << getGPUPrefetch() << "\n";
  }
  if (all || tt == TTID::Cuda) {
    os << "  Cuda arch:               " << getCudaArch() << "\n";
    os << "  Cuda virtual arch:       " << getCudaVirtArch() << "\n";
    os << "  Cuda target features:    " << getCudaTargetFeatures() << "\n";
    os << "  Cuda bitcode file:       " << getCudaRuntimeBCFile() << "\n";
  }
  // if (all || tt == TTID::Custom) {
  //   if (std::optional<TTPlugin> plugin = getTTPlugin()) {
  //     StringRef name = plugin->getName();
  //     StringRef version = plugin->getVersion();
  //     os << "  Custom plugin:           " << name << " " << version << "\n";
  //     os << "  Custom plugin file:      " << plugin->getFile() << "\n";
  //   }
  // }
  if (all || tt == TTID::Hip) {
    os << "  Hip arch:                " << getHipArch() << "\n";
    os << "  Hip sramecc:             " << getHipSRAMECC() << "\n";
    os << "  Hip xnack:               " << getHipXnack() << "\n";
    os << "  Hip target features:     " << getHipTargetFeatures() << "\n";
    os << "  Hip bitcode files: [\n";
    for (StringRef file : getHipRuntimeBCFiles())
      os << "    " << file << "\n";
    os << "  ]";
    os << "  LLD:                     " << getLLD() << "\n";
  }
  // if (all || tt == TTID::OpenCilk) {
  //   os << "  Opencilk bitcode file:   " << getOpenCilkRuntimeBCFile() << "\n";
  // }
}
