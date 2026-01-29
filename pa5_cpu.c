#include "pa5_cpu.h"

/*****************************************************************************
 *                      PROVIDED UTILITY FUNCTIONS                           *
 *****************************************************************************/
// extract the immediate from an instruction
uint64_t extract_immediate(uint32_t inst)
{
    uint32_t opcode = extract_bits(inst, 6, 0);
    uint64_t imm = 0;

    switch(opcode)
    {
        case RV_OPCODE_OP: return 0;

        case RV_OPCODE_OP_IMM:
        case RV_OPCODE_LOAD:
            imm = extract_bits(inst, 31, 20);
            return imm >> 11 ? (-1ul << 11) | imm : imm;

        case RV_OPCODE_STORE:
            imm = (extract_bits(inst, 31, 25) << 5) | extract_bits(inst, 11, 7);
            return imm >> 11 ? (-1ul << 11) | imm : imm;

        case RV_OPCODE_BRANCH:
            imm = (extract_bits(inst, 31, 31) << 12) | (extract_bits(inst, 7, 7) << 11) | (extract_bits(inst, 30, 25) << 5) | (extract_bits(inst, 11, 8) << 1);
            return imm >> 12 ? (-1ul << 12) | imm : imm;

        default:
            printf("error: cannot extract immediate from instruction of unknown itype\n");
            return 0;
    }
}

// extract some range of bits from an instruction (indices are right to left)
uint16_t extract_bits(uint32_t inst, uint8_t msb_idx, uint8_t lsb_idx)
{
    return (inst >> lsb_idx) & ((1u << (msb_idx - lsb_idx + 1)) - 1);
}

/*****************************************************************************
 *                   RECOMMENDED UTILITY FUNCTIONS                           *
 *                                                                           *
 * We recommend you implement these to make the fetch() and memory() stage   *
 * cleaner. These will not be graded, but feel free to define assert-based   *
 * tests for them if you like.                                               *
 *****************************************************************************/
/*
 * use the function pointer rd_fn to read the RISC-V instruction from memory at
 * the given program counter 
 */
uint32_t read_instruction(uint64_t pc, uint8_t (*rd_fn)(uint64_t addr))
{
    uint32_t instruction = 0; //riscv instructions = 32 bit

    for (int i = 0; i < 4; i++) { //loop to read each byte
        instruction |= ((uint32_t)rd_fn(pc + i)) << (8 * i); //store in little-endian
    }

    return instruction;
}

// use the function pointer rd_fn to read the dword at the given memory address
uint64_t read_dword(uint64_t addr, uint8_t (*rd_fn)(uint64_t addr))
{
    uint64_t dword = 0; //dword = 64 bit

    for (int i = 0; i < 8; i++) { //loop to read each byte
        dword |= ((uint64_t)rd_fn(addr + i)) << (8 * i); //store in little endian
    }
    return dword;
}

// use the function pointer wr_fn to write the dword "data" to the given memory address
void write_dword(uint64_t addr, uint64_t data, void (*wr_fn)(uint64_t addr, uint8_t data))
{
    for (int i = 0; i < 8; i++) { //loop through each byte
        uint8_t byte = (data >> (8 * i)) & 0xFF; //extract i-th byte
        wr_fn(addr + i, byte); //write to memory
    }
}

/*****************************************************************************
 *                                                                           *
 *                   REQUIRED + GRADED FUNCTIONS                             *
 *                                                                           *
 *****************************************************************************/

/*****************************************************************************
 *                                   FETCH                                   *
 *****************************************************************************/
/*
 * simulates the fetch stage
 *
 * inputs:
 *     pc: the program counter for this cycle
 *     mem_read_byte: a function pointer to a function that allows reading a single byte from memory
 * outputs:
 *     out: the outputs of the fetch stage (defined in types.h)
 * assumptions:
 *     - the pc will be a multiple of 4 bytes
 */
