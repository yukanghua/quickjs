#ifndef QUICKJS_CODEGEN_H
#define QUICKJS_CODEGEN_H

#include <stdint.h>

typedef struct masm *masm_t;
typedef struct reg *masm_reg_t;
typedef int masm_label_t;

#ifdef __cplusplus
extern "C" {
#endif

extern masm_reg_t kRetReg, kArg1Reg, kArg2Reg, kArg3Reg, kArg4Reg, kTmpReg;
extern masm_reg_t kStkReg, kArgBufReg, kVarBufReg, kCtxReg, kSfReg, kHeapReg;

masm_t masm_new();
void masm_prolog(masm_t);
void masm_epilog(masm_t);
masm_label_t masm_new_label(masm_t, const char *name);
void masm_bind(masm_t, masm_label_t);
void masm_inline_comment(masm_t, const char *data);
void masm_comment(masm_t, const char *fmt, ...);
void masm_mov_reg(masm_t, masm_reg_t, masm_reg_t);
void masm_mov_reg_imm32(masm_t, masm_reg_t, int32_t);
void masm_mov_reg_imm64(masm_t, masm_reg_t, int64_t);
void masm_ldp(masm_t, masm_reg_t, masm_reg_t, masm_reg_t src, int disp);
void masm_ldr(masm_t, masm_reg_t, masm_reg_t src, int disp);
void masm_ldr2(masm_t, masm_reg_t, masm_reg_t src, masm_reg_t idx);
void masm_str(masm_t, masm_reg_t, masm_reg_t dst, int disp);
void masm_str2(masm_t, masm_reg_t, masm_reg_t dst, masm_reg_t idx);
void masm_str_imm64(masm_t, int64_t, masm_reg_t dst, int disp);
void masm_lsl(masm_t, masm_reg_t dst, masm_reg_t src, int bits);
void masm_lsr(masm_t, masm_reg_t dst, masm_reg_t src, int bits);
void masm_asr(masm_t, masm_reg_t dst, masm_reg_t src, int bits);
void masm_push_imm64(masm_t, int64_t);
void masm_push_reg(masm_t, masm_reg_t);
void masm_pop_reg(masm_t, masm_reg_t);
void masm_mul_reg32(masm_t, masm_reg_t, masm_reg_t, masm_reg_t);
void masm_sub_reg(masm_t, masm_reg_t, masm_reg_t, masm_reg_t);
void masm_add_reg(masm_t, masm_reg_t, masm_reg_t, masm_reg_t);
void masm_add_reg_imm32(masm_t, masm_reg_t, masm_reg_t, int32_t);
void masm_inc32_rtmem(masm_t as, masm_reg_t reg);
void masm_dec32_rtmem(masm_t as, masm_reg_t reg, masm_label_t skip);
void masm_zero_reg_hi32(masm_t, masm_reg_t);
void masm_sxtw(masm_t, masm_reg_t, masm_reg_t);
void masm_toNewFloat64(masm_t, masm_reg_t);
void masm_orr_reg(masm_t, masm_reg_t, masm_reg_t);
void masm_cmp_reg(masm_t, masm_reg_t, masm_reg_t);
void masm_cmp_reg32(masm_t, masm_reg_t, masm_reg_t);
void masm_cmp_reg_imm32(masm_t, masm_reg_t, int32_t);
void masm_cmp_reg_imm64(masm_t, masm_reg_t, int64_t);
void masm_b_eq(masm_t, masm_label_t);
void masm_b_ne(masm_t, masm_label_t);
void masm_b_lo(masm_t, masm_label_t); // unsigned less than
void masm_b_ls(masm_t, masm_label_t); // unsigned less than or equal
void masm_b_hi(masm_t, masm_label_t); // unsigned greater than
void masm_b_hs(masm_t, masm_label_t); // unsigned greater than or equal
void masm_b_lt(masm_t, masm_label_t); // signed less than
void masm_b_le(masm_t, masm_label_t); // signed less than or equal
void masm_b_gt(masm_t, masm_label_t); // signed greater than
void masm_b_ge(masm_t, masm_label_t); // signed greater than or equal
void masm_cset(masm_t, masm_reg_t, int cc);
void masm_cbz(masm_t, masm_reg_t reg, masm_label_t);
void masm_cbz32(masm_t, masm_reg_t reg, masm_label_t);
void masm_cbnz(masm_t, masm_reg_t reg, masm_label_t);
void masm_cbnz32(masm_t, masm_reg_t reg, masm_label_t);
void masm_jmp(masm_t, masm_label_t);
void masm_call(masm_t, masm_label_t);
void masm_call_stub(masm_t, void *, int save_lr);
void masm_tail_call_stub(masm_t, void *, masm_label_t);
void masm_push_c_arg_imm64(masm_t, int, int64_t);
void masm_push_c_arg_reg(masm_t, int, masm_reg_t);
void masm_push_c_arg_mem(masm_t, int, masm_reg_t, int disp);
void masm_push_c_arg_mem2(masm_t, int, masm_reg_t, masm_reg_t, int disp);
void masm_ret(masm_t);
void* masm_finish(masm_t);
void masm_delete(masm_t);

#ifdef __cplusplus
}
#endif
#endif
