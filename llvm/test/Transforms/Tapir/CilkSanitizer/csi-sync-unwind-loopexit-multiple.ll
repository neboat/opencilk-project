; Check that CSI loop instrumentation instruments around sync-unwind loop exits properly.
;
; RUN: opt < %s -passes="csi-setup,csi" -S | FileCheck %s
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

$__clang_call_terminate = comdat any

; Function Attrs: mustprogress uwtable
define dso_local void @_Z3fooi(i32 noundef %n) local_unnamed_addr #0 personality ptr @__gxx_personality_v0 {
entry:
  %syncreg = tail call token @llvm.syncregion.start()
  %cmp = icmp sgt i32 %n, 0
  br i1 %cmp, label %pfor.cond, label %try.cont

pfor.cond:                                        ; preds = %entry, %pfor.inc
  %__begin.0 = phi i32 [ %inc, %pfor.inc ], [ 0, %entry ]
  detach within %syncreg, label %pfor.body.entry, label %pfor.inc unwind label %lpad69.loopexit

pfor.body.entry:                                  ; preds = %pfor.cond
  %w = alloca i32, align 4
  %syncreg2 = tail call token @llvm.syncregion.start()
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %w)
  %and = and i32 %__begin.0, 1
  %tobool.not = icmp eq i32 %and, 0
  %0 = tail call token @llvm.taskframe.create()
  br i1 %tobool.not, label %if.else.tf.tf.tf.tf, label %if.then.tf.tf.tf.tf

if.then.tf.tf.tf.tf:                              ; preds = %pfor.body.entry
  detach within %syncreg2, label %det.achd, label %det.cont unwind label %lpad4

det.achd:                                         ; preds = %if.then.tf.tf.tf.tf
  %call = invoke noundef i32 @_Z3bari(i32 noundef %__begin.0)
          to label %invoke.cont unwind label %lpad

invoke.cont:                                      ; preds = %det.achd
  store i32 %call, ptr %w, align 4, !tbaa !5
  reattach within %syncreg2, label %det.cont

det.cont:                                         ; preds = %if.then.tf.tf.tf.tf, %invoke.cont
  %add14 = add nuw nsw i32 %__begin.0, 1
  %call16 = invoke noundef i32 @_Z3bari(i32 noundef %add14)
          to label %invoke.cont15 unwind label %lpad11.tfsplit.split-lp

invoke.cont15:                                    ; preds = %det.cont
  sync within %syncreg2, label %sync.continue

