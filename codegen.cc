#include "codegen.h"
#include "quickjs.h"

#if defined(_M_X64) || defined(__x86_64__)
#include "asmjit/asmjit/x86.h"
#define arch asmjit::x86
masm_reg_t kRetReg = (masm_reg_t)&arch::rax;
masm_reg_t kStkReg = (masm_reg_t)&arch::rbx;
#define kStkReg arch::rbx
#ifdef _WIN32
#define kArg1Reg arch::rcx
#define kArg2Reg arch::rdx
#define kArg3Reg arch::r8
#define kArg4Reg arch::r9
#else
#define kArg1Reg arch::rdi
#define kArg2Reg arch::rsi
#define kArg3Reg arch::rdx
#define kArg4Reg arch::rcx
#endif
#elif defined(__aarch64__) || defined(_M_ARM64)
#include "asmjit/asmjit/a64.h"
#define arch asmjit::a64
masm_reg_t kRetReg = (masm_reg_t)&arch::x0;
masm_reg_t kStkReg = (masm_reg_t)&arch::x19;
masm_reg_t kArg1Reg = (masm_reg_t)&arch::x0;
masm_reg_t kArg2Reg = (masm_reg_t)&arch::x1;
masm_reg_t kArg3Reg = (masm_reg_t)&arch::x2;
masm_reg_t kArg4Reg = (masm_reg_t)&arch::x3;
masm_reg_t kArgBufReg = (masm_reg_t)&arch::x20;
masm_reg_t kVarBufReg = (masm_reg_t)&arch::x21;
masm_reg_t kHeapReg = (masm_reg_t)&arch::x22;
#define kStkReg arch::x19
#define kArgBufReg arch::x20
#define kVarBufReg arch::x21
#define kHeapReg arch::x22
#define kArg1Reg arch::x0
#define kArg2Reg arch::x1
#define kArg3Reg arch::x2
#define kArg4Reg arch::x3
#else
#error unsupported architecture
#endif

struct masm {
  asmjit::JitRuntime *rt;
  asmjit::CodeHolder code;
  arch::Assembler as;
#ifndef ASMJIT_NO_LOGGING
  asmjit::FileLogger logger{stdout};
#endif
};

masm_t masm_new() {
  masm_t m = new struct masm;
  m->rt = new asmjit::JitRuntime;
  m->code.init(m->rt->environment(), m->rt->cpu_features());
  m->code.attach(&m->as);
#ifndef ASMJIT_NO_LOGGING
  m->logger.set_indentation(asmjit::FormatIndentationGroup::kCode, 6);
  m->logger.set_indentation(asmjit::FormatIndentationGroup::kComment, 6);
  m->code.set_logger(&m->logger);
#endif
  return m;
}

void masm_delete(masm_t masm) { delete masm; }

#define __ masm->as.

void masm_prolog(masm_t masm) {
#if defined(_M_X64) || defined(__x86_64__)
  __ push(kStkReg);
  __ push(kArgBufReg);
  __ push(kVarBufReg);
  __ push(kHeapReg);
  __ mov(kStkReg, kArg1Reg);
#else
  __ stp(arch::x29, arch::x30, arch::ptr_pre(arch::sp, -16));
  __ stp(kStkReg, kArgBufReg, arch::ptr_pre(arch::sp, -16));
  __ stp(kVarBufReg, kHeapReg, arch::ptr_pre(arch::sp, -16));
  __ mov(kStkReg, arch::x0);
  __ mov(kHeapReg, arch::x1);
  __ mov(kArgBufReg, arch::x2);
  __ mov(kVarBufReg, arch::x3);
#endif
}

void masm_epilog(masm_t masm) {
#if defined(_M_X64) || defined(__x86_64__)
  __ pop(kHeapReg);
  __ pop(kVarBufReg);
  __ pop(kArgBufReg);
  __ pop(kStkReg);
  __ ret();
#else
  __ ldp(kVarBufReg, kHeapReg, arch::ptr_post(arch::sp, 16));
  __ ldp(kStkReg, kArgBufReg, arch::ptr_post(arch::sp, 16));
  __ ldp(arch::x29, arch::x30, arch::ptr_post(arch::sp, 16));
  __ ret(arch::x30);
#endif
}

