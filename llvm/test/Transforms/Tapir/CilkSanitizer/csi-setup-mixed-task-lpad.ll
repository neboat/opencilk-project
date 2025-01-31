; Check that csi-setup properly promotes calls to invokes when a call that might throw is inside a task with a detach-unwind.
;
; RUN: opt < %s -passes="csi-setup" -S | FileCheck %s
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@_ZTIi = external constant ptr

; Function Attrs: mustprogress noinline optnone sanitize_cilk uwtable
define dso_local void @_Z3fooi(i32 noundef %n) #0 personality ptr @__gxx_personality_v0 {
entry:
  %n.addr = alloca i32, align 4
  %syncreg = call token @llvm.syncregion.start()
  %__init = alloca i32, align 4
  %__limit = alloca i32, align 4
  %__begin = alloca i32, align 4
  %__end = alloca i32, align 4
  %exn.slot4 = alloca ptr, align 8
  %ehselector.slot5 = alloca i32, align 4
  store i32 %n, ptr %n.addr, align 4
  store i32 0, ptr %__init, align 4
  %0 = load i32, ptr %n.addr, align 4
  store i32 %0, ptr %__limit, align 4
  %1 = load i32, ptr %__init, align 4
  %2 = load i32, ptr %__limit, align 4
  %cmp = icmp slt i32 %1, %2
  br i1 %cmp, label %pfor.ph, label %pfor.end

pfor.ph:                                          ; preds = %entry
  store i32 0, ptr %__begin, align 4
  %3 = load i32, ptr %__limit, align 4
  %4 = load i32, ptr %__init, align 4
  %sub = sub nsw i32 %3, %4
  store i32 %sub, ptr %__end, align 4
  br label %pfor.cond

pfor.cond:                                        ; preds = %pfor.inc, %pfor.ph
  br label %pfor.detach

pfor.detach:                                      ; preds = %pfor.cond
  %5 = load i32, ptr %__init, align 4
  %6 = load i32, ptr %__begin, align 4
  %add = add nsw i32 %5, %6
  detach within %syncreg, label %pfor.body.entry, label %pfor.inc unwind label %lpad3

pfor.body.entry:                                  ; preds = %pfor.detach
  %i = alloca i32, align 4
  %w = alloca i32, align 4
  %exn.slot = alloca ptr, align 8
  %ehselector.slot = alloca i32, align 4
  store i32 %add, ptr %i, align 4
  br label %pfor.body

pfor.body:                                        ; preds = %pfor.body.entry
  %7 = load i32, ptr %i, align 4
  %call = call noundef i32 @_Z3bari(i32 noundef %7)
  store i32 %call, ptr %w, align 4
  %exception = call ptr @__cxa_allocate_exception(i64 4) #4
  %8 = load i32, ptr %w, align 4
  %call1 = invoke noundef i32 @_Z3bari(i32 noundef %8)
          to label %invoke.cont unwind label %lpad

; CHECK: pfor.body:
; CHECK-NEXT: %[[ARG1:.+]] = load i32, ptr %i
; CHECK-NOT: call {{.*}}i32 @_Z3bari(i32 noundef %{{.*}})
; CHECK: invoke {{.*}}i32 @_Z3bari(i32 noundef %[[ARG1]])
; CHECK-NEXT: to label %[[CALL_NOEXC:.+]] unwind label %[[CSI_SETUP_LPAD:.+]]

; CHECK: [[CALL_NOEXC]]:
; CHECK: %[[ARG2:.+]] = load i32, ptr %w
; CHECK-NEXT: invoke noundef i32 @_Z3bari(i32 noundef %[[ARG2]])
; CHECK-NEXT: to label %invoke.cont unwind label %lpad

invoke.cont:                                      ; preds = %pfor.body
  store i32 %call1, ptr %exception, align 16
  call void @__cxa_throw(ptr %exception, ptr @_ZTIi, ptr null) #5
  unreachable

