
#include <stdint.h>

bool check_ret_hazard(opcode_t D_opcode);
bool check_mispred_branch_hazard(opcode_t X_opcode, bool X_condval);
bool check_load_use_hazard(opcode_t D_opcode, uint8_t D_src1, uint8_t D_src2, opcode_t X_opcode, uint8_t X_dst);
void handle_hazards(opcode_t D_opcode, uint8_t D_src1, uint8_t D_src2, opcode_t X_opcode, uint8_t X_dst, bool X_condval);