masm_label_t masm_new_label(masm_t masm, const char *name) {
  auto label = __ new_label();
  return (masm_label_t)(label._base_id + 1LL);
}

void masm_bind(masm_t masm, masm_label_t id) {
  asmjit::Label label;
  label._base_id = (intptr_t)id - 1;
  __ bind(label);
}

void masm_inline_comment(masm_t masm, const char *data) {
#ifndef ASMJIT_NO_LOGGING
  if (data) {
    __ set_inline_comment(data);
  }
#endif
}

void masm_comment(masm_t masm, const char *fmt, ...) {
#ifndef ASMJIT_NO_LOGGING
  if (fmt) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    __ comment(buf);
    va_end(args);
  }
#endif
}

void masm_mov_reg_imm64(masm_t masm, masm_reg_t reg, int64_t imm) {
  if (imm == 0) {
#if defined(_M_X64) || defined(__x86_64__)
    __ xor (*(arch::Gp *)reg, *(arch::Gp *)reg);
#else
    __ mov(*(arch::Gp *)reg, arch::xzr);
#endif
  } else {
    __ mov(*(arch::Gp *)reg, imm);
  }
}

void masm_mov_reg_imm32(masm_t masm, masm_reg_t reg, int32_t imm) {
  if (imm == 0) {
#if defined(_M_X64) || defined(__x86_64__)
    __ xor (*(arch::Gp *)reg, *(arch::Gp *)reg);
#else
    __ mov(*(arch::Gp *)reg, arch::xzr);
#endif
  } else {
    __ mov((*(arch::Gp *)reg).r32(), imm);
  }
}

void masm_ldp(masm_t masm, masm_reg_t dst1, masm_reg_t dst2, masm_reg_t src,
              int disp) {
#if defined(_M_X64) || defined(__x86_64__)
#else
  __ ldp(*(arch::Gp *)dst1, *(arch::Gp *)dst2,
         arch::ptr(*(arch::Gp *)src, disp));
#endif
}

void masm_ldr(masm_t masm, masm_reg_t dst, masm_reg_t src, int disp) {
#if defined(_M_X64) || defined(__x86_64__)
#else
  auto err = __ ldr(*(arch::Gp *)dst, arch::ptr(*(arch::Gp *)src, disp));
  if (err != asmjit::Error::kOk) {
    __ mov(arch::x16, disp);
    __ ldr(*(arch::Gp *)dst, arch::ptr(*(arch::Gp *)src, arch::x16));
  }
#endif
}

void masm_str(masm_t masm, masm_reg_t src, masm_reg_t dst, int disp) {
#if defined(_M_X64) || defined(__x86_64__)
#else
  auto err = __ str(*(arch::Gp *)src, arch::ptr(*(arch::Gp *)dst, disp));
  if (err != asmjit::Error::kOk) {
    __ mov(arch::x16, disp);
    __ str(*(arch::Gp *)src, arch::ptr(*(arch::Gp *)dst, arch::x16));
  }
#endif
}

void masm_lsl(masm_t masm, masm_reg_t dst, masm_reg_t src, int bits) {
#if defined(_M_X64) || defined(__x86_64__)
#else
  __ lsl(*(arch::Gp *)dst, *(arch::Gp *)src, bits);
#endif
}

void masm_lsr(masm_t masm, masm_reg_t dst, masm_reg_t src, int bits) {
#if defined(_M_X64) || defined(__x86_64__)
#else
  __ lsr(*(arch::Gp *)dst, *(arch::Gp *)src, bits);
#endif
}

void masm_asr(masm_t masm, masm_reg_t dst, masm_reg_t src, int bits) {
#if defined(_M_X64) || defined(__x86_64__)
#else
  __ asr(*(arch::Gp *)dst, *(arch::Gp *)src, bits);
#endif
}

