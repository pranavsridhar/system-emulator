#include "machine.h"

/* Bubble and stall checking logic.
 * STUDENT TO-DO:
 * Implement logic for hazard handling.
 * 
 * handle_hazards is called from proc.c with the appropriate
 * parameters already set, you must implement the logic for it.
 * 
 * You may optionally use the other three helper functions to 
 * make it easier to follow your logic.
 */

extern machine_t guest;

void reset_stall()
{
    guest.proc->w_insn->in->stall = 0;
    guest.proc->m_insn->in->stall = 0;
    guest.proc->x_insn->in->stall = 0;
    guest.proc->d_insn->in->stall = 0;
    guest.proc->f_insn->in->stall = 0;

    guest.proc->w_insn->out->stall = 0;
    guest.proc->m_insn->out->stall = 0;
    guest.proc->x_insn->out->stall = 0;
    guest.proc->d_insn->out->stall = 0;
    guest.proc->f_insn->out->stall = 0;
}

void reset_bubble()
{
    guest.proc->w_insn->in->bubble = 0;
    guest.proc->m_insn->in->bubble = 0;
    guest.proc->x_insn->in->bubble = 0;
    guest.proc->d_insn->in->bubble = 0;
    guest.proc->f_insn->in->bubble = 0;

    guest.proc->w_insn->out->bubble = 0;
    guest.proc->m_insn->out->bubble = 0;
    guest.proc->x_insn->out->bubble = 0;
    guest.proc->d_insn->out->bubble = 0;
    guest.proc->f_insn->out->bubble = 0;
}

comb_logic_t reset()
{
    reset_bubble();
    reset_stall();
}

bool check_load_use_hazard(opcode_t D_opcode, uint8_t D_src1, uint8_t D_src2,
                           opcode_t X_opcode, uint8_t X_dst) {
    return (X_opcode == OP_LDUR) && ((X_dst == D_src1) ||(X_dst == D_src2));
}

bool check_mispred_branch_hazard(opcode_t X_opcode, bool X_condval) {
    return (X_opcode == OP_B_COND)&& !X_condval;
}

bool check_ret_hazard(opcode_t D_opcode) {
    return D_opcode == OP_RET;
}

comb_logic_t handle_hazards(opcode_t D_opcode, uint8_t D_src1, uint8_t D_src2, 
                            opcode_t X_opcode, uint8_t X_dst, bool X_condval) {
    reset();
    if (check_load_use_hazard(D_opcode, D_src1, D_src2, X_opcode, X_dst))
    {
        guest.proc->f_insn->in->stall = 1;
        guest.proc->d_insn->in->stall = 1;
        guest.proc->x_insn->in->bubble = 1;
    }
    // reset();
    else if (check_mispred_branch_hazard(X_opcode, X_condval))
    {
        guest.proc->d_insn->in->bubble = 1;
        guest.proc->x_insn->in->bubble = 1;
    }
    // reset();
    // somethings probably wrong w this case
    else if (check_ret_hazard(D_opcode))
    {
        guest.proc->d_insn->in->bubble = 1;
    }
    return;
}

