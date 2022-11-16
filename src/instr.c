/**************************************************************************
 * C S 429 architecture emulator
 * 
 * instr.c - The top-level entry point for instruction processing.
 * 
 * Most of the details will be written in files in the instr/ subdirectory.
 * 
 * Copyright (c) 2022. S. Chatterjee. All rights reserved.
 * May not be used, modified, or copied without permission.
 **************************************************************************/ 

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "err_handler.h"
#include "instr.h"
#include "machine.h"
#include "hw_elts.h"

extern machine_t guest;

// The four global variables below are signals that correspond to long wires.
extern uint64_t pred_pc; // Defined in proc.c.
extern uint64_t current_PC; // Defined in proc.c.
bool X_condval; // Generated in X. Consumed in F.
static uint64_t W_wval; // Generated in W. Consumed in D.
extern mem_status_t dmem_status;

inline unsigned safe_GETBF(int32_t src, unsigned frompos, unsigned width) {
    return ((((unsigned) src) & (((1 << width) - 1) << frompos)) >> frompos);
}

int64_t safe_GETBOFFSET(int32_t src, unsigned frompos, unsigned width) {
    return (
        (int64_t) (((((uint64_t) src) & (((1 << width) - 1) << frompos)) >> frompos) << (64-width))
        >> (64-width)
    );
}

static inline void init_itable_entry(opcode_t op, unsigned idx) {itable[idx] = op;}

static inline void init_itable_range(opcode_t op, unsigned idx1, unsigned idx2) {
    for (unsigned i = idx1; i <= idx2; i++) itable[i] = op;
}

/*
 * Initialize the itable. Called from interface.c.
 * Do not re-write.
 */

void init_itable(void) {
    for (int i = 0; i < (2<<11); i++) itable[i] = OP_ERROR;
    init_itable_entry(OP_LDURB, 0x1c2U);
    init_itable_entry(OP_LDUR, 0x7c2U);
    init_itable_entry(OP_STURB, 0x1c0U);
    init_itable_entry(OP_STUR, 0x7c0U);
    init_itable_range(OP_MOVK, 0x794U, 0x797U);
    init_itable_range(OP_MOVZ, 0x694U, 0x697U);
    init_itable_range(OP_ADD_RI, 0x488U, 0x48bU);
    init_itable_entry(OP_ADDS_RR, 0x558U);
    init_itable_entry(OP_SUBS_RR, 0x758U);
    init_itable_entry(OP_MVN, 0x551U);
    init_itable_entry(OP_ORR_RR, 0x550U);
    init_itable_entry(OP_EOR_RR, 0x650U);
    init_itable_entry(OP_ANDS_RR, 0x750U);
    init_itable_range(OP_UBFM, 0x69aU, 0x69bU); // LSL, LSR share the same opcode
    init_itable_range(OP_ASR, 0x49aU, 0x49bU);
    init_itable_range(OP_B, 0x0a0U, 0x0bfU);
    init_itable_range(OP_B_COND, 0x2a0U, 0x2a7U);
    init_itable_range(OP_BL, 0x4a0U, 0x4bfU);
    init_itable_entry(OP_RET, 0x6b2U);
    init_itable_entry(OP_NOP, 0x6a8U);
    init_itable_entry(OP_HLT, 0x6a2U);
}

/*
 * Select PC logic.
 * STUDENT TO-DO:
 * Write the next PC to current_PC.
 */

static comb_logic_t 
select_PC(uint64_t pred_PC, opcode_t X_opcode, bool X_cond_val, uint64_t seq_succ, opcode_t D_opcode, uint64_t val_a,
          uint64_t *current_PC) {
            
    if (X_opcode == OP_B_COND)
    {
        if (X_cond_val)
        {
            *current_PC = pred_PC;
        }
        else
        {
            *current_PC = seq_succ;
        }
    }

    else if (D_opcode != OP_RET)
    {
        *current_PC = pred_PC;
    }

    else
    {
        *current_PC = val_a;
    }

    
    return;
}

/*
 * Predict PC logic. Conditional branches are predicted taken.
 * STUDENT TO-DO:
 * Write the predicted next PC to predicted_PC
 * and the next sequential pc to seq_succ.
 */

