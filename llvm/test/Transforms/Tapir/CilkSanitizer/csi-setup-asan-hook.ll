; Check that CSI-setup ignores __asan hooks when promoting calls to invokes.
;
; ASan should have inserted these hooks into tasks with proper attributes or
; control flow for exceptional returns, but it does not do so at this time.
; As a workaround, CSI will ignore these hooks when setting up a function for
; instrumentation.
;
; RUN: opt < %s -passes="csi-setup" -S | FileCheck %s

target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define void @_Z16reduce_with_cilkPP6ScalarS1_S0_S0_mm() personality ptr null {
entry:
  %syncreg = tail call token @llvm.syncregion.start()
  unreachable

pfor.cond.preheader:                              ; No predecessors!
  detach within %syncreg, label %pfor.body.entry, label %pfor.inc241 unwind label %lpad240.loopexit

pfor.body.entry:                                  ; preds = %pfor.cond.preheader
  %syncreg14.strpm.detachloop = call token @llvm.syncregion.start()
  %syncreg14 = call token @llvm.syncregion.start()
  br label %invoke.cont222

pfor.cond24.preheader.new:                        ; No predecessors!
  detach within %syncreg14.strpm.detachloop, label %invoke.cont174.strpm.outer, label %pfor.inc.strpm.outer

invoke.cont174.strpm.outer:                       ; preds = %pfor.cond24.preheader.new
  unreachable

pfor.inc.strpm.outer:                             ; preds = %pfor.cond24.preheader.new
  sync within %syncreg14.strpm.detachloop, label %invoke.cont222

invoke.cont222:                                   ; preds = %pfor.inc.strpm.outer, %pfor.body.entry
  call void @__asan_report(i64 0)
  unreachable

; CHECK: invoke.cont222:
; CHECK-NOT: invoke {{.*}}void @__asan_report(
; CHECK-NEXT: call void @__asan_report(
; CHECK-NEXT: unreachable

pfor.inc241:                                      ; preds = %pfor.cond.preheader
  sync within %syncreg, label %sync.continue246

lpad240.loopexit:                                 ; preds = %pfor.cond.preheader
  %lpad.loopexit = landingpad { ptr, i32 }
          cleanup
  resume { ptr, i32 } zeroinitializer

sync.continue246:                                 ; preds = %pfor.inc241
  unreachable
}

declare void @__asan_report(i64) local_unnamed_addr

; Function Attrs: nounwind willreturn memory(argmem: readwrite)
declare token @llvm.syncregion.start() #0

; uselistorder directives
uselistorder ptr null, { 1, 2, 0 }
uselistorder ptr @llvm.syncregion.start, { 2, 1, 0 }

attributes #0 = { nounwind willreturn memory(argmem: readwrite) }