void pa5_fetch(uint64_t pc, uint8_t (*mem_read_byte)(uint64_t addr), struct fetch_outputs *out)
{

    uint32_t instruction = read_instruction(pc, mem_read_byte); //read instruction

    out->inst = instruction; //update output instruction
    out->pc = pc; //pass on program counter


}

/*****************************************************************************
 *                                  DECODE                                   *
 *****************************************************************************/
/*
 * decode the ALU operation from the instruction
 *
 * inputs:
 *     inst: the instruction to decode
 * outputs:
 *     out: the ALU operation (enums are defined in types.h)
 */
enum ALU_OP get_aluop(uint32_t inst)
{

    //extract bits of opcode, funct3, and funct7
    uint32_t opcode = extract_bits(inst, 6, 0);
    uint32_t funct3 = extract_bits(inst, 14, 12);
    uint32_t funct7 = extract_bits(inst, 31, 25);

    if (opcode == RV_OPCODE_OP) { //if R-type
        if (funct3 == FUNCT3_ADD_SUB) {
            if (funct7 == FUNCT7_ADD) {
                return ALU_OP_ADD;
            }
            else if (funct7 == FUNCT7_SUB) {
                return ALU_OP_SUB;
            }
        }
        else if (funct3 == FUNCT3_XOR) {
            return ALU_OP_XOR;
        }
        else if (funct3 == FUNCT3_OR) {
            return ALU_OP_OR;
        }
        else if (funct3 == FUNCT3_AND) {
            return ALU_OP_AND;
        }
        else {
            return ALU_OP_NONE;
        }
    }

    else if (opcode == RV_OPCODE_OP_IMM) { //if I-type
        if (funct3 == FUNCT3_ADD_SUB) {
            return ALU_OP_ADD;
        }
        else if (funct3 == FUNCT3_XOR) {
            return ALU_OP_XOR;
        }
        else if (funct3 == FUNCT3_OR){
            return ALU_OP_OR;
        }
        else if (funct3 == FUNCT3_AND) {
            return ALU_OP_AND;
        }
        else {
            return ALU_OP_NONE;
        }
    }

    else if (opcode == RV_OPCODE_LOAD || opcode == RV_OPCODE_STORE || opcode == RV_OPCODE_BRANCH) {
        return ALU_OP_ADD;
    }

    return ALU_OP_NONE;
}

/*
 * decode the branch condition from the instruction
 *
 * inputs:
 *     inst: the instruction to decode
 * outputs:
 *     out: the branch condition (enums are defined in types.h)
 */
enum BR_COND get_br_cond(uint32_t inst)
{
    //br_cond = never for everything except branch (in this assignment)

    uint32_t opcode = extract_bits(inst, 6, 0);
    uint32_t funct3 = extract_bits(inst, 14, 12);

    if (opcode != RV_OPCODE_BRANCH) {
        return BR_COND_NEVER;
    }

    if (funct3 == FUNCT3_BEQ) {
        return BR_COND_EQ;
    }
    else if (funct3 == FUNCT3_BNE) {
        return BR_COND_NEQ;
    }
    else if (funct3 == FUNCT3_BLT) {
        return BR_COND_LT;
    }
    else if (funct3 == FUNCT3_BGE) {
        return BR_COND_GE;
    }
    else if (funct3 == FUNCT3_BLTU) {
        return BR_COND_LTU;
    }
    else if (funct3 == FUNCT3_BGEU) {
        return BR_COND_GEU;
    }

    return BR_COND_NONE;
}

/*
 * simulates the decode stage
 *
 * inputs:
 *     in: the outputs of the fetch stage (that are inputs to the decode stage)
 *     gprs: x0 - x31 (the general-purpose registers) READ-ONLY
 * outputs:
 *     out: the outputs of the decode stage (defined in types.h)
 * assumptions:
 *     - there will always be NUM_GPRS elements in the gprs array
 */