static comb_logic_t 
predict_PC(uint64_t current_PC, uint32_t insnbits, opcode_t op, uint64_t *predicted_PC, uint64_t *seq_succ) {
    uint64_t pc_inc = current_PC + 4;
    if (op == OP_B || op == OP_BL || op == OP_B_COND)
    {
        if (op == OP_B_COND)
        {
            *predicted_PC = current_PC + (safe_GETBOFFSET(insnbits, 5, 19) << 2);
        }
        else
        {
            *predicted_PC = current_PC + (safe_GETBOFFSET(insnbits, 0, 26) << 2);
        }
    }
    else
    {
        *predicted_PC = pc_inc;
    }

    *seq_succ = pc_inc;
    return;
}

/*
 * Control signals for D, X, M, and W stages.
 * Generated by D stage logic.
 * D control signals are consumed locally. 
 * Others must be buffered in pipeline registers.
 * STUDENT TO-DO:
 * Generate the correct control signals for this instruction's
 * future stages and write them to the corresponding struct.
 */

static comb_logic_t 
generate_DXMW_control(opcode_t op,
                      d_ctl_sigs_t *D_sigs, x_ctl_sigs_t *X_sigs, m_ctl_sigs_t *M_sigs, w_ctl_sigs_t *W_sigs) {

    bool op_load = (op == OP_LDUR || op == OP_LDURB);
    bool op_store = (op == OP_STURB || op == OP_STUR);
    bool op_mov = (op == OP_MOVZ || op == OP_MOVK);

    M_sigs->dmem_write = op_store;
    M_sigs->dmem_read = op_load;

    W_sigs->dst_31isSP = 0;
    W_sigs->wval_sel = op_load; 
    W_sigs->dst_sel = (op == OP_BL);
    W_sigs->w_enable = (op_mov || op == OP_ADD_RI || op_load || op == OP_UBFM || op == OP_ASR ||
    op == OP_ADDS_RR|| op == OP_SUBS_RR|| op == OP_MVN|| op == OP_ORR_RR|| op == OP_EOR_RR|| op == OP_ANDS_RR);
    
    X_sigs->set_CC = (op == OP_ADDS_RR || op == OP_SUBS_RR || op == OP_ANDS_RR);
    X_sigs->valb_sel = (op == OP_MVN || op <= OP_ADD_RI || (op >= OP_LSL && op <= OP_ASR));

    D_sigs->src1_31isSP = (op_store|| op_load);
    D_sigs->src2_sel = op_store;
    D_sigs->src2_31isSP = 0; 
    return;
}


/*
 * Logic for extracting the immediate value for M-, I-, and RI-format instructions.
 * STUDENT TO-DO:
 * Extract the immediate value and write it to imm.
 */

static comb_logic_t 
extract_immval(uint32_t insnbits, int64_t *imm) {
    opcode_t op = itable[safe_GETBF(insnbits, 21, 11)];
    if (op == OP_UBFM || op == OP_ASR)
    {
        *imm = safe_GETBF(insnbits, 10, 12);
    }
    else if (op == OP_STUR || op == OP_STURB || op == OP_LDURB || op == OP_LDUR)
    {
        *imm = safe_GETBF(insnbits, 12, 9);
    }
    else if (op == OP_MOVZ || op == OP_MOVK)
    {
        *imm = safe_GETBF(insnbits, 5, 16);
    }

    return;
}

/*
 * Logic for determining the ALU operation needed for this opcode.
 * STUDENT TO-DO:
 * Determine the ALU operation based on the given opcode
 * and write it to ALU_op.
 */
static comb_logic_t
decide_alu_op(opcode_t op, alu_op_t *ALU_op) {

    

    if (op == OP_LDURB || op == OP_LDUR)
    {
        *ALU_op = PLUS_OP;
    }
    else if (op == OP_STURB || op == OP_STUR)
    {
        *ALU_op = PLUS_OP;
    }
    else if (op == OP_MOVK || op == OP_MOVZ )
    {
        *ALU_op = MOV_OP;
    }
    else if (op == OP_ADDS_RR || op == OP_ADD_RI)
    {
        *ALU_op = PLUS_OP;
    }
    else if (op == OP_SUBS_RR)
    {
        *ALU_op = MINUS_OP;
    }
    else if (op == OP_MVN)
    {
        *ALU_op = NEG_OP;
    }
    else if (op == OP_EOR_RR)
    {
        *ALU_op = EOR_OP;
    }
    else if (op == OP_ORR_RR)
    {
        *ALU_op = OR_OP;
    }
    else if (op == OP_ANDS_RR )
    {
        *ALU_op = AND_OP;
    }
    else if (op == OP_LSR)
    {
        *ALU_op = LSR_OP;
    }
    else if (op == OP_ASR)
    {
        *ALU_op = ASR_OP;
    }
    else
    {
        *ALU_op = PASS_A_OP;
    }

    return;
}