; CHECK: invoke.cont15:
; CHECK: call void @__csi_loopbody_exit(
; CHECK: call void @__csi_before_sync(
; CHECK-NEXT: sync within %syncreg2, label %sync.continue

sync.continue:                                    ; preds = %invoke.cont15
  invoke void @llvm.sync.unwind(token %syncreg2)
          to label %invoke.cont17 unwind label %lpad11.tfsplit.split-lp

; CHECK: sync.continue:
; CHECK-NOT: call
; CHECK-NEXT: invoke void @llvm.sync.unwind(token %syncreg2)
; CHECK-NEXT: to label %invoke.cont17 unwind label %[[CSI_LPAD_SPLIT:.+]]

invoke.cont17:                                    ; preds = %sync.continue
  tail call void @llvm.taskframe.end(token %0)
  br label %if.end

; CHECK: invoke.cont17:
; CHECK-NEXT: call void @__csi_after_sync(
; CHECK-NEXT: call void @llvm.taskframe.end(

lpad:                                             ; preds = %det.achd
  %1 = landingpad { ptr, i32 }
          cleanup
  invoke void @llvm.detached.rethrow.sl_p0i32s(token %syncreg2, { ptr, i32 } %1)
          to label %unreachable unwind label %lpad4

lpad4:                                            ; preds = %if.then.tf.tf.tf.tf, %lpad
  %2 = landingpad { ptr, i32 }
          cleanup
  br label %lpad11

; CHECK: [[CSI_LPAD_SPLIT]]:
; CHECK-NEXT: landingpad
; CHECK-NEXT: cleanup
; CHECK-NEXT: call void @__csi_after_sync(

lpad11.tfsplit.split-lp:                          ; preds = %det.cont, %sync.continue
  %lpad.tfsplit.split-lp100 = landingpad { ptr, i32 }
          cleanup
  br label %lpad11

lpad11:                                           ; preds = %lpad11.tfsplit.split-lp, %lpad4
  %lpad.phi101 = phi { ptr, i32 } [ %2, %lpad4 ], [ %lpad.tfsplit.split-lp100, %lpad11.tfsplit.split-lp ]
  invoke void @llvm.taskframe.resume.sl_p0i32s(token %0, { ptr, i32 } %lpad.phi101)
          to label %unreachable unwind label %lpad23.tfsplit

lpad23.tfsplit:                                   ; preds = %lpad11
  %lpad.tfsplit = landingpad { ptr, i32 }
          cleanup
  br label %lpad23

lpad23.tfsplit.split-lp.tfsplit:                  ; preds = %lpad48
  %lpad.tfsplit102 = landingpad { ptr, i32 }
          cleanup
  br label %lpad23

lpad23.tfsplit.split-lp.tfsplit.split-lp:         ; preds = %if.end
  %lpad.tfsplit.split-lp103 = landingpad { ptr, i32 }
          cleanup
  br label %lpad23

lpad23:                                           ; preds = %lpad23.tfsplit.split-lp.tfsplit, %lpad23.tfsplit.split-lp.tfsplit.split-lp, %lpad23.tfsplit
  %lpad.phi = phi { ptr, i32 } [ %lpad.tfsplit, %lpad23.tfsplit ], [ %lpad.tfsplit102, %lpad23.tfsplit.split-lp.tfsplit ], [ %lpad.tfsplit.split-lp103, %lpad23.tfsplit.split-lp.tfsplit.split-lp ]
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %w)
  invoke void @llvm.detached.rethrow.sl_p0i32s(token %syncreg, { ptr, i32 } %lpad.phi)
          to label %unreachable unwind label %lpad69.loopexit

if.else.tf.tf.tf.tf:                              ; preds = %pfor.body.entry
  detach within %syncreg2, label %det.achd28, label %det.cont42 unwind label %lpad39

det.achd28:                                       ; preds = %if.else.tf.tf.tf.tf
  %add27 = or disjoint i32 %__begin.0, 1
  %call33 = invoke noundef i32 @_Z3bari(i32 noundef %add27)
          to label %invoke.cont32 unwind label %lpad29

invoke.cont32:                                    ; preds = %det.achd28
  store i32 %call33, ptr %w, align 4, !tbaa !5
  reattach within %syncreg2, label %det.cont42

det.cont42:                                       ; preds = %if.else.tf.tf.tf.tf, %invoke.cont32
  %call52 = invoke noundef i32 @_Z3bari(i32 noundef %__begin.0)
          to label %invoke.cont51 unwind label %lpad48.tfsplit.split-lp

invoke.cont51:                                    ; preds = %det.cont42
  sync within %syncreg2, label %sync.continue53

; CHECK: invoke.cont51:
; CHECK: call void @__csi_loopbody_exit(
; CHECK: call void @__csi_before_sync(
; CHECK-NEXT: sync within %syncreg2, label %sync.continue53

sync.continue53:                                  ; preds = %invoke.cont51
  invoke void @llvm.sync.unwind(token %syncreg2)
          to label %invoke.cont54 unwind label %lpad48.tfsplit.split-lp

; CHECK: sync.continue53:
; CHECK-NOT: call
; CHECK-NEXT: invoke void @llvm.sync.unwind(token %syncreg2)
; CHECK-NEXT: to label %invoke.cont54 unwind label %[[CSI_LPAD_SPLIT2:.+]]

invoke.cont54:                                    ; preds = %sync.continue53
  tail call void @llvm.taskframe.end(token %0)
  br label %if.end

; CHECK: invoke.cont54:
; CHECK-NEXT: call void @__csi_after_sync(
; CHECK-NEXT: call void @llvm.taskframe.end(

lpad29:                                           ; preds = %det.achd28
  %3 = landingpad { ptr, i32 }
          cleanup
  invoke void @llvm.detached.rethrow.sl_p0i32s(token %syncreg2, { ptr, i32 } %3)
          to label %unreachable unwind label %lpad39

lpad39:                                           ; preds = %if.else.tf.tf.tf.tf, %lpad29
  %4 = landingpad { ptr, i32 }
          cleanup
  br label %lpad48

; CHECK: [[CSI_LPAD_SPLIT2]]:
; CHECK-NEXT: landingpad
; CHECK-NEXT: cleanup
; CHECK-NEXT: call void @__csi_after_sync(

lpad48.tfsplit.split-lp:                          ; preds = %det.cont42, %sync.continue53
  %lpad.tfsplit.split-lp = landingpad { ptr, i32 }
          cleanup
  br label %lpad48

lpad48:                                           ; preds = %lpad48.tfsplit.split-lp, %lpad39
  %lpad.phi106 = phi { ptr, i32 } [ %4, %lpad39 ], [ %lpad.tfsplit.split-lp, %lpad48.tfsplit.split-lp ]
  invoke void @llvm.taskframe.resume.sl_p0i32s(token %0, { ptr, i32 } %lpad.phi106)
          to label %unreachable unwind label %lpad23.tfsplit.split-lp.tfsplit

if.end:                                           ; preds = %invoke.cont54, %invoke.cont17
  %w.0.load110 = load i32, ptr %w, align 4
  %call61 = invoke noundef i32 @_Z3bari(i32 noundef %w.0.load110)
          to label %invoke.cont60 unwind label %lpad23.tfsplit.split-lp.tfsplit.split-lp

invoke.cont60:                                    ; preds = %if.end
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %w)
  reattach within %syncreg, label %pfor.inc

pfor.inc:                                         ; preds = %pfor.cond, %invoke.cont60
  %inc = add nuw nsw i32 %__begin.0, 1
  %exitcond.not = icmp eq i32 %inc, %n
  br i1 %exitcond.not, label %pfor.cond.cleanup, label %pfor.cond, !llvm.loop !9

pfor.cond.cleanup:                                ; preds = %pfor.inc
  sync within %syncreg, label %sync.continue73

lpad69.loopexit:                                  ; preds = %lpad23, %pfor.cond
  %lpad.loopexit = landingpad { ptr, i32 }
          catch ptr null
  br label %lpad69

lpad69.loopexit.split-lp:                         ; preds = %sync.continue73
  %lpad.loopexit.split-lp = landingpad { ptr, i32 }
          catch ptr null
  br label %lpad69

lpad69:                                           ; preds = %lpad69.loopexit.split-lp, %lpad69.loopexit
  %lpad.phi109 = phi { ptr, i32 } [ %lpad.loopexit, %lpad69.loopexit ], [ %lpad.loopexit.split-lp, %lpad69.loopexit.split-lp ]
  %5 = extractvalue { ptr, i32 } %lpad.phi109, 0
  %6 = tail call ptr @__cxa_begin_catch(ptr %5) #6
  %call83 = invoke noundef i32 @_Z3bari(i32 noundef 0)
          to label %invoke.cont82 unwind label %lpad81

sync.continue73:                                  ; preds = %pfor.cond.cleanup
  invoke void @llvm.sync.unwind(token %syncreg)
          to label %try.cont unwind label %lpad69.loopexit.split-lp

invoke.cont82:                                    ; preds = %lpad69
  tail call void @__cxa_end_catch()
  br label %try.cont

try.cont:                                         ; preds = %entry, %sync.continue73, %invoke.cont82
  ret void

lpad81:                                           ; preds = %lpad69
  %7 = landingpad { ptr, i32 }
          cleanup
  invoke void @__cxa_end_catch()
          to label %eh.resume unwind label %terminate.lpad

eh.resume:                                        ; preds = %lpad81
  resume { ptr, i32 } %7

terminate.lpad:                                   ; preds = %lpad81
  %8 = landingpad { ptr, i32 }
          catch ptr null
  %9 = extractvalue { ptr, i32 } %8, 0
  tail call void @__clang_call_terminate(ptr %9) #7
  unreachable

unreachable:                                      ; preds = %lpad23, %lpad48, %lpad29, %lpad11, %lpad
  unreachable
}

; Function Attrs: mustprogress nounwind willreturn memory(argmem: readwrite)
declare token @llvm.syncregion.start() #1

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #2

; Function Attrs: mustprogress nounwind willreturn memory(argmem: readwrite)
declare token @llvm.taskframe.create() #1

declare noundef i32 @_Z3bari(i32 noundef) local_unnamed_addr #3

declare i32 @__gxx_personality_v0(...)

; Function Attrs: mustprogress willreturn memory(argmem: readwrite)
declare void @llvm.detached.rethrow.sl_p0i32s(token, { ptr, i32 }) #4

; Function Attrs: mustprogress willreturn memory(argmem: readwrite)
declare void @llvm.taskframe.resume.sl_p0i32s(token, { ptr, i32 }) #4

; Function Attrs: mustprogress willreturn memory(argmem: readwrite)
declare void @llvm.sync.unwind(token) #4

; Function Attrs: mustprogress nounwind willreturn memory(argmem: readwrite)
declare void @llvm.taskframe.end(token) #1

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #2

declare ptr @__cxa_begin_catch(ptr) local_unnamed_addr

declare void @__cxa_end_catch() local_unnamed_addr

; Function Attrs: noinline noreturn nounwind uwtable
define linkonce_odr hidden void @__clang_call_terminate(ptr noundef %0) local_unnamed_addr #5 comdat {
  %2 = tail call ptr @__cxa_begin_catch(ptr %0) #6
  tail call void @_ZSt9terminatev() #7
  unreachable
}

declare void @_ZSt9terminatev() local_unnamed_addr

attributes #0 = { mustprogress uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { mustprogress nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #3 = { "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { mustprogress willreturn memory(argmem: readwrite) }
attributes #5 = { noinline noreturn nounwind uwtable "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #6 = { nounwind }
attributes #7 = { noreturn nounwind }

!llvm.module.flags = !{!0, !1, !2, !3}
!llvm.ident = !{!4}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{!"clang version 19.1.7 (git@github.com:neboat/opencilk-project.git 8789ce788f0a6ecd35d9e9eef9e6652704d143d2)"}
!5 = !{!6, !6, i64 0}
!6 = !{!"int", !7, i64 0}
!7 = !{!"omnipotent char", !8, i64 0}
!8 = !{!"Simple C++ TBAA"}
!9 = distinct !{!9, !10, !11}
!10 = !{!"llvm.loop.mustprogress"}
!11 = !{!"tapir.loop.spawn.strategy", i32 1}
