; ModuleID = '/home/bulow/capslock-artefacts/llvm-pass/test/two-functions.ll'
source_filename = "llvm-pass/test/two-functions.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@0 = private unnamed_addr constant [7 x i8] c"helper\00", align 1
@1 = private unnamed_addr constant [5 x i8] c"main\00", align 1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @helper(i32 noundef %value) #0 {
entry:
  call void @capslock_function_entry(ptr @0)
  %value.addr = alloca i32, align 4
  store i32 %value, ptr %value.addr, align 4
  %0 = load i32, ptr %value.addr, align 4
  %add = add nsw i32 %0, 1
  ret i32 %add
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
entry:
  call void @capslock_function_entry(ptr @1)
  %retval = alloca i32, align 4
  store i32 0, ptr %retval, align 4
  %call = call i32 @helper(i32 noundef 41)
  %cmp = icmp eq i32 %call, 42
  %0 = zext i1 %cmp to i64
  %cond = select i1 %cmp, i32 0, i32 1
  ret i32 %cond
}

declare void @capslock_function_entry(ptr)

attributes #0 = { noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"clang version 18.1.4 (https://github.com/rust-lang/llvm-project.git 5399a24c66cb6164cf32280e7d300488c90d5765)"}
