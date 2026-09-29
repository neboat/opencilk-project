//===- ModuleUtils.cpp - Utilities for LLVM modules -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Definitions of utilities for LLVM Module.
//
//===----------------------------------------------------------------------===//

#include "kitsune/Core/ModuleUtils.h"
#include "kitsune/Core/ConstantUtils.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/ToolOutputFile.h"

#include <set>

using namespace llvm;

/// Name of a named metadata node Kitsune metadata for device modules.
static constexpr StringRef mdDeviceModuleFlags = "kitsune.device.module.flags";

bool llvm::hasDeviceModuleMetadata(const Module &m) {
  return m.getNamedMetadata(mdDeviceModuleFlags);
}

NamedMDNode &llvm::addDeviceModuleMetadata(TTID tt, Module &m) {
  auto addOperandAt = [](NamedMDNode &nmd, unsigned i, MDNode *md) -> void {
    if (nmd.getNumOperands() > i)
      nmd.setOperand(i, md);
    else
      nmd.addOperand(md);
  };

  LLVMContext &ctx = m.getContext();
  Type *i32 = Type::getInt32Ty(ctx);
  NamedMDNode *nmd = m.getOrInsertNamedMetadata(mdDeviceModuleFlags);

  Constant *cTT = ConstantInt::get(i32, int(tt));

  addOperandAt(*nmd, 0, MDNode::get(ctx, ConstantAsMetadata::get(cTT)));
  addOperandAt(*nmd, 1, MDNode::get(ctx, MDString::get(ctx, m.getName())));

  return *nmd;
}

NamedMDNode &llvm::cloneModuleFlagsMetadataInto(const Module &hostM,
                                                Module &devM) {
  // These are the module flags that should be cloned over. Others will be
  // ignored.
  std::set<StringRef> flags = {"Debug Info Version", "Dwarf Version",
                               "PIC Level", "PIE Level", "wchar_size"};

  NamedMDNode &nmd = *devM.getOrInsertModuleFlagsMetadata();
  if (const NamedMDNode *hostMD = hostM.getModuleFlagsMetadata())
    for (const MDNode *md : hostMD->operands())
      if (md->getNumOperands() > 1)
        if (auto *mdString = dyn_cast<MDString>(md->getOperand(1)))
          if (flags.find(mdString->getString()) != flags.end())
            nmd.addOperand(MDNode::replaceWithPermanent(md->clone()));
  return nmd;
}

NamedMDNode &llvm::cloneIdentMetadataInto(const Module &hostM, Module &devM) {
  NamedMDNode &nmd = *devM.getOrInsertNamedMetadata("llvm.ident");
  if (const NamedMDNode *ident = hostM.getNamedMetadata("llvm.ident"))
    for (const MDNode *md : ident->operands())
      nmd.addOperand(MDNode::replaceWithPermanent(md->clone()));
  return nmd;
}

std::optional<TTID> llvm::getTTIDFromDeviceModuleMetadata(const Module &m) {
  if (const NamedMDNode *nmd = m.getNamedMetadata(mdDeviceModuleFlags))
    if (const MDNode *md = nmd->getOperand(0))
      if (const auto *cmd = dyn_cast<ConstantAsMetadata>(md->getOperand(0)))
        if (const auto *cint = dyn_cast<ConstantInt>(cmd->getValue()))
          return createTTIDFrom(*cint);
  return std::nullopt;
}

std::optional<StringRef>
llvm::getNameFromDeviceModuleMetadata(const Module &m) {
  if (const NamedMDNode *nmd = m.getNamedMetadata(mdDeviceModuleFlags))
    if (const MDNode *md = nmd->getOperand(1))
      if (const auto *mds = dyn_cast<MDString>(md->getOperand(0)))
        return mds->getString();
  return std::nullopt;
}

/// @brief Wite the given module to a file as readable IR.
/// @param M - the module to save.
/// @param Filename - optional file name (empty string uses module name).
void llvm::saveModuleToFile(const Module *M, const std::string &FileName,
                            const std::string &Extension) {
  std::error_code EC;
  SmallString<256> IRFileName;
  if (FileName.empty())
    IRFileName = Twine(sys::path::filename(M->getName())).str() + Extension;
  else
    IRFileName = Twine(FileName).str() + Extension;

  std::unique_ptr<ToolOutputFile> IRFile = std::make_unique<ToolOutputFile>(
      IRFileName, EC, sys::fs::OpenFlags::OF_None);
  if (not EC) {
    M->print(IRFile->os(), nullptr);
    IRFile->keep();
  } else
    errs() << "warning: unable to save module '" << IRFileName.c_str() << "'\n";
}