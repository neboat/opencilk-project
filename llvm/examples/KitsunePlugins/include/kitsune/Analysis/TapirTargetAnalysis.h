//===- TapirTargetAnalysis.h - Analysis pass for tapir targets --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// An interface for information about the tapir targets needed by a module and
// options to control the behavior of the tapir targets.
//
//===----------------------------------------------------------------------===//

#ifndef KITSUNE_ANALYSIS_TAPIR_TARGET_ANALYSIS_H
#define KITSUNE_ANALYSIS_TAPIR_TARGET_ANALYSIS_H

#include "kitsune/Core/KitsuneTTOptions.h"
#include "kitsune/Core/Tapir.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Tapir/LoweringUtils.h"

#include <map>
#include <optional>
#include <vector>

namespace llvm {

class LoopInfo;
class TapirTarget;
class TapirTargetAnalysis;
class TapirTargetAnalysisWrapperPass;
class TaskInfo;

/// An object that contains information about the tapir targets that are
/// enabled.
class TapirTargetInfo {
public:
  using GetLoopInfo = std::function<LoopInfo &(Function &)>;
  using GetTaskInfo = std::function<TaskInfo &(Function &)>;

private:
  /// Options for the primary tapir target.
  std::optional<KitsuneTTOptions> ttOpts;

  /// The tapir targets used by each function in the module.
  std::map<Function *, std::vector<TTID>> ttsInFunc;

  /// The tapir targets needed by the functions in the module.
  std::vector<TTID> ttsInModule;

  /// The TapirTarget objects needed by the module. These will be created
  /// when \ref computeRequiredTTs is run. If computeRequiredTTs is run more
  /// than once, a new TapirTarget object will only be created if one does not
  /// already exist in this map. The actual TapirTarget objects are owned by the
  /// TapirTargetAnalysis object.
  std::map<TTID, TapirTarget *> tts;

private:
  TapirTargetInfo(std::optional<KitsuneTTOptions> ttOpts);

  /// Compute the tapir target objects required by every function in a module.
  void computeRequiredTTs(Module &m, GetLoopInfo getLoopInfo,
                          GetTaskInfo getTaskInfo);

  /// Add a TapirTarget object for the given ID. The object will be owned by
  /// the TapirTargetAnalysis object.
  void addTT(TTID id, TapirTarget *tt);

public:
  bool hasTTID() const { return ttOpts.has_value(); }

  /// Get the primary tapir target ID if the tapir target options have been set.
  std::optional<TTID> getTTIDOrNull() const;

  /// Get the primary tapir target ID. This should only be called when the tapir
  /// target options are guaranteed to have been set.
  TTID getTTID() const;

  /// Check if a TapirTarget exists for the given ID.
  bool hasTT(TTID id) const;

  /// Get the TapirTarget for the given ID. The id is assumed to exist.
  TapirTarget *getTT(TTID id) const;

  /// Get the tapir target options. This should only be called when the tapir
  /// target options are guaranteed to have been set.
  const KitsuneTTOptions &getOptions() const;

  /// Get the tapir target ID's required by a function. This will be an empty
  /// vector if there are no tapir loops in the function. If there is at least
  /// one tapir loop in the function, this will contain the tapir target ID's
  /// that appear as loop hints on the loop (this is the case for attributed
  /// forall loops) and the primary tapir target ID if there is at least one
  /// tapir loop in the function which does not have a target loop hint.
  ArrayRef<TTID> getRequiredTTs(Function &f) const;

  /// Get the tapir target ID's required by the module. This will be an empty
  /// vector if there are no tapir loops in the module, even if a primary tapir
  /// target has been set by the frontend.
  ArrayRef<TTID> getRequiredTTs(Module &m) const;

  bool invalidate(Module &, const PreservedAnalyses &pa,
                  ModuleAnalysisManager::Invalidator &);

  friend class TapirTargetAnalysis;
  friend class TapirTargetAnalysisWrapperPass;
};

/// Analysis pass to provide information about the "global" tapir targets.
/// Individual loops and other constructs may require other tapir targets.
/// Eventually, this should probably contain information about all the tapir
/// targets that are needed by a given module, but that has not yet been
/// implemented.
class TapirTargetAnalysis : public AnalysisInfoMixin<TapirTargetAnalysis> {
private:
  /// The TapirTargetInfo that will be populated when @ref run() is called.
  /// A copy of this will be returned whenever the analysis is retrieved.
  TapirTargetInfo ttInfo;

  /// The tapir targets needed by the module.
  std::map<TTID, std::unique_ptr<TapirTarget>> tts;

public:
  using Result = TapirTargetInfo;

  /// Construct an analysis pass with an optional KitsuneTTOptions object.
  /// This may be std::nullopt if the frontend creating this pass has not been
  /// given a tapir target to use.
  TapirTargetAnalysis(std::optional<KitsuneTTOptions> ttOpts);

  Result run(Module &m, ModuleAnalysisManager &mam);

private:
  static AnalysisKey Key;

  friend AnalysisInfoMixin<TapirTargetAnalysis>;
};

/// Legacy wrapper pass to provide the tapir target analysis results. This is
/// provided since some transformations are run as part of code generation
/// which still uses the legacy pass manager. This is an immutable pass because
/// only function passes are allowed during the code generation phase. This
/// means that the results returned by this pass will be slightly different from
/// those returned by the new pass manager. Specifically, the required tapir
/// targets will never be computed by this pass. This is ok since during
/// codegen, we should never need anything other than the tapir target options.
class TapirTargetAnalysisWrapperPass : public ImmutablePass {
private:
  /// The TapirTargetInfo that will be populated when @ref run() is called.
  /// A copy of this will be returned whenever the analysis is retrieved.
  TapirTargetInfo ttInfo;

public:
  using Result = TapirTargetInfo;

public:
  /// Create a default constructor because it is needed by the legacy pass
  /// manager, but this should never be used anywhere else.
  TapirTargetAnalysisWrapperPass();

  /// Construct an analysis pass with an optional KitsuneTTOptions object.
  /// This may be std::nullopt if the frontend creating this pass has not been
  /// given a tapir target to use.
  TapirTargetAnalysisWrapperPass(std::optional<KitsuneTTOptions> ttOpts);

  void getAnalysisUsage(AnalysisUsage &au) const override;

  Result getResult() const;

public:
  static char ID;
};

ModulePass *
createTapirTargetAnalysisWrapperPass(std::optional<KitsuneTTOptions> ttOpts);

} // namespace llvm

#endif // KITSUNE_ANALYSIS_TAPIR_TARGET_ANALYSIS_H
