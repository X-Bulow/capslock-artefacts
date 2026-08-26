target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

declare void @llvm.va_start(ptr)
declare void @llvm.memcpy.element.unordered.atomic.p0.p0.i64(ptr align 8, ptr align 8, i64, i32)

define i32 @uses_varargs(ptr %fmt, ...) {
entry:
  %ap = alloca ptr, align 8
  call void @llvm.va_start(ptr %ap)
  %arg = va_arg ptr %ap, i32
  ret i32 %arg
}

define void @uses_atomic_memcpy(ptr align 8 %dst, ptr align 8 %src) {
entry:
  call void @llvm.memcpy.element.unordered.atomic.p0.p0.i64(ptr align 8 %dst, ptr align 8 %src, i64 8, i32 8)
  ret void
}
