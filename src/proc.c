/**************************************************************************
 * STUDENTS: DO NOT MODIFY.
 * 
 * C S 429 architecture emulator
 * 
 * proc.c - The top-level instruction processing loop of the processor.
 * 
 * Copyright (c) 2022. S. Chatterjee. All rights reserved.
 * May not be used, modified, or copied without permission.
 **************************************************************************/ 

#include "archsim.h"
#include "hw_elts.h"
#include "pipe/hazard_control.h"

#define F_insn_in guest.proc->f_insn->in
#define F_insn_out guest.proc->f_insn->out
#define D_insn_in guest.proc->d_insn->in
#define D_insn_out guest.proc->d_insn->out
#define X_insn_in guest.proc->x_insn->in
#define X_insn_out guest.proc->x_insn->out
#define M_insn_in guest.proc->m_insn->in
#define M_insn_out guest.proc->m_insn->out
#define W_insn_in guest.proc->w_insn->in
#define W_insn_out guest.proc->w_insn->out

uint64_t pred_pc;
uint64_t current_PC;

extern machine_t guest;
extern bool X_condval;

int runElf(const uint64_t entry) {
    logging(LOG_INFO, "Running ELF executable");
    guest.proc->PC.bits->xval = entry;
    guest.proc->SP.bits->xval = guest.mem->seg_start_addr[KERNEL_SEG]-8;
    guest.proc->NZCV.bits->ccval = PACK_CC(0, 1, 0, 0);
    guest.proc->GPR.bits[30].xval = RET_FROM_MAIN_ADDR;

    /* Bubble instructions for hazard handling */
    instr_impl_t *bubble_insn = calloc(1, sizeof(instr_impl_t));
    bubble_insn->op = OP_NOP;
    bubble_insn->insnbits = 0xd503201f;

    pipe_reg_t **pipes[] = {&guest.proc->f_insn, &guest.proc->d_insn, &guest.proc->x_insn,
                           &guest.proc->m_insn, &guest.proc->w_insn};

    for (int i = 0; i < 5; i++) {
        *pipes[i] = (pipe_reg_t *)calloc(1, sizeof(pipe_reg_t));
        (*pipes[i])->in = (instr_impl_t *)calloc(1, sizeof(instr_impl_t));
        (*pipes[i])->out = (instr_impl_t *)calloc(1, sizeof(instr_impl_t));
        memcpy((*pipes[i])->in, bubble_insn, sizeof(instr_impl_t));
        memcpy((*pipes[i])->out, bubble_insn, sizeof(instr_impl_t));
    }

    /* Will be selected as the first PC */
    pred_pc = guest.proc->PC.bits->xval;

#ifdef DEBUG
    printf("\n%s%s   Addr      Instr       Op  \tCond\tDest\tSrc1\tSrc2\tImmval   \t\tShift%s\n", 
           ANSI_BOLD, ANSI_COLOR_RED, ANSI_RESET);
#endif
    unsigned int num_instr = 0;
    do {
        if (!guest.proc->f_insn->out->stall) {
            guest.proc->f_insn->in->pred_PC = pred_pc;
        }
        
        /* Run each stage */
        wback_instr(guest.proc->w_insn);
        memory_instr(guest.proc->m_insn);
        execute_instr(guest.proc->x_insn);   
        decode_instr(guest.proc->d_insn);   
        fetch_instr(guest.proc->f_insn);

        /* Check for hazards and appropriately stall/bubble stages */
        uint8_t D_src1 = (D_insn_in->op == OP_MOVZ) ? 0x1F : GETBF(D_insn_in->insnbits, 5, 5);
        uint8_t D_src2 = (D_insn_in->op != OP_STUR) ? GETBF(D_insn_in->insnbits, 16, 5) : GETBF(D_insn_in->insnbits, 0, 5);
        uint8_t X_dst = X_insn_in->W_sigs.dst_sel ? 30 : X_insn_in->dst;
    
        handle_hazards(D_insn_out->op, D_src1, D_src2, X_insn_in->op, X_dst, X_condval);

        /* Print debug output */
        if(debug_level > 0)
            printf("\nPipeline state at end of cycle %d:\n", num_instr);
        show_instr(guest.proc->f_insn->out, S_FETCH, debug_level);
        show_instr(guest.proc->d_insn->out, S_DECODE, debug_level);
        show_instr(guest.proc->x_insn->out, S_EXECUTE, debug_level);
        show_instr(guest.proc->m_insn->out, S_MEMORY, debug_level);
        show_instr(guest.proc->w_insn->out, S_WBACK, debug_level);
        if(debug_level > 0)
            printf("\n\n");

        /* Stall checking */
        if (!guest.proc->f_insn->out->stall) {
            guest.proc->PC.bits->xval = pred_pc;
        }

        /* Cycle instructions */
        for (int i = 0; i < 4; i++) {
            pipe_reg_t *pipe = *pipes[i];
            /* Can only stall, bubble, or neither, not both */
            if (pipe->out->stall && pipe->out->bubble) {
                logging(LOG_ERROR, "An instruction was both bubbled and stalled.");
            }
            /* Insert bubbles if needed */
            if (pipe->out->bubble) {
                memcpy(pipe->out, bubble_insn, sizeof(instr_impl_t));
            }
            /* Copy output of one stage to the next stage's input */
            if (!pipe->out->stall) {
                memcpy((*pipes[i+1])->in, pipe->out, sizeof(instr_impl_t));
            }
        }

        /* Writeback - do we need it or is it for students? */

        num_instr++;
    } while (!(D_insn_out->op == OP_RET && D_insn_out->val_a == RET_FROM_MAIN_ADDR) && num_instr < MAX_NUM_INSTR);
    free(bubble_insn);
    return EXIT_SUCCESS;
}