lpad:                                             ; preds = %pfor.body
  %9 = landingpad { ptr, i32 }
          cleanup
  %10 = extractvalue { ptr, i32 } %9, 0
  store ptr %10, ptr %exn.slot, align 8
  %11 = extractvalue { ptr, i32 } %9, 1
  store i32 %11, ptr %ehselector.slot, align 4
  call void @__cxa_free_exception(ptr %exception) #4
  %exn = load ptr, ptr %exn.slot, align 8
  %sel = load i32, ptr %ehselector.slot, align 4
  %lpad.val = insertvalue { ptr, i32 } undef, ptr %exn, 0
  %lpad.val2 = insertvalue { ptr, i32 } %lpad.val, i32 %sel, 1
  invoke void @llvm.detached.rethrow.sl_p0i32s(token %syncreg, { ptr, i32 } %lpad.val2)
          to label %unreachable unwind label %lpad3

; CHECK: lpad:
; CHECK-NEXT: landingpad
; CHECK-NEXT: cleanup
; CHECK: invoke void @llvm.detached.rethrow.sl_p0i32s(token %syncreg,
; CHECK-NEXT: to label %unreachable unwind label %lpad3

pfor.preattach:                                   ; No predecessors!
  reattach within %syncreg, label %pfor.inc

pfor.inc:                                         ; preds = %pfor.preattach, %pfor.detach
  %12 = load i32, ptr %__begin, align 4
  %inc = add nsw i32 %12, 1
  store i32 %inc, ptr %__begin, align 4
  %13 = load i32, ptr %__begin, align 4
  %14 = load i32, ptr %__end, align 4
  %cmp6 = icmp slt i32 %13, %14
  br i1 %cmp6, label %pfor.cond, label %pfor.cond.cleanup, !llvm.loop !6

pfor.cond.cleanup:                                ; preds = %pfor.inc
  sync within %syncreg, label %sync.continue

lpad3:                                            ; preds = %lpad, %pfor.detach
  %15 = landingpad { ptr, i32 }
          cleanup
  %16 = extractvalue { ptr, i32 } %15, 0
  store ptr %16, ptr %exn.slot4, align 8
  %17 = extractvalue { ptr, i32 } %15, 1
  store i32 %17, ptr %ehselector.slot5, align 4
  br label %eh.resume

sync.continue:                                    ; preds = %pfor.cond.cleanup
  call void @llvm.sync.unwind(token %syncreg)
  br label %pfor.end

pfor.end:                                         ; preds = %sync.continue, %entry
  ret void

eh.resume:                                        ; preds = %lpad3
  %exn7 = load ptr, ptr %exn.slot4, align 8
  %sel8 = load i32, ptr %ehselector.slot5, align 4
  %lpad.val9 = insertvalue { ptr, i32 } poison, ptr %exn7, 0
  %lpad.val10 = insertvalue { ptr, i32 } %lpad.val9, i32 %sel8, 1
  resume { ptr, i32 } %lpad.val10

unreachable:                                      ; preds = %lpad
  unreachable

; CHECK: [[CSI_SETUP_LPAD]]:
; CHECK-NEXT: landingpad
; CHECK-NEXT: cleanup
; CHECK-NEXT: invoke void @llvm.detached.rethrow.sl_p0i32s(token %syncreg,
; CHECK-NEXT: to label %[[CSI_SETUP_UNREACHABLE:.+]] unwind label %lpad3
}

; Function Attrs: nounwind willreturn memory(argmem: readwrite)
declare token @llvm.syncregion.start() #1

declare noundef i32 @_Z3bari(i32 noundef) #2

declare ptr @__cxa_allocate_exception(i64)

declare i32 @__gxx_personality_v0(...)

declare void @__cxa_free_exception(ptr)

declare void @__cxa_throw(ptr, ptr, ptr)

; Function Attrs: willreturn memory(argmem: readwrite)
declare void @llvm.detached.rethrow.sl_p0i32s(token, { ptr, i32 }) #3

; Function Attrs: willreturn memory(argmem: readwrite)
declare void @llvm.sync.unwind(token) #3

attributes #0 = { mustprogress noinline optnone sanitize_cilk uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { willreturn memory(argmem: readwrite) }
attributes #4 = { nounwind }
attributes #5 = { noreturn }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"clang version 19.1.7 (git@github.com:OpenCilk/opencilk-project.git e929b19f1ca3426871e22a5843cc9e5725894576)"}
!6 = distinct !{!6, !7, !8}
!7 = !{!"llvm.loop.mustprogress"}
!8 = !{!"tapir.loop.spawn.strategy", i32 1}
