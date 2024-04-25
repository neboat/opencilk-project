//===- WorkSpanAnalysis.cpp - Analysis to estimate work and span ----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements an analysis pass to estimate the work and span of the
// program.
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/WorkSpanAnalysis.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Analysis/CodeMetrics.h"
#include "llvm/Analysis/InlineCost.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Analysis/ProfileSummaryInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/BlockFrequency.h"
#include "llvm/Support/InstructionCost.h"

using namespace llvm;

#define DEBUG_TYPE "work-span"

// Get a constant trip count for the given loop.
unsigned llvm::getConstTripCount(const Loop *L, ScalarEvolution &SE) {
  int64_t ConstTripCount = 0;
  // If there are multiple exiting blocks but one of them is the latch, use
  // the latch for the trip count estimation. Otherwise insist on a single
  // exiting block for the trip count estimation.
  BasicBlock *ExitingBlock = L->getLoopLatch();
  if (!ExitingBlock || !L->isLoopExiting(ExitingBlock))
    ExitingBlock = L->getExitingBlock();
  if (ExitingBlock)
    ConstTripCount = SE.getSmallConstantTripCount(L, ExitingBlock);
  return ConstTripCount;
}

/// Recursive helper routine to estimate the amount of work in a loop.
static void estimateLoopCostHelper(const Loop *L, CodeMetrics &Metrics,
                                   WSCost &LoopCost, LoopInfo *LI,
                                   ScalarEvolution *SE, BlockFrequencyInfo *BFI,
                                   OptimizationRemarkEmitter *ORE) {
  if (LoopCost.UnknownCost)
    return;

  BlockFrequency LoopEntryFreq =
      BFI ? BFI->getBlockFreq(L->getHeader()) : BlockFrequency();

  for (Loop *SubL : *L) {
    WSCost SubLoopCost;
    BlockFrequency SubloopEntryFreq =
        BFI ? BFI->getBlockFreq(SubL->getHeader()) : BlockFrequency();

    estimateLoopCostHelper(SubL, Metrics, SubLoopCost, LI, SE, BFI, ORE);
    if (LoopEntryFreq.getFrequency() && SubloopEntryFreq.getFrequency() &&
        SubloopEntryFreq < LoopEntryFreq)
      SubLoopCost.Work /=
          (LoopEntryFreq.getFrequency() / SubloopEntryFreq.getFrequency());
    // Quit early if the size of this subloop is already too big.
    if (InstructionCost::getMax() == SubLoopCost.Work)
      LoopCost.Work = InstructionCost::getMax();

    // Find a constant trip count if available
    int64_t ConstTripCount = SE ? getConstTripCount(SubL, *SE) : 0;
    // TODO: Use a more precise analysis to account for non-constant trip
    // counts.
    if (!ConstTripCount) {
      if (ORE)
        ORE->emit([&]() {
          return OptimizationRemark("work-span-analysis", "NoConstTripCount",
                                    SubL->getStartLoc(), SubL->getHeader())
                 << "Could not determine constant trip count for subloop.";
        });
      if (BFI && SubloopEntryFreq.getFrequency() &&
          LoopEntryFreq.getFrequency()) {
        ConstTripCount =
            SubloopEntryFreq.getFrequency() / LoopEntryFreq.getFrequency();
        ConstTripCount |= (ConstTripCount == 0);
      } else {
        // If we cannot compute a constant trip count, assume this subloop
        // executes at least once.
        LoopCost.UnknownCost = true;
        ConstTripCount = 1;
      }
    } else if (BFI && SubloopEntryFreq.getFrequency() &&
               LoopEntryFreq.getFrequency()) {
      LLVM_DEBUG(dbgs() << "ConstTripCount " << ConstTripCount
                        << ", BFI estimate "
                        << SubloopEntryFreq.getFrequency() /
                               LoopEntryFreq.getFrequency()
                        << "\n");
    }

    // Check if this subloop suffices to make loop L huge.
    if (InstructionCost::getMax() - LoopCost.Work <
        (SubLoopCost.Work * ConstTripCount)) {
      if (ORE)
        ORE->emit([&]() {
          return OptimizationRemark("work-span-analysis", "LargeSubloop",
                                    SubL->getStartLoc(), SubL->getHeader())
                 << "Subloop work makes this loop huge.";
        });
      LoopCost.Work = InstructionCost::getMax();
    }

    if (LoopCost.Work < InstructionCost::getMax())
      // Add in the size of this subloop.
      LoopCost.Work += (SubLoopCost.Work * ConstTripCount);
  }

  // After looking at all subloops, if we've concluded we have a huge loop size,
  // return early.
  if (InstructionCost::getMax() == LoopCost.Work)
    return;

  for (BasicBlock *BB : L->blocks()) {
    if (LI->getLoopFor(BB) == L) {
      InstructionCost BBCost = Metrics.NumBBInsts[BB];
      BlockFrequency BBFreq = BFI ? BFI->getBlockFreq(BB) : BlockFrequency();
      if (LoopEntryFreq.getFrequency() && BBFreq.getFrequency() &&
          BBFreq < LoopEntryFreq) {
        BBCost /= (LoopEntryFreq.getFrequency() / BBFreq.getFrequency());
      }
      // Check if this BB suffices to make loop L huge.
      if (InstructionCost::getMax() - LoopCost.Work < BBCost) {
        LoopCost.Work = InstructionCost::getMax();
        return;
      }
      LoopCost.Work += BBCost;
    }
  }
}

void llvm::estimateLoopCost(WSCost &LoopCost, const Loop *L, LoopInfo *LI,
                            ScalarEvolution *SE, const TargetTransformInfo &TTI,
                            TargetLibraryInfo *TLI, BlockFrequencyInfo *BFI,
                            const SmallPtrSetImpl<const Value *> &EphValues,
                            OptimizationRemarkEmitter *ORE) {
  // TODO: Use more precise analysis to estimate the work in each call.
  // TODO: Use vectorizability to enhance cost analysis.

  // Gather code metrics for all basic blocks in the loop.
  for (BasicBlock *BB : L->blocks())
    LoopCost.Metrics.analyzeBasicBlock(BB, TTI, EphValues,
                                       /*PrepareForLTO*/ false, TLI);

  estimateLoopCostHelper(L, LoopCost.Metrics, LoopCost, LI, SE, BFI, ORE);
}
