//-----------------------------------------------------------------------------
// 
//   simulator.h - Data type definitions
//
//   Author:
//      Rick Stievenart - 09/07/2017
//
//   Updates:
//
//-----------------------------------------------------------------------------


//
//--------------------------------------------------------
// simulator internals - breakpoint mechanism definitions
//--------------------------------------------------------
//

// Execution breakpoints
extern struct breakpoint ins_breakpoints[NUM_INS_BREAKPOINTS];

// Trigger breakpoint
extern struct breakpoint application_breakpoint;

// Data Breakpoint
extern struct breakpoint data_breakpoints[NUM_DATA_BREAKPOINTS];

//
//--------------------------------------------------------
// simulator internals - machine control and timing
//--------------------------------------------------------
//

// true if we are in run mode
extern int running;

extern int stop_reason;

extern int trace; // for selective printing

extern int step_over;

extern unsigned long instruction_count;

extern int break_on_all_calls;

// timers
extern unsigned long sim_time_ns;	// nanoseconds elapsed

extern unsigned char print_buffer[1024]; // the processor prints stuff to here

//
//--------------------------------------------------------
// simulator internals - trace execution to a file
//--------------------------------------------------------
//
extern int run_log_enable;
extern int run_log_triggered;
extern FILE *run_log_fp;

//
//--------------------------------------------------------
// Function prototypes
//--------------------------------------------------------
//

void simulator_output(void);

int run_internals(void);

void inc_sim_time(unsigned int quantity);
unsigned long get_sim_time(void);
void set_sim_time(unsigned long time);

unsigned char get_data_memory_byte_internal(unsigned int address, int rawflag);
unsigned char get_data_memory_byte_raw(unsigned int address);
unsigned char get_data_memory_byte(unsigned int address);

void put_data_memory_byte_internal(unsigned int address, unsigned char data, int rawflag);
void put_data_memory_byte_raw(unsigned int address, unsigned char data);
void put_data_memory_byte(unsigned int address, unsigned char data);

unsigned char get_prog_memory_byte(unsigned int address);

void display_registers(int pre_post_flag);
void display_data_memory(unsigned int address, unsigned short size);
void display_data_memory_to_run_log(unsigned int address, unsigned short size);