; Check that loop-simplifycfg properly removes dead loops from constant-folding
; terminators within a Tapir loop.
;
; RUN: opt < %s -passes="function<eager-inv>(loop-mssa(loop-simplifycfg))" -S | FileCheck %s 
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define void @_Z9impl_zeraRKNSt10filesystem7__cxx114pathES3_S3_() personality ptr null {
entry:
  %syncreg139 = tail call token @llvm.syncregion.start()
  sync within %syncreg139, label %pfor.cond202

pfor.cond202:                                     ; preds = %invoke.cont407, %pfor.cond202, %entry
  detach within %syncreg139, label %pfor.body.entry205, label %pfor.cond202 unwind label %lpad485.loopexit

pfor.body.entry205:                               ; preds = %pfor.cond202
  br i1 false, label %pfor.body.entry205.pfor.body.entry329.epil_crit_edge, label %invoke.cont407

pfor.body.entry205.pfor.body.entry329.epil_crit_edge: ; preds = %pfor.body.entry205
  br label %pfor.body.entry329.epil

pfor.body.entry329.epil:                          ; preds = %pfor.body.entry205.pfor.body.entry329.epil_crit_edge, %pfor.body.entry329.epil
  br label %pfor.body.entry329.epil

invoke.cont407:                                   ; preds = %pfor.body.entry205
  reattach within %syncreg139, label %pfor.cond202

lpad485.loopexit:                                 ; preds = %pfor.cond202
  %lpad.loopexit = landingpad { ptr, i32 }
          cleanup
  ret void

; uselistorder directives
  uselistorder label %pfor.body.entry329.epil, { 1, 0 }
}

; CHECK: pfor.cond202:
; CHECK-NEXT: detach within %syncreg139, label %pfor.body.entry205, label %pfor.cond202.backedge unwind label %lpad485.loopexit

; CHECK: pfor.cond202.backedge:
; CHECK-NEXT: br label %pfor.cond202
 
; CHECK: pfor.body.entry205:
; CHECK-NEXT: reattach within %syncreg139, label %pfor.cond202.backedge

; Function Attrs: nounwind willreturn memory(argmem: readwrite)
declare token @llvm.syncregion.start() #0

; uselistorder directives
uselistorder ptr null, { 1, 2, 0 }

attributes #0 = { nounwind willreturn memory(argmem: readwrite) }