/*
 * Utility functions for copying over control signals across a stage.
 * STUDENT TO-DO:
 * Copy the input signals from the input side of the pipeline
 * register to the output side of the register.
 */

static comb_logic_t 
copy_m_ctl_sigs(pipe_reg_t *const insn) {
    insn->out->M_sigs = insn->in->M_sigs;
    return;
}

static comb_logic_t 
copy_w_ctl_sigs(pipe_reg_t *const insn) {
    insn->out->W_sigs = insn->in->W_sigs;
    return;
}

/*
 * Fetch stage logic.
 * STUDENT TO-DO:
 * Implement the fetch stage.
 * 
 * You will also need the global variables
 * current_PC, pred_pc, X_condval, and itable,
 * and helper functions
 * select_pc, predict_pc, and imem.
 */

comb_logic_t fetch_instr(pipe_reg_t *const insn) {
    bool imem_err; // Ignored, though you will need this as a filler parameter to imem.
    select_PC(insn->in->pred_PC, guest.proc->m_insn->in->op, guest.proc->m_insn->in->cond_holds, guest.proc->m_insn->in->seq_succ_PC, guest.proc->x_insn->in->op, guest.proc->x_insn->in->val_a, &current_PC);
    imem(current_PC, &insn->out->insnbits, &imem_err);
    uint64_t out_op = itable[safe_GETBF(insn->out->insnbits, 21, 11)];
    predict_PC(current_PC, insn->out->insnbits, out_op, &pred_pc, &insn->out->seq_succ_PC);
    insn->out->op = out_op;
    return;
}

/*
 * Decode stage logic.
 * STUDENT TO-DO:
 * Implement the decode stage.
 * 
 * You will need the global variable W_wval
 * and helper functions
 * generate_DXMW_control, regfile, extract_immval,
 * and decide_alu_op.
 */

comb_logic_t decode_instr(pipe_reg_t *const insn) {
    insn->out->seq_succ_PC = insn->in->seq_succ_PC;
    insn->out->op = insn->in->op;
    
    uint8_t src1 = 31;
    uint8_t src2 = 0;
    d_ctl_sigs_t D;

    

    generate_DXMW_control(insn->in->op, &D, &(insn->out->X_sigs), &(insn->out->M_sigs), &(insn->out->W_sigs));

    if (insn->in->op != OP_MOVZ)
    {
        src1 = safe_GETBF(insn->in->insnbits, 5, 5);
    }

    if (!D.src2_sel)
    {
        src2 = safe_GETBF(insn->in->insnbits, 16, 5);
    }
    else
    {
        src2 = safe_GETBF(insn->in->insnbits, 0, 5);
    }

    insn->out->dst = insn->out->W_sigs.dst_sel ? 30 : guest.proc->w_insn->in->dst;

    decide_alu_op(insn->in->op, &insn->out->ALU_op);

    regfile(src1, src2, insn->out->dst, W_wval, D.src1_31isSP, D.src2_31isSP, guest.proc->w_insn->in->W_sigs.dst_31isSP, guest.proc->w_insn->in->W_sigs.w_enable,
    &insn->out->val_a, &insn->out->val_b);

    if (insn->in->op == OP_B_COND)
    {
        insn->out->cond = GETBF(insn->in->insnbits, 0, 4);
    }
    

    extract_immval(insn->in->insnbits, &(insn->out->val_imm));
    
    insn->out->dst = safe_GETBF(insn->in->insnbits, 0, 5);
    

    insn->out->val_hw = insn->in->op == OP_MOVZ || insn->in->op == OP_MOVK ? safe_GETBF(insn->in->insnbits, 21, 2) << 4 : 0; 
    insn->out->op = insn->out->op == OP_MOVZ ? 0 : insn->out->op;

    return;
}


