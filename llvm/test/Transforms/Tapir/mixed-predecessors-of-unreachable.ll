; Check that loop simplification does not split placeholder successors of
; detached.rethrows when those unreachable blocks have other predecessors.
;
; RUN: opt < %s -passes="cilksan" -S | FileCheck %s

target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128-Fn32"
target triple = "arm64-apple-macosx13.0.0"

; Function Attrs: nounwind willreturn memory(argmem: readwrite)
declare token @llvm.syncregion.start() #0

; Function Attrs: willreturn memory(argmem: readwrite)
declare void @llvm.detached.rethrow.sl_p0i32s(token, { ptr, i32 }) #1

; Function Attrs: sanitize_cilk
define void @_ZN9LAMMPS_NS9StencilMD31SORT_LOCAL_ATOMS_ZOID_MANY_CUTSEv() #2 personality ptr null {
entry:
  %syncreg = call token @llvm.syncregion.start()
  br label %pfor.detach

pfor.detach:                                      ; preds = %pfor.detach, %entry
  detach within %syncreg, label %pfor.body.entry, label %pfor.detach unwind label %lpad635

pfor.body.entry:                                  ; preds = %pfor.detach
  %syncreg51 = call token @llvm.syncregion.start()
  br label %pfor.detach62

pfor.detach62:                                    ; preds = %pfor.detach62, %pfor.body.entry
  detach within %syncreg51, label %pfor.body.entry64, label %pfor.detach62 unwind label %lpad109

pfor.body.entry64:                                ; preds = %pfor.detach62
  br label %for.cond

for.cond:                                         ; preds = %for.cond, %pfor.body.entry64
  br label %for.cond

lpad109:                                          ; preds = %pfor.detach62
  %0 = landingpad { ptr, i32 }
          cleanup
  invoke void @llvm.detached.rethrow.sl_p0i32s(token %syncreg, { ptr, i32 } zeroinitializer)
          to label %unreachable unwind label %lpad635

; CHECK: lpad109:
; CHECK-NEXT: landingpad
; CHECK-NEXT: cleanup
; CHECK-NEXT: invoke void @llvm.detached.rethrow.sl_p0i32s(token %syncreg,
; CHECK-NEXT: to label %unreachable unwind label %lpad635

pfor.body.entry140:                               ; No predecessors!
  %syncreg143 = call token @llvm.syncregion.start()
  br label %pfor.detach157

pfor.detach157:                                   ; preds = %pfor.preattach289, %pfor.detach157, %pfor.body.entry140
  detach within %syncreg143, label %pfor.body.entry159, label %pfor.detach157 unwind label %lpad295

pfor.body.entry159:                               ; preds = %pfor.detach157
  switch i32 0, label %unreachable [
    i32 0, label %pfor.preattach289
    i32 1, label %pfor.preattach289
  ]

pfor.preattach289:                                ; preds = %pfor.body.entry159, %pfor.body.entry159
  reattach within %syncreg143, label %pfor.detach157

lpad295:                                          ; preds = %pfor.detach157
  %1 = landingpad { ptr, i32 }
          cleanup
  invoke void @llvm.detached.rethrow.sl_p0i32s(token none, { ptr, i32 } zeroinitializer)
          to label %unreachable unwind label %lpad635

; CHECK: lpad295:
; CHECK-NEXT: landingpad
; CHECK-NEXT: cleanup
; CHECK-NEXT: invoke void @llvm.detached.rethrow.sl_p0i32s(token none,
; CHECK-NEXT: to label %unreachable unwind label %lpad635

lpad635:                                          ; preds = %lpad295, %lpad109, %pfor.detach
  %2 = landingpad { ptr, i32 }
          cleanup
  resume { ptr, i32 } zeroinitializer

unreachable:                                      ; preds = %lpad295, %pfor.body.entry159, %lpad109
  unreachable
}

; uselistorder directives
uselistorder ptr null, { 1, 2, 0 }

attributes #0 = { nounwind willreturn memory(argmem: readwrite) }
attributes #1 = { willreturn memory(argmem: readwrite) }
attributes #2 = { sanitize_cilk }
