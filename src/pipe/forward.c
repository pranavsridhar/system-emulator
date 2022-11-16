#include <stdbool.h>
#include "forward.h"
#include "machine.h"

extern machine_t guest;

/* STUDENT TO-DO:
 * Implement forwarding register values from
 * execute, memory, and writeback back to decode.
 */
comb_logic_t forward_reg(uint8_t D_src1, uint8_t D_src2, uint8_t X_dst, uint8_t M_dst, uint8_t W_dst,
                 uint64_t X_val_ex, uint64_t M_val_ex, uint64_t M_val_mem, uint64_t W_val_ex,
                 uint64_t W_val_mem, bool M_wval_sel, bool W_wval_sel, uint64_t *val_a, uint64_t *val_b) {
        opcode_t D_op = guest.proc->d_insn->in->op;
        opcode_t M_op = guest.proc->m_insn->in->op;
        opcode_t X_op = guest.proc->x_insn->in->op;
        opcode_t W_op = guest.proc->w_insn->in->op;

        //if ()
    return;
}
