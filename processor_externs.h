//-----------------------------------------------------------------------------
// 
//   processor_externs.h - Data type definitions
//
//   Author:
//      Rick Stievenart - 09/07/2017
//
//   Updates:
//
//-----------------------------------------------------------------------------


// Processor memory
extern unsigned char prog_memory[MEMSIZE];		// Page 00
extern unsigned char prog2_memory[MEMSIZE];	// Page 10
extern unsigned char flash_memory[MEMSIZE];	// Flash memory

// Processor registers
extern unsigned char register_a;
extern unsigned char register_x;
extern unsigned char register_y;
extern unsigned int register_pc;
extern unsigned short register_sp;
extern unsigned char register_cc;
extern unsigned int previous_register_pc;
extern unsigned int previous_register_sp;

// for function call deliniation
extern int executed_call_instruction;
extern int executed_return_instruction;

extern unsigned int aabnormal_termination;

//
//--------------------------------------------------------
// Function prototypes
//--------------------------------------------------------
//

void display_scoreboard(void);
void clear_scoreboard(void);

void reset_processor(void);

int execute(void);