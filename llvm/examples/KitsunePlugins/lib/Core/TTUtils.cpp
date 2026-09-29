//===- TTUtils.cpp - Utilities closely related to tapir targets -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Miscellaneous utilities that are closely related to tapir targets in some
// way. These are intended to be for utilities that are primarily, if not
// exclusively, used in code that lives in kitsune/.
//
//===----------------------------------------------------------------------===//

#include "kitsune/Core/TTUtils.h"
#include "kitsune/Core/KitsuneTTOptions.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Support/SourceMgr.h"

using namespace llvm;

static Expected<OwnedModule> parseLLVMFile(StringRef file, LLVMContext &ctx) {
  SMDiagnostic diag;
  if (OwnedModule m = parseIRFile(file, diag, ctx))
    return m;
  return createStringError(
      join_items("", "failed to parse LLVM file: ", diag.getMessage()));
}

static Expected<OwnedModule> getLibDeviceModuleCuda(const KitsuneTTOptions &tto,
                                                    LLVMContext &ctx) {
  return parseLLVMFile(tto.getCudaRuntimeBCFile(), ctx);
}

static Expected<OwnedModule> getLibDeviceModuleHip(const KitsuneTTOptions &tto,
                                                   LLVMContext &ctx) {
  const std::vector<std::string> &bcFiles = tto.getHipRuntimeBCFiles();
  assert(tto.getHipRuntimeBCFiles().size() &&
         "At least one bitcode file is required by Hip's libDevice module");

  Expected<OwnedModule> libDeviceM = parseLLVMFile(bcFiles[0], ctx);
  if (!libDeviceM)
    return libDeviceM.takeError();
  Linker linker(**libDeviceM);
  for (size_t i = 1; i < bcFiles.size(); ++i) {
    StringRef bcFile = bcFiles[i];
    Expected<OwnedModule> m = parseLLVMFile(bcFile, ctx);
    if (!m)
      return m;
    if (linker.linkInModule(std::move(*m), Linker::OverrideFromSrc))
      return createStringError(
          join_items("", "Error linking device bitcode file: ", bcFile));
  }
  return libDeviceM;
}

// static Expected<OwnedModule> getRuntimeModuleOpenCilk(const KitsuneTTOptions &tto,
//                                                       LLVMContext &ctx) {
//   return parseLLVMFile(tto.getOpenCilkRuntimeBCFile(), ctx);
// }

Expected<OwnedModule> llvm::getSupportModule(TTID tt, const KitsuneTTOptions &tto,
                                             LLVMContext &ctx) {
  switch (tt) {
  case TTID::Cuda:
    return getLibDeviceModuleCuda(tto, ctx);
  case TTID::Hip:
    return getLibDeviceModuleHip(tto, ctx);
  // case TTID::OpenCilk:
  //   return getRuntimeModuleOpenCilk(tto, ctx);
  default:
    llvm_unreachable("getSupportModule: TTID not handled");
  }
}

Expected<OwnedModule> llvm::getLibDeviceModule(TTID tt, const KitsuneTTOptions &tto,
                                               LLVMContext &ctx) {
  switch (tt) {
  case TTID::Cuda:
  case TTID::Hip:
    return getSupportModule(tt, tto, ctx);
  default:
    llvm_unreachable("getLibDeviceModule: TTID not handled");
  }
}