/*
 * Execute stage logic.
 * STUDENT TO-DO:
 * Implement the execute stage.
 * 
 * You will need the helper functions
 * copy_m_ctl_signals, copy_w_ctl_signals, and alu.
 */

comb_logic_t execute_instr(pipe_reg_t *const insn) {
    
    copy_m_ctl_sigs(insn);
    copy_w_ctl_sigs(insn);
    uint64_t alu_b = insn->in->val_b;
    if (insn->in->X_sigs.valb_sel)
    {
        alu_b = insn->in->val_imm;
    }
    alu(insn->in->val_a, alu_b, insn->in->val_hw, insn->in->ALU_op, insn->in->X_sigs.set_CC, insn->in->cond, &insn->out->val_ex, &X_condval);
    insn->out->cond_holds = X_condval;
    insn->out->dst = insn->in->dst;
    insn->out->val_b = insn->in->val_b;
    insn->out->op = insn->in->op;
    insn->out->M_sigs = insn->in->M_sigs;
    insn->out->W_sigs = insn->in->W_sigs;
    return; 
}

/*
 * Memory stage logic.
 * STUDENT TO-DO:
 * Implement the memory stage.
 * 
 * You will need the helper functions
 * copy_m_ctl_signals, copy_w_ctl_signals, and dmem.
 */

comb_logic_t memory_instr(pipe_reg_t *const insn) {
    bool dmem_err; // Ignored, though you will need this as a filler parameter to dmem.
    insn->out->val_ex = insn->in->val_ex;
    copy_m_ctl_sigs(insn);
    copy_w_ctl_sigs(insn);
    dmem(insn->in->val_ex, insn->in->val_b, insn->in->M_sigs.dmem_read, insn->in->M_sigs.dmem_write, &insn->out->val_mem, &dmem_err);
    insn->out->dst = insn->in->dst;
    insn->out->op = insn->in->op;
    
    return;
}

/*
 * Write-back stage logic.
 * STUDENT TO-DO:
 * Implement the writeback stage.
 * 
 * You will need the global variable W_wval
 * and the helper function copy_w_ctl_sigs.
 */

comb_logic_t wback_instr(pipe_reg_t *const insn) {
    
    copy_w_ctl_sigs(insn);  
    W_wval = insn->in->val_ex;
    if (insn->in->W_sigs.wval_sel)
    {
        W_wval = insn->in->val_mem;
    }
    insn->out->val_ex = insn->in->val_ex;
    insn->out->op = insn->in->op;

    return;
}


static char *opcode_names[] = {
    "ERR ", 
    "LDURB ",
    "LDUR ",
    "STURB ",
    "STUR ",
    "MOVK ",
    "MOVZ ",
    "ADD ",
    "ADDS ",
    "SUBS ",
    "MVN ",
    "ORR ",
    "EOR ",
    "ANDS ",
    "LSL ",
    "LSR ",
    "UBFM ",
    "ASR ",
    "B ",
    "B.cond ",
    "BL ",
    "RET ",
    "NOP ",
    "HLT ",
};

static char *cond_names[] = {
    "EQ", "NE", "CS", "CC", "MI", "PL", "VS", "VC", 
    "HI", "LS", "GE", "LT", "GT", "LE", "AL", "NV"
};

static char *alu_op_names[] = {
    "PLUS_OP",
    "MINUS_OP",
    "NEG_OP",
    "OR_OP",
    "EOR_OP",
    "AND_OP",
    "MOV_OP",
    "LSL_OP",
    "LSR_OP",
    "ASR_OP",
    "PASS_A_OP",
    "PASS_B_OP"
};


/*
 * A debugging aid to print out the fields of an instruction.
 * This routine is called from runElf().
 * Do not re-write.
 * 
 * The amount of detail printed is controlled by debug_level.
 *  0: No output.
 *  1: Medium output. Data signals only in the pipeline register.
 *  2: Full output. Data and control signals in the pipeline register.
 */

