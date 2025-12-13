#include "codegen.h"
#include "quickjs.h"
#include <assert.h>

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
masm_reg_t kTmpReg = (masm_reg_t)&arch::x16;
masm_reg_t kArgBufReg = (masm_reg_t)&arch::x20;
masm_reg_t kVarBufReg = (masm_reg_t)&arch::x21;
masm_reg_t kCtxReg = (masm_reg_t)&arch::x22;
masm_reg_t kSfReg = (masm_reg_t)&arch::x23;
masm_reg_t kHeapReg = (masm_reg_t)&arch::x24;
#define kStkReg arch::x19
#define kArgBufReg arch::x20
#define kVarBufReg arch::x21
#define kCtxReg arch::x22
#define kSfReg arch::x23
#define kHeapReg arch::x24
#define kArg1Reg arch::x0
#define kArg2Reg arch::x1
#define kArg3Reg arch::x2
#define kArg4Reg arch::x3
#define kTmpReg arch::x16
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

extern "C" {
extern const int kJSContext_rt;
extern const int kJSRuntime_heap;
extern const int kJSStackFrame_arg_buf;
extern const int kJSStackFrame_sp;
};

void masm_prolog(masm_t masm) {
#if defined(_M_X64) || defined(__x86_64__)
  __ push(kStkReg);
  __ push(kArgBufReg);
  __ push(kVarBufReg);
  __ push(kCtxReg);
  __ mov(kStkReg, kArg1Reg);
  __ mov(kCtxReg, kArg2Reg);
#else
  __ stp(arch::x29, arch::x30, arch::ptr_pre(arch::sp, -16));
  __ stp(kStkReg, kCtxReg, arch::ptr_pre(arch::sp, -16));
  __ stp(kVarBufReg, kArgBufReg, arch::ptr_pre(arch::sp, -16));
  __ stp(kSfReg, kHeapReg, arch::ptr_pre(arch::sp, -16));
  __ mov(kSfReg, arch::x0);
  __ mov(kCtxReg, arch::x1);
  __ ldp(kArgBufReg, kVarBufReg, arch::ptr(arch::x0, kJSStackFrame_arg_buf));
  __ ldr(kStkReg, arch::ptr(arch::x0, kJSStackFrame_sp));
  __ ldr(arch::x16, arch::ptr(arch::x1, kJSContext_rt));
  __ ldr(kHeapReg, arch::ptr(arch::x16, kJSRuntime_heap));
#endif
}

void masm_epilog(masm_t masm) {
#if defined(_M_X64) || defined(__x86_64__)
  __ pop(kCtxReg);
  __ pop(kVarBufReg);
  __ pop(kArgBufReg);
  __ pop(kStkReg);
  __ ret();
#else
  __ str(kStkReg, arch::ptr(kSfReg, kJSStackFrame_sp));
  __ ldp(kSfReg, kHeapReg, arch::ptr_post(arch::sp, 16));
  __ ldp(kVarBufReg, kArgBufReg, arch::ptr_post(arch::sp, 16));
  __ ldp(kStkReg, kCtxReg, arch::ptr_post(arch::sp, 16));
  __ ldp(arch::x29, arch::x30, arch::ptr_post(arch::sp, 16));
  __ ret(arch::x30);
#endif
}