void pa5_decode(const struct fetch_outputs *in, const uint64_t *gprs, struct decode_outputs *out)
{
    
    uint32_t inst = in->inst; //get instruction from fetch output

    //extract opcode, rd, rs1, rs2
    //always in the same place
    uint32_t opcode = extract_bits(inst, 6, 0);
    uint32_t rd = extract_bits(inst, 11, 7);
    uint32_t rs1 = extract_bits(inst, 19, 15);
    uint32_t rs2 = extract_bits(inst, 24, 20);

    //extract immediate
    uint64_t imm = extract_immediate(inst);

    //get values at registers
    uint64_t rs1_value = gprs[rs1];
    uint64_t rs2_value = gprs[rs2];

    out->rd_idx = (enum GPR_IDX)rd; //cast bits of rd to a GPR index enum
    out->alu_op = get_aluop(inst);
    out->br_cond = get_br_cond(inst);
    out->rs1 = rs1_value;
    out->rs2 = rs2_value;
    out->imm = imm;
    out->pc = in->pc;

    //set rest of enum/bools to false, none, etc. as default
    out->wb_sel = WB_SEL_NONE;
    out->a_sel = A_SEL_NONE;
    out->b_sel = B_SEL_NONE;
    out->reg_write_en = false;
    out->mem_read_en = false;
    out->mem_write_en = false;

    if (opcode == RV_OPCODE_OP) { //R-type
        out->wb_sel = WB_SEL_ALU_RESULT;
        out->a_sel = A_SEL_RS1;
        out->b_sel = B_SEL_RS2;
        out->reg_write_en = true;
        out->mem_read_en = false;
        out->mem_write_en = false;
    }

    else if (opcode == RV_OPCODE_OP_IMM) { //I-type besides load
        out->wb_sel = WB_SEL_ALU_RESULT;
        out->a_sel = A_SEL_RS1;
        out->b_sel = B_SEL_IMM;
        out->reg_write_en = true;
        out->mem_read_en = false;
        out->mem_write_en = false;
    }

    else if (opcode == RV_OPCODE_LOAD) { //load
        out->wb_sel = WB_SEL_MEM_READ_DATA;
        out->a_sel = A_SEL_RS1;
        out->b_sel = B_SEL_IMM;
        out->reg_write_en = true;
        out->mem_read_en = true;
        out->mem_write_en = false;
    }

    else if (opcode == RV_OPCODE_STORE) { //S-type
        out->wb_sel = WB_SEL_NONE;
        out->a_sel = A_SEL_RS1;
        out->b_sel = B_SEL_IMM;
        out->reg_write_en = false;
        out->mem_read_en = false;
        out->mem_write_en = true;
    }

    else if (opcode == RV_OPCODE_BRANCH) { //B-type
        out->wb_sel = WB_SEL_NONE;
        out->a_sel = A_SEL_PC;
        out->b_sel = B_SEL_IMM;
        out->reg_write_en = false;
        out->mem_read_en = false;
        out->mem_write_en = false;
    }

}

/*****************************************************************************
 *                        ADDRESS GENERATION + EXECUTE                       *
 *****************************************************************************/
/*
 * simulates the agex stage
 *
 * inputs:
 *     in: the outputs of the decode stage
 * outputs:
 *     out: the outputs of the agex stage (defined in types.h)
 */
