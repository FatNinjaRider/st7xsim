//-----------------------------------------------------------------------------
// 
//   processor.h - Data type definitions
//
//   Author:
//      Rick Stievenart - 09/07/2017
//
//   Updates:
//
//-----------------------------------------------------------------------------


// Processor memory
unsigned char prog_memory[MEMSIZE];		// Page 00
unsigned char prog2_memory[MEMSIZE];	// Page 10
unsigned char flash_memory[MEMSIZE];	// Flash memory

// Processor registers
unsigned char register_a;
unsigned char register_x;
unsigned char register_y;
unsigned int register_pc;
unsigned short register_sp;
unsigned char register_cc;
unsigned int previous_register_pc;
unsigned int previous_register_sp;

// for function call deliniation
int	executed_call_instruction;
int	executed_return_instruction;

unsigned int aabnormal_termination;

// Instruction precode flags
unsigned int precode_72, precode_90, precode_91, precode_92;

#define REG_PC_LO16 (register_pc & 0x0000ffff)
#define REG_PC_HI16 (register_pc & 0xffff0000)

//
//--------------------------------------------------------
// Function prototypes
//--------------------------------------------------------
//
void reset_processor(void);

void set_flags(unsigned char reg);

// alu instruction helpers
void add(unsigned char val);
void adc(unsigned char val);
unsigned char rlc(unsigned char val);
unsigned char rrc(unsigned char val);
unsigned char sla(unsigned char val);
unsigned char sra(unsigned char val);
unsigned char srl(unsigned char val);

int execute(void);