masm_label_t masm_new_label(masm_t masm, const char *name) {
  auto label = name ? __ new_named_label(name) : __ new_label();
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

void masm_mov_reg(masm_t masm, masm_reg_t dst, masm_reg_t src) {
  __ mov(*(arch::Gp *)dst, *(arch::Gp *)src);
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

void masm_ldr2(masm_t masm, masm_reg_t dst, masm_reg_t src, masm_reg_t idx) {
#if defined(_M_X64) || defined(__x86_64__)
#else
  __ ldr(*(arch::Gp *)dst, arch::ptr(*(arch::Gp *)src, *(arch::Gp *)idx));
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

void masm_str2(masm_t masm, masm_reg_t src, masm_reg_t dst, masm_reg_t idx) {
#if defined(_M_X64) || defined(__x86_64__)
#else
  __ str(*(arch::Gp *)src, arch::ptr(*(arch::Gp *)dst, *(arch::Gp *)idx));
#endif
}

void masm_str_imm64(masm_t masm, int64_t imm, masm_reg_t dst, int disp) {
#if defined(_M_X64) || defined(__x86_64__)
#else
  arch::Gp src = arch::xzr;
  if (imm != 0) {
    __ mov(arch::x17, imm);
    src = arch::x17;
  }
  auto err = __ str(src, arch::ptr(*(arch::Gp *)dst, disp));
  if (err != asmjit::Error::kOk) {
    __ mov(arch::x16, disp);
    __ str(src, arch::ptr(*(arch::Gp *)dst, arch::x16));
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
  if (imm == 0) {
    __ str(arch::xzr, arch::ptr_post(kStkReg, 8));
  } else {
    __ mov(arch::x16, imm);
    __ str(arch::x16, arch::ptr_post(kStkReg, 8));
  }
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

void masm_mul_reg32(masm_t masm, masm_reg_t dst, masm_reg_t x, masm_reg_t y) {
#if defined(_M_X64) || defined(__x86_64__)
#else
  __ smull(*(arch::Gp *)dst, (*(arch::Gp *)x).r32(), (*(arch::Gp *)y).r32());
#endif
}

void masm_sub_reg(masm_t masm, masm_reg_t dst, masm_reg_t x, masm_reg_t y) {
#if defined(_M_X64) || defined(__x86_64__)
#else
  __ sub(*(arch::Gp *)dst, *(arch::Gp *)x, *(arch::Gp *)y);
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
  if (imm > 0) {
    __ add(*(arch::Gp *)dst, *(arch::Gp *)src, imm);
  } else if (imm == 0) {
    __ mov(*(arch::Gp *)dst, *(arch::Gp *)src);
  } else {
    __ sub(*(arch::Gp *)dst, *(arch::Gp *)src, -imm);
  }
#endif
}

void masm_inc32_rtmem(masm_t masm, masm_reg_t reg) {
#if defined(_M_X64) || defined(__x86_64__)
#else
  __ ldr(arch::w17,
         arch::ptr(kHeapReg, (*(arch::Gp *)reg).r32(), arch::uxtw(0)));
  __ add(arch::w17, arch::w17, 1);
  __ str(arch::w17,
         arch::ptr(kHeapReg, (*(arch::Gp *)reg).r32(), arch::uxtw(0)));
#endif
}

void masm_dec32_rtmem(masm_t masm, masm_reg_t reg, masm_label_t skip) {
#if defined(_M_X64) || defined(__x86_64__)
#else
  assert(kArg2Reg == *(arch::Gp *)reg);
  __ ldr(arch::w2,
         arch::ptr(kHeapReg, (*(arch::Gp *)reg).r32(), arch::uxtw(0)));
  __ sub(arch::w2, arch::w2, 1);
  __ str(arch::w2,
         arch::ptr(kHeapReg, (*(arch::Gp *)reg).r32(), arch::uxtw(0)));
  if (skip != 0) {
    asmjit::Label label;
    label._base_id = (intptr_t)skip - 1;
    __ cbnz(arch::w2, label);
  }
#endif
}

void masm_zero_reg_hi32(masm_t masm, masm_reg_t reg) {
  __ mov((*(arch::Gp *)reg).r32(), (*(arch::Gp *)reg).r32());
}

void masm_sxtw(masm_t masm, masm_reg_t dst, masm_reg_t src) {
#if defined(_M_X64) || defined(__x86_64__)
#else
  __ sxtw(*(arch::Gp *)dst, (*(arch::Gp *)src).r32());
#endif
}

void masm_toNewFloat64(masm_t masm, masm_reg_t reg) {
#if defined(_M_X64) || defined(__x86_64__)
#else
  __ scvtf(arch::d0, *(arch::Gp *)reg); // d0 = (double) r
  __ mov(arch::x8, (uint64_t)JS_FLOAT64_TAG_ADDEND << 32);
  __ mov(arch::x11, 0x7ff0000000000000);
  __ fmov(arch::x9, arch::d0);  // u64 = d0
  __ and_(arch::x10, arch::x9, 0x7fffffffffffffff);
  __ sub(arch::x8, arch::x9, arch::x8);
  __ mov(arch::x9, JS_NAN);
  __ cmp(arch::x10, arch::x11);
  __ csel(*(arch::Gp *)reg, arch::x9, arch::x8, arch::CondCode::kHI);
#endif
}

void masm_orr_reg(masm_t masm, masm_reg_t dst, masm_reg_t src) {
#if defined(_M_X64) || defined(__x86_64__)
#else
  __ orr(*(arch::Gp *)dst, *(arch::Gp *)dst, *(arch::Gp *)src);
#endif
}

void masm_cmp_reg(masm_t masm, masm_reg_t x, masm_reg_t y) {
  __ cmp(*(arch::Gp *)x, *(arch::Gp *)y);
}

void masm_cmp_reg32(masm_t masm, masm_reg_t x, masm_reg_t y) {
  __ cmp((*(arch::Gp *)x).r32(), (*(arch::Gp *)y).r32());
}

void masm_cmp_reg_imm32(masm_t masm, masm_reg_t reg, int32_t imm) {
#if defined(_M_X64) || defined(__x86_64__)
  __ cmp((*(arch::Gp *)reg).r32(), imm);
#else
  if (imm == 0) {
    __ cmp((*(arch::Gp *)reg).r32(), arch::wzr);
  } else if (imm < 0) {
    auto err = __ cmn((*(arch::Gp *)reg).r32(), -imm);
    if (err != asmjit::Error::kOk) {
      __ mov(arch::x16, imm);
      __ cmp((*(arch::Gp *)reg).r32(), arch::w16);
    }
  } else {
    auto err = __ cmp((*(arch::Gp *)reg).r32(), imm);
    if (err != asmjit::Error::kOk) {
      __ mov(arch::x16, imm);
      __ cmp((*(arch::Gp *)reg).r32(), arch::w16);
    }
  }
#endif
}

void masm_cmp_reg_imm64(masm_t masm, masm_reg_t reg, int64_t imm) {
#if defined(_M_X64) || defined(__x86_64__)
  __ cmp(*(arch::Gp *)reg, imm);
#else
  if (imm == 0) {
    __ cmp(*(arch::Gp *)reg, arch::xzr);
  } else if (imm < 0) {
    auto err = __ cmn(*(arch::Gp *)reg, -imm);
    if (err != asmjit::Error::kOk) {
      __ mov(arch::x16, imm);
      __ cmp(*(arch::Gp *)reg, arch::x16);
    }
  } else {
    auto err = __ cmp(*(arch::Gp *)reg, imm);
    if (err != asmjit::Error::kOk) {
      __ mov(arch::x16, imm);
      __ cmp(*(arch::Gp *)reg, arch::x16);
    }
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

void masm_b_ls(masm_t masm, masm_label_t id) {
  asmjit::Label label;
  label._base_id = (intptr_t)id - 1;
#if defined(_M_X64) || defined(__x86_64__)
  __ jbe(label);
#else
  __ b_ls(label);
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

void masm_b_ne(masm_t masm, masm_label_t id) {
  asmjit::Label label;
  label._base_id = (intptr_t)id - 1;
#if defined(_M_X64) || defined(__x86_64__)
  __ jne(label);
#else
  __ b_ne(label);
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

void masm_b_hs(masm_t masm, masm_label_t id) {
  asmjit::Label label;
  label._base_id = (intptr_t)id - 1;
#if defined(_M_X64) || defined(__x86_64__)
  __ jae(label);
#else
  __ b_hs(label);
#endif
}

void masm_b_lt(masm_t masm, masm_label_t id) {
  asmjit::Label label;
  label._base_id = (intptr_t)id - 1;
#if defined(_M_X64) || defined(__x86_64__)
#else
  __ b_lt(label);
#endif
}

void masm_b_le(masm_t masm, masm_label_t id) {
  asmjit::Label label;
  label._base_id = (intptr_t)id - 1;
#if defined(_M_X64) || defined(__x86_64__)
#else
  __ b_le(label);
#endif
}

void masm_b_gt(masm_t masm, masm_label_t id) {
  asmjit::Label label;
  label._base_id = (intptr_t)id - 1;
#if defined(_M_X64) || defined(__x86_64__)
#else
  __ b_gt(label);
#endif
}

void masm_b_ge(masm_t masm, masm_label_t id) {
  asmjit::Label label;
  label._base_id = (intptr_t)id - 1;
#if defined(_M_X64) || defined(__x86_64__)
#else
  __ b_ge(label);
#endif
}

void masm_cset(masm_t masm, masm_reg_t dst, int cc) {
#if defined(_M_X64) || defined(__x86_64__)
#else
  switch (cc) {
  case 0: // lt
    __ cset(*(arch::Gp *)dst, arch::CondCode::kLT);
    break;
  case 1: // lte
    __ cset(*(arch::Gp *)dst, arch::CondCode::kLE);
    break;
  case 2: // gt
    __ cset(*(arch::Gp *)dst, arch::CondCode::kGT);
    break;
  case 3: // gte
    __ cset(*(arch::Gp *)dst, arch::CondCode::kGE);
    break;
  }
#endif
}

void masm_cbz(masm_t masm, masm_reg_t reg, masm_label_t id) {
  asmjit::Label label;
  label._base_id = (intptr_t)id - 1;
#if defined(_M_X64) || defined(__x86_64__)
#else
  __ cbz(*(arch::Gp *)reg, label);
#endif
}

void masm_cbz32(masm_t masm, masm_reg_t reg, masm_label_t id) {
  asmjit::Label label;
  label._base_id = (intptr_t)id - 1;
#if defined(_M_X64) || defined(__x86_64__)
#else
  __ cbz((*(arch::Gp *)reg).r32(), label);
#endif
}

void masm_cbnz(masm_t masm, masm_reg_t reg, masm_label_t id) {
  asmjit::Label label;
  label._base_id = (intptr_t)id - 1;
#if defined(_M_X64) || defined(__x86_64__)
#else
  __ cbnz(*(arch::Gp *)reg, label);
#endif
}

void masm_cbnz32(masm_t masm, masm_reg_t reg, masm_label_t id) {
  asmjit::Label label;
  label._base_id = (intptr_t)id - 1;
#if defined(_M_X64) || defined(__x86_64__)
#else
  __ cbnz((*(arch::Gp *)reg).r32(), label);
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

void masm_call(masm_t masm, masm_label_t id) {
  asmjit::Label label;
  label._base_id = (intptr_t)id - 1;
#if defined(_M_X64) || defined(__x86_64__)
  __ call(label);
#else
  __ bl(label);
#endif
}

void masm_call_stub(masm_t masm, void *stub, int save_lr) {
#if defined(_M_X64) || defined(__x86_64__)
#else
  const char *ic = nullptr;
  if (save_lr) {
    ic = __ inline_comment();
    if (ic)
      __ reset_inline_comment();
    __ str(arch::x30, arch::ptr_pre(arch::sp, -16));
  }
  if (ic)
    __ set_inline_comment(ic);
  __ mov(arch::x16, int64_t(stub));
  __ blr(arch::x16);
  if (save_lr) {
    __ ldr(arch::x30, arch::ptr_post(arch::sp, 16));
  }
#endif
}

void masm_tail_call_stub(masm_t masm, void *stub, masm_label_t cont) {
  asmjit::Label label;
  label._base_id = (intptr_t)cont - 1;
#if defined(_M_X64) || defined(__x86_64__)
#else
  __ mov(arch::x16, int64_t(stub));
  __ adr(arch::x30, label);
  __ br(arch::x16);
#endif
}

void masm_push_c_arg_imm64(masm_t masm, int arg, int64_t imm) {
#if defined(_M_X64) || defined(__x86_64__)
#else
  assert(arg >= 1 && arg <= 8);
  auto dst = arch::Gp::make_r64(arg - 1);
  if (imm == 0) {
    __ mov(dst, arch::xzr);
  } else {
    __ mov(dst, imm);
  }
#endif
}

void masm_push_c_arg_reg(masm_t masm, int arg, masm_reg_t reg) {
#if defined(_M_X64) || defined(__x86_64__)
#else
  assert(arg >= 1 && arg <= 8);
  auto dst = arch::Gp::make_r64(arg - 1);
  if (dst != *(arch::Gp *)reg) {
    __ mov(dst, *(arch::Gp *)reg);
  }
#endif
}

void masm_push_c_arg_mem(masm_t masm, int arg, masm_reg_t reg, int disp) {
#if defined(_M_X64) || defined(__x86_64__)
#else
  assert(arg >= 1 && arg <= 8);
  auto dst = arch::Gp::make_r64(arg - 1);
  __ ldr(dst, arch::ptr(*(arch::Gp *)reg, disp));
#endif
}

void masm_push_c_arg_mem2(masm_t masm, int arg, masm_reg_t reg, masm_reg_t idx,
                          int disp) {
#if defined(_M_X64) || defined(__x86_64__)
#else
  assert(arg >= 1 && arg <= 8);
#endif
}

void masm_ret(masm_t masm) {
#if defined(_M_X64) || defined(__x86_64__)
  __ ret();
#else
  __ ret(arch::x30);
#endif
}
