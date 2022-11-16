
#include <stdint.h>

void forward_reg(uint8_t D_src1, uint8_t D_src2, uint8_t X_dst, uint8_t M_dst, uint8_t W_dst,
                 uint64_t X_val_ex, uint64_t M_val_ex, uint64_t M_val_mem, uint64_t W_val_ex,
                 uint64_t W_val_mem, bool M_wval_sel, bool W_wval_sel, uint64_t *val_a, uint64_t *val_b);