void pa5_agex(const struct decode_outputs *in, struct agex_outputs *out)
{
    
    uint64_t agex_a = 0;
    uint64_t agex_b = 0;
    uint64_t agex_result = 0;

    //get proper values of A and B from input
    if (in->a_sel == A_SEL_RS1) {
        agex_a = in->rs1;
    }
    else if (in->a_sel == A_SEL_PC) {
        agex_a = in->pc;
    }

    if (in->b_sel == B_SEL_RS2) {
        agex_b = in->rs2;
    }
    else if (in->b_sel == B_SEL_IMM) {
        agex_b = in->imm;
    }

    //possible alu operations
    if (in->alu_op == ALU_OP_ADD) {
        agex_result = agex_a + agex_b;
    }
    else if (in->alu_op == ALU_OP_SUB) {
        agex_result = agex_a - agex_b;
    }
    else if (in->alu_op == ALU_OP_XOR) {
        agex_result = agex_a ^ agex_b;
    }
    else if (in->alu_op == ALU_OP_OR) {
        agex_result = agex_a | agex_b;
    }
    else if (in->alu_op == ALU_OP_AND) {
        agex_result = agex_a & agex_b;
    }

    //pc select
    if (in->br_cond != BR_COND_NONE && in->br_cond != BR_COND_NEVER) {
        out->pc_sel = PC_SEL_ALU_RESULT;
    }
    else {
        out->pc_sel = PC_SEL_PC_PLUS_4;
    }

    //forwarded control signals
    out->rd_idx = in->rd_idx;
    out->wb_sel = in->wb_sel;
    out->reg_write_en = in->reg_write_en;
    out->mem_read_en = in->mem_read_en;
    out->mem_write_en = in->mem_write_en;

    //forwarded data
    out->alu_result = agex_result;
    out->rs2 = in->rs2;
    out->pc = in->pc;

}

/*****************************************************************************
 *                                 MEMORY                                    *
 *****************************************************************************/
/*
 * simulates the memory stage
 *
 * inputs:
 *     in: the outputs of the agex stage
 *     mem_read_byte: a function pointer to a function that allows reading a single byte from memory
 *     mem_write_byte: a function pointer to a function that allows writing a single byte to memory
 * outputs:
 *     out: the outputs of the memory stage (defined in types.h)
 * assumptions:
 *     - the memory address to access will always be a multiple of 8 bytes
 */
void pa5_mem(const struct agex_outputs *in,
    uint8_t (*mem_read_byte)(uint64_t addr),
    void (*mem_write_byte)(uint64_t addr, uint8_t data),
    struct mem_outputs *out,
    bool use_cache)
{

    (void)use_cache;
    
    //forward all data/instructions besides memory read
    out->rd_idx = in->rd_idx;
    out->wb_sel = in->wb_sel;
    out->pc_sel = in->pc_sel;
    out->reg_write_en = in->reg_write_en;

    out->alu_result = in->alu_result;
    out->pc = in->pc;

    //LOAD operation
    if (in->mem_read_en && in->wb_sel == WB_SEL_MEM_READ_DATA) {
        out->mem_rdata = read_dword(in->alu_result, mem_read_byte);
    }

    //STORE operation
    if (in->mem_write_en) {
        write_dword(in->alu_result, in->rs2, mem_write_byte);
    }
    
    


}

/*****************************************************************************
 *                               WRITEBACK                                   *
 *****************************************************************************/
/*
 * simulates the writeback stage
 *
 * inputs:
 *     in: the outputs of the memory stage
 *     gprs: x0 - x31 (the general-purpose registers)
 * outputs:
 *     out: the outputs of the writeback stage (defined in types.h)
 * assumptions: 
 *     - there will always be NUM_GPRS elements in the gprs array
 */
void pa5_writeback(const struct mem_outputs *in, uint64_t *gprs, struct writeback_outputs *out)
{

    //write to destination register
    if (in->reg_write_en) {
        
        if (in->wb_sel == WB_SEL_ALU_RESULT) {
            gprs[in->rd_idx] = in->alu_result;  //write ALU result
        } else if (in->wb_sel == WB_SEL_MEM_READ_DATA) {
            gprs[in->rd_idx] = in->mem_rdata;  //write data read from memory
        }
    }

    //update next pc
    if (in->pc_sel == PC_SEL_ALU_RESULT) {
        out->next_pc = in->alu_result;  //use ALU result for next pc
    } else {
        out->next_pc = in->pc + 4;  //update pc by 4
    }

}
