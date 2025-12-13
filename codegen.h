#ifndef QUICKJS_CODEGEN_H
#define QUICKJS_CODEGEN_H

#include <stdint.h>

typedef struct masm *masm_t;
typedef struct label *masm_label_t;
typedef struct reg *masm_reg_t;

#ifdef __cplusplus
extern "C" {
#endif

extern masm_reg_t kRetReg, kArg1Reg, kArg2Reg, kArg3Reg, kArg4Reg;
extern masm_reg_t kStkReg, kArgBufReg, kVarBufReg, kHeapReg;

masm_t masm_new();
void masm_prolog(masm_t);
void masm_epilog(masm_t);
masm_label_t masm_new_label(masm_t, const char *name);
void masm_bind(masm_t, masm_label_t);
void masm_inline_comment(masm_t, const char *data);
void masm_comment(masm_t, const char *fmt, ...);
void masm_mov_reg_imm32(masm_t, masm_reg_t, int32_t);
void masm_mov_reg_imm64(masm_t, masm_reg_t, int64_t);
void masm_ldp(masm_t, masm_reg_t, masm_reg_t, masm_reg_t src, int disp);
void masm_ldr(masm_t, masm_reg_t, masm_reg_t src, int disp);
void masm_str(masm_t, masm_reg_t, masm_reg_t dst, int disp);
void masm_lsl(masm_t, masm_reg_t dst, masm_reg_t src, int bits);
void masm_lsr(masm_t, masm_reg_t dst, masm_reg_t src, int bits);
void masm_asr(masm_t, masm_reg_t dst, masm_reg_t src, int bits);
void masm_push_imm64(masm_t, int64_t);
void masm_push_reg(masm_t, masm_reg_t);
void masm_pop_reg(masm_t, masm_reg_t);
void masm_inc32_rtmem(masm_t, masm_reg_t);
void masm_add_reg(masm_t, masm_reg_t, masm_reg_t, masm_reg_t);
void masm_add_reg_imm32(masm_t, masm_reg_t, masm_reg_t, int32_t);
void masm_zero_reg_hi32(masm_t, masm_reg_t);
void masm_cmp_reg(masm_t, masm_reg_t, masm_reg_t);
void masm_cmp_reg_imm32(masm_t, masm_reg_t, int32_t);
void masm_b_lo(masm_t, masm_label_t);
void masm_b_eq(masm_t, masm_label_t);
void masm_b_hi(masm_t, masm_label_t);
void masm_jmp(masm_t, masm_label_t);
void* masm_finish(masm_t);
void masm_delete(masm_t);

#ifdef __cplusplus
}
#endif
#endif