void show_instr(const instr_impl_t *insn, const proc_stage_t stage, int debug_level) {
    
    if(debug_level < 1) {
        return;
    }

    switch (stage) {
    case S_FETCH:
        printf("F: %-6s[PC, insn_bits] = [%08lX,  %08X], seq_succ_PC: 0x%lX, pred_PC: 0x%lX\n", 
            opcode_names[insn->op],
            guest.proc->PC.bits->xval, 
            insn->insnbits,
            insn->seq_succ_PC,
            pred_pc);

        if (debug_level == 1)
            break;
        
        // debug level 2: also print control signals
        printf(" \t [bubble, stall] = [%s, %s]\n",
            insn->bubble ? "true" : "false",
            insn->stall ? "true" : "false");
        break;
    case S_DECODE:
        printf("D: %-6s[val_a, val_b, imm] = [0x%lX, 0x%lX, 0x%lX], alu_op: %s, cond: %s, dst: X%d\n",
            opcode_names[insn->op],
            insn->val_a,
            insn->val_b,
            insn->val_imm,
            alu_op_names[insn->ALU_op],
            cond_names[insn->cond],
            insn->dst);
        if (debug_level == 1)
            break;
        
        // debug level 2: also print control signals
        printf(" \t [bubble, stall] = [%s, %s]\n",
            insn->bubble ? "true" : "false",
            insn->stall ? "true" : "false");

        printf("\t X_sigs: [valb_sel, set_CC] = [%s, %s]\n",
            insn->X_sigs.valb_sel ? "true " : "false",
            insn->X_sigs.set_CC ? "true" : "false");

        printf("\t M_sigs: [dmem_read, dmem_write] = [%s, %s]\n",
            insn->M_sigs.dmem_read ? "true " : "false",
            insn->M_sigs.dmem_write ? "true" : "false");

        printf("\t W_sigs: [dst_31isSP, dst_sel, wval_sel, w_enable] = [%s, %s, %s, %s]\n",
            insn->W_sigs.dst_31isSP ? "true " : "false",
            insn->W_sigs.dst_sel ? "true " : "false",
            insn->W_sigs.wval_sel ? "true " : "false",
            insn->W_sigs.w_enable ? "true" : "false");

        break;

        case S_EXECUTE: 
            printf("X: %-6s[val_ex, a, b, imm, hw] = [0x%lX, 0x%lX, 0x%lX, 0x%lX, 0x%X], alu_op: %s\n",
                opcode_names[insn->op],
                insn->val_ex,
                insn->val_a,
                insn->val_b,
                insn->val_imm,
                insn->val_hw,
                alu_op_names[insn->ALU_op]);
            if (debug_level == 1)
                break;
        
            // debug level 2: also print control signals
            printf(" \t [bubble, stall] = [%s, %s]\n",
                insn->bubble ? "true" : "false",
                insn->stall ? "true" : "false");

            printf("\t X_sigs: [valb_sel, set_CC] = [%s, %s]\n",
            insn->X_sigs.valb_sel ? "true " : "false",
            insn->X_sigs.set_CC ? "true" : "false");

            break;
        case S_MEMORY: 
            printf("M: %-6s[val_ex, val_b, val_mem] = [0x%lX, 0x%lX, 0x%lX]\n",
                opcode_names[insn->op],
                insn->val_ex,
                insn->val_b,
                insn->val_mem);

            if (debug_level == 1)
                break;
        
            // debug level 2: also print control signals
            printf(" \t [bubble, stall] = [%s, %s]\n",
                insn->bubble ? "true" : "false",
                insn->stall ? "true" : "false");

            printf("\t M_sigs: [dmem_read, dmem_write] = [%s, %s]\n",
                insn->M_sigs.dmem_read ? "true " : "false",
                insn->M_sigs.dmem_write ? "true" : "false");

            break;
        case S_WBACK: 
            printf("W: %-6s[dst, val_ex, val_mem] = [X%d, 0x%lX, 0x%lX]\n",
                opcode_names[insn->op],
                insn->dst,
                insn->val_ex,
                insn->val_mem);

            if (debug_level == 1)
                break;
        
            // debug level 2: also print control signals
            printf(" \t [bubble, stall] = [%s, %s]\n",
                insn->bubble ? "true" : "false",
                insn->stall ? "true" : "false");
            
            printf("\t W_sigs: [dst_31isSP, dst_sel, wval_sel, w_enable] = [%s, %s, %s, %s]\n",
                insn->W_sigs.dst_31isSP ? "true " : "false",
                insn->W_sigs.dst_sel ? "true " : "false",
                insn->W_sigs.wval_sel ? "true " : "false",
                insn->W_sigs.w_enable ? "true" : "false");

            break;
        default: IMPOSSIBLE(); break;
    }
    return;
}
