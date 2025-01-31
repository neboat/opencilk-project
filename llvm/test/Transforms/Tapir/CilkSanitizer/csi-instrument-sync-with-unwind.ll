; Check that CSI does not insert instrumentation between a sync and its corresponding sync.unwind.
;
; RUN: opt < %s -passes="csi-setup,csi" -csi-instrument-basic-blocks=false -S | FileCheck %s --check-prefixes=CHECK
; RUN: opt < %s -passes="csi-setup,csi" -S | FileCheck %s --check-prefixes=CHECK,CHECK-BB
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: nounwind willreturn memory(argmem: readwrite)
declare token @llvm.syncregion.start() #0

; Function Attrs: nounwind willreturn memory(argmem: readwrite)
declare token @llvm.taskframe.create() #0

; Function Attrs: willreturn memory(argmem: readwrite)
declare void @llvm.sync.unwind(token) #1

; Function Attrs: nounwind willreturn memory(argmem: readwrite)
declare void @llvm.taskframe.end(token) #0

define fastcc void @_Z28prove_sumcheck_cubic_batchedR16ProverTranscriptRKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE14GoldilockField8FixedVecIP9DensePolyESD_RSB_SD_SD_SD_St4spanIKS9_Lm18446744073709551615EE.outline_pfor.cond522.ls2() personality ptr null {
pfor.cond522.preheader.ls2:
  %syncreg529.ls2 = tail call token @llvm.syncregion.start()
  br label %pfor.body.entry525.tf.tf.tf.tf.tf.tf.tf.tf.ls2

pfor.body.entry525.tf.tf.tf.tf.tf.tf.tf.tf.ls2:   ; preds = %sync.continue578.ls2, %pfor.cond522.preheader.ls2
  %0 = tail call token @llvm.taskframe.create()
  detach within %syncreg529.ls2, label %det.achd554.ls2, label %det.cont569.ls2

det.cont569.ls2:                                  ; preds = %det.achd554.ls2, %pfor.body.entry525.tf.tf.tf.tf.tf.tf.tf.tf.ls2
  sync within %syncreg529.ls2, label %sync.continue578.ls2

sync.continue578.ls2:                             ; preds = %det.cont569.ls2
  tail call void @llvm.sync.unwind(token %syncreg529.ls2) #2
  tail call void @llvm.taskframe.end(token %0)
  br i1 false, label %pfor.cond.cleanup599.ls2.tfend, label %pfor.body.entry525.tf.tf.tf.tf.tf.tf.tf.tf.ls2

det.achd554.ls2:                                  ; preds = %pfor.body.entry525.tf.tf.tf.tf.tf.tf.tf.tf.ls2
  reattach within %syncreg529.ls2, label %det.cont569.ls2

pfor.cond.cleanup599.ls2.tfend:                   ; preds = %sync.continue578.ls2
  ret void
}

; CHECK: define {{.*}}void @_Z28prove_sumcheck_cubic_batchedR16ProverTranscriptRKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE14GoldilockField8FixedVecIP9DensePolyESD_RSB_SD_SD_SD_St4spanIKS9_Lm18446744073709551615EE.outline_pfor.cond522.ls2()

; CHECK: %syncreg529.ls2 = {{.*}}call token @llvm.syncregion.start()
; CHECK: %[[TF:.+]] = {{.*}}call token @llvm.taskframe.create()

; CHECK: sync within %syncreg529.ls2, label %[[SYNC_CONT:.+]]

; CHECK: [[SYNC_CONT]]:
; CHECK-NOT: call void @__csi_
; CHECK-NEXT: void @llvm.sync.unwind(token %syncreg529.ls2
; CHECK: call void @__csi_after_sync(
; CHECK-BB-NOT: @__csi_bb_
; CHECK: call void @llvm.taskframe.end(token %[[TF]])

; CHECK-BB: call void @__csi_bb_entry(

; CHECK: call void @__csi_loopbody_exit(

; CHECK: reattach within %syncreg529.ls2

; uselistorder directives
uselistorder ptr null, { 1, 2, 0 }

attributes #0 = { nounwind willreturn memory(argmem: readwrite) }
attributes #1 = { willreturn memory(argmem: readwrite) }
attributes #2 = { nounwind }