void masm_push_imm64(masm_t masm, int64_t imm) {
#if defined(_M_X64) || defined(__x86_64__)
#else
  __ mov(arch::x16, imm);
  __ str(arch::x16, arch::ptr_post(kStkReg, 8));
#endif
}

void masm_push_reg(masm_t masm, masm_reg_t reg) {
#if defined(_M_X64) || defined(__x86_64__)
#else
  __ str(*(arch::Gp *)reg, arch::ptr_post(kStkReg, 8));
#endif
}

void masm_pop_reg(masm_t masm, masm_reg_t reg) {
#if defined(_M_X64) || defined(__x86_64__)
#else
  __ ldr(*(arch::Gp *)reg, arch::ptr_pre(kStkReg, -8));
#endif
}

void masm_inc32_rtmem(masm_t masm, masm_reg_t reg) {
#if defined(_M_X64) || defined(__x86_64__)
#else
  __ ldr(arch::x17, arch::ptr(kHeapReg, *(arch::Gp *)reg));
  __ add(arch::x17, arch::x17, 1);
  __ str(arch::x17, arch::ptr(kHeapReg, *(arch::Gp *)reg));
#endif
}

void masm_add_reg(masm_t masm, masm_reg_t dst, masm_reg_t x, masm_reg_t y) {
#if defined(_M_X64) || defined(__x86_64__)
#else
  __ add(*(arch::Gp *)dst, *(arch::Gp *)x, *(arch::Gp *)y);
#endif
}

void masm_add_reg_imm32(masm_t masm, masm_reg_t dst, masm_reg_t src,
                        int32_t imm) {
#if defined(_M_X64) || defined(__x86_64__)
#else
  if (imm > 0)
    __ add(*(arch::Gp *)dst, *(arch::Gp *)src, imm);
  else
    __ sub(*(arch::Gp *)dst, *(arch::Gp *)src, -imm);
#endif
}

void masm_zero_reg_hi32(masm_t masm, masm_reg_t reg) {
  __ mov((*(arch::Gp *)reg).r32(), (*(arch::Gp *)reg).r32());
}

void masm_cmp_reg(masm_t masm, masm_reg_t x, masm_reg_t y) {
  __ cmp(*(arch::Gp *)x, *(arch::Gp *)y);
}

void masm_cmp_reg_imm32(masm_t masm, masm_reg_t reg, int32_t imm) {
#if defined(_M_X64) || defined(__x86_64__)
  __ cmp((*(arch::Gp *)reg).r32(), imm);
#else
  auto err = __ cmp((*(arch::Gp *)reg).r32(), imm);
  if (err != asmjit::Error::kOk) {
    __ mov(arch::x16, imm);
    __ cmp((*(arch::Gp *)reg).r32(), arch::w16);
  }
#endif
}

void *masm_finish(masm_t masm) {
  void *fn;
  auto err = masm->rt->add(&fn, &masm->code);
  if (err != asmjit::Error::kOk) {
    return nullptr;
  }
  return fn;
}

void masm_b_lo(masm_t masm, masm_label_t id) {
  asmjit::Label label;
  label._base_id = (intptr_t)id - 1;
#if defined(_M_X64) || defined(__x86_64__)
  __ jb(label);
#else
  __ b_lo(label);
#endif
}

void masm_b_eq(masm_t masm, masm_label_t id) {
  asmjit::Label label;
  label._base_id = (intptr_t)id - 1;
#if defined(_M_X64) || defined(__x86_64__)
  __ je(label);
#else
  __ b_eq(label);
#endif
}

void masm_b_hi(masm_t masm, masm_label_t id) {
  asmjit::Label label;
  label._base_id = (intptr_t)id - 1;
#if defined(_M_X64) || defined(__x86_64__)
  __ ja(label);
#else
  __ b_hi(label);
#endif
}

void masm_jmp(masm_t masm, masm_label_t id) {
  asmjit::Label label;
  label._base_id = (intptr_t)id - 1;
#if defined(_M_X64) || defined(__x86_64__)
  __ jmp(label);
#else
  __ b(label);
#endif
}
