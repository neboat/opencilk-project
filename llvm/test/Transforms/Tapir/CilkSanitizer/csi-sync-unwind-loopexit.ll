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
  detach within %syncreg, label %pfor.body.entry, label %pfor.inc unwind label %lpad35.loopexit

pfor.body.entry:                                  ; preds = %pfor.cond
  %w = alloca i32, align 4
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %w)
  %0 = tail call token @llvm.taskframe.create()
  %syncreg2 = tail call token @llvm.syncregion.start()
  detach within %syncreg2, label %det.achd, label %det.cont unwind label %lpad4

det.achd:                                         ; preds = %pfor.body.entry
  %call = invoke noundef i32 @_Z3bari(i32 noundef %__begin.0)
          to label %invoke.cont unwind label %lpad

invoke.cont:                                      ; preds = %det.achd
  store i32 %call, ptr %w, align 4, !tbaa !5
  reattach within %syncreg2, label %det.cont

det.cont:                                         ; preds = %pfor.body.entry, %invoke.cont
  %add14 = add nuw nsw i32 %__begin.0, 1
  %call16 = invoke noundef i32 @_Z3bari(i32 noundef %add14)
          to label %invoke.cont15 unwind label %lpad11.tfsplit.split-lp

invoke.cont15:                                    ; preds = %det.cont
  sync within %syncreg2, label %sync.continue

sync.continue:                                    ; preds = %invoke.cont15
  invoke void @llvm.sync.unwind(token %syncreg2)
          to label %invoke.cont17 unwind label %lpad11.tfsplit.split-lp

; CHECK: invoke.cont15:
; CHECK: call void @__csi_bb_exit(
; CHECK: call void @__csi_loopbody_exit(
; CHECK: call void @__csi_before_sync(
; CHECK: sync within %syncreg2, label %sync.continue

; CHECK: sync.continue:
; CHECK-NOT: call
; CHECK-NEXT: invoke void @llvm.sync.unwind(token %syncreg2)
; CHECK-NEXT: to label %invoke.cont17 unwind label %[[CSI_LPAD_SPLIT:.+]]

invoke.cont17:                                    ; preds = %sync.continue
  tail call void @llvm.taskframe.end(token %0)
  %w.0.load67 = load i32, ptr %w, align 4
  %call27 = invoke noundef i32 @_Z3bari(i32 noundef %w.0.load67)
          to label %invoke.cont26 unwind label %lpad23.tfsplit.split-lp

; CHECK: invoke.cont17:
; CHECK-NEXT: call void @__csi_after_sync(
; CHECK-NEXT: call void @llvm.taskframe.end(

invoke.cont26:                                    ; preds = %invoke.cont17
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %w)
  reattach within %syncreg, label %pfor.inc

pfor.inc:                                         ; preds = %pfor.cond, %invoke.cont26
  %inc = add nuw nsw i32 %__begin.0, 1
  %exitcond.not = icmp eq i32 %inc, %n
  br i1 %exitcond.not, label %pfor.cond.cleanup, label %pfor.cond, !llvm.loop !9

pfor.cond.cleanup:                                ; preds = %pfor.inc
  sync within %syncreg, label %sync.continue39

lpad:                                             ; preds = %det.achd
  %1 = landingpad { ptr, i32 }
          cleanup
  invoke void @llvm.detached.rethrow.sl_p0i32s(token %syncreg2, { ptr, i32 } %1)
          to label %unreachable unwind label %lpad4

lpad4:                                            ; preds = %pfor.body.entry, %lpad
  %2 = landingpad { ptr, i32 }
          cleanup
  br label %lpad11

; CHECK: [[CSI_LPAD_SPLIT]]:
; CHECK-NEXT: landingpad
; CHECK-NEXT: cleanup
; CHECK-NEXT: call void @__csi_after_sync(

lpad11.tfsplit.split-lp:                          ; preds = %det.cont, %sync.continue
  %lpad.tfsplit.split-lp63 = landingpad { ptr, i32 }
          cleanup
  br label %lpad11

lpad11:                                           ; preds = %lpad11.tfsplit.split-lp, %lpad4
  %lpad.phi64 = phi { ptr, i32 } [ %2, %lpad4 ], [ %lpad.tfsplit.split-lp63, %lpad11.tfsplit.split-lp ]
  invoke void @llvm.taskframe.resume.sl_p0i32s(token %0, { ptr, i32 } %lpad.phi64)
          to label %unreachable unwind label %lpad23.tfsplit

lpad23.tfsplit:                                   ; preds = %lpad11
  %lpad.tfsplit = landingpad { ptr, i32 }
          cleanup
  br label %lpad23

lpad23.tfsplit.split-lp:                          ; preds = %invoke.cont17
  %lpad.tfsplit.split-lp = landingpad { ptr, i32 }
          cleanup
  br label %lpad23

lpad23:                                           ; preds = %lpad23.tfsplit.split-lp, %lpad23.tfsplit
  %lpad.phi = phi { ptr, i32 } [ %lpad.tfsplit, %lpad23.tfsplit ], [ %lpad.tfsplit.split-lp, %lpad23.tfsplit.split-lp ]
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %w)
  invoke void @llvm.detached.rethrow.sl_p0i32s(token %syncreg, { ptr, i32 } %lpad.phi)
          to label %unreachable unwind label %lpad35.loopexit

lpad35.loopexit:                                  ; preds = %lpad23, %pfor.cond
  %lpad.loopexit = landingpad { ptr, i32 }
          catch ptr null
  br label %lpad35

lpad35.loopexit.split-lp:                         ; preds = %sync.continue39
  %lpad.loopexit.split-lp = landingpad { ptr, i32 }
          catch ptr null
  br label %lpad35

lpad35:                                           ; preds = %lpad35.loopexit.split-lp, %lpad35.loopexit
  %lpad.phi66 = phi { ptr, i32 } [ %lpad.loopexit, %lpad35.loopexit ], [ %lpad.loopexit.split-lp, %lpad35.loopexit.split-lp ]
  %3 = extractvalue { ptr, i32 } %lpad.phi66, 0
  %4 = tail call ptr @__cxa_begin_catch(ptr %3) #6
  %call49 = invoke noundef i32 @_Z3bari(i32 noundef 0)
          to label %invoke.cont48 unwind label %lpad47

sync.continue39:                                  ; preds = %pfor.cond.cleanup
  invoke void @llvm.sync.unwind(token %syncreg)
          to label %try.cont unwind label %lpad35.loopexit.split-lp

invoke.cont48:                                    ; preds = %lpad35
  tail call void @__cxa_end_catch()
  br label %try.cont

try.cont:                                         ; preds = %entry, %sync.continue39, %invoke.cont48
  ret void

lpad47:                                           ; preds = %lpad35
  %5 = landingpad { ptr, i32 }
          cleanup
  invoke void @__cxa_end_catch()
          to label %eh.resume unwind label %terminate.lpad

eh.resume:                                        ; preds = %lpad47
  resume { ptr, i32 } %5

terminate.lpad:                                   ; preds = %lpad47
  %6 = landingpad { ptr, i32 }
          catch ptr null
  %7 = extractvalue { ptr, i32 } %6, 0
  tail call void @__clang_call_terminate(ptr %7) #7
  unreachable

unreachable:                                      ; preds = %lpad23, %lpad11, %lpad
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
!9 = distinct !{!9, !10, !11, !12}
!10 = !{!"llvm.loop.mustprogress"}
!11 = !{!"tapir.loop.spawn.strategy", i32 1}
!12 = !{!"llvm.loop.unroll.disable"}
