//
//---------------------------------------------------------------------------
//
// ST7x Simulator - main module
//
// Author: Rick Stievenart
//
// Genesis: 03/04/2011
//
// History:
//
// 05/04/2012 Fixed DIVIDE instruction
//				Fixed add, adc to affect HALF_CARRY_FLAG
//				Fixed BTJT	set carry wrong
//
// 09/06/2017 Some fixes and addition of new hp217 sec chip instructions
//
//----------------------------------------------------------------------------
//

#include "stdafx.h"
#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <conio.h>									// for kbhit()
#include <stdlib.h>
#include <time.h>
#include <windows.h>

#include "st7xcpu.h"
#include "st7xfio.h"

#include "breakpoints.h"

#include "processor_externs.h"

#include "application.h"

#include "st7xsim.h"

//
//--------------------------------------------------------
// simulator internals - breakpoint mechanism definitions
//--------------------------------------------------------
//

// Execution breakpoints
struct breakpoint ins_breakpoints[NUM_INS_BREAKPOINTS];
int any_ins_breakpoint_enabled;							// to speed up code

// Application Trigger/break point
struct breakpoint application_breakpoint;

// Data Breakpoint
struct breakpoint data_breakpoints[NUM_DATA_BREAKPOINTS];
int data_breakpoint_triggered_number;						// number of the data breakpoint that triggered the stop, -1 = none
int any_data_breakpoint_enabled;						// to speed up code

int in_function_call;
unsigned int in_function_call_sp;

//
//--------------------------------------------------------
// simulator internals - capture io writes to a file
//--------------------------------------------------------
//
unsigned int capture_address;
int capture_enable;
FILE *capture_fp;

//
//--------------------------------------------------------
// simulator internals - trace execution to a file
//--------------------------------------------------------
//
int run_log_enable;
int run_log_triggered;
FILE *run_log_fp;

//
//--------------------------------------------------------
// simulator internals - machine control and timing
//--------------------------------------------------------
//

// true if we are in run mode
int running;

int stop_reason;

int trace; // for selective printing

int step_over;

unsigned long instruction_count;

int break_on_all_calls;

int enable_pre_instruction_register_display;
int enable_post_instruction_register_display;

// timers
unsigned long sim_time_ns;							// nanoseconds elapsed

unsigned char print_buffer[1024]; // the processor prints stuff to here

unsigned int instruction_cycle_duration_ns;			// in nanoseconds

// forward function definitions
unsigned char get_data_memory_byte_internal(unsigned int address, int rawflag);
unsigned char get_data_memory_byte_raw(unsigned int address);
unsigned char get_data_memory_byte(unsigned int address);

void put_data_memory_byte_internal(unsigned int address, unsigned char data, int rawflag);
void put_data_memory_byte_raw(unsigned int address, unsigned char data);
void put_data_memory_byte(unsigned int address, unsigned char data);

unsigned char get_prog_memory_byte(unsigned int address);


//
//------------------------------------------------------------------------------
// Beginning of functions
//------------------------------------------------------------------------------
//


//
// called by the processor module to print stuff to whereever
//
void simulator_output(void)
{
	int stupid;

	if(trace) {
//
// for debug only
		if(print_buffer[0] == 0) { // so i can catch empty buffer in output guy
			stupid = 1;
		}
		printf((char *)print_buffer);
	
		if(run_log_enable) {
			if(run_log_triggered) {
				fprintf(run_log_fp, (char *)print_buffer);
			}
		}
	}
}

//
// Read a byte from the data memory
// 
unsigned char get_data_memory_byte_internal(unsigned int address, int rawflag)
{
	register int x;
	unsigned char application_value;

	if(!rawflag) {
		// Check for a data breakpoint
		if(any_data_breakpoint_enabled) {
			x = 0;
			while(x != NUM_DATA_BREAKPOINTS) {
				if(data_breakpoints[x].enable) {
					if((address == data_breakpoints[x].address) && (data_breakpoints[x].type & DBRK_TYPE_READ)) {
						// Break triggered
						data_breakpoints[x].triggered = 1;
						data_breakpoint_triggered_number = x;	
					}
				}
				x++;
			}
		}
	}

	//
	// call application specific so it can simulate io space peripherals
	//
	if(application_get_data_memory_byte(address, &application_value)) {
		return(application_value);
	}

	//
	// memory emulation
	//

	// just a check
//	if(((address & 0x0000ffff) >= XIO_START) && ((address & 0x0000ffff) <= XIO_END)) {
//		printf("\n*** READING FROM IO REGION: pc=%08x, address=%08x\n", register_pc, address);
//	}

	// just a check
//	if(((address & 0x0000ffff) >= IO_START) && ((address & 0x0000ffff) <= IO_END)) {
//		printf("\n*** READING FROM IO REGION: pc=%08x, address=%08x\n", register_pc, address);
//	}

	// just a check
//	if(((address & 0x0000ffff) >= RAM_START) && ((address & 0x0000ffff) <= RAM_END)) {
//		printf("\n*** READING FROM RAM REGION: pc=%08x, address=%08x\n", register_pc, address);
//	}

	// Flash
	if(((address & 0x0000ffff) >= FLASH_START) && ((address & 0x0000ffff) <= FLASH_END)) {
		// Return a byte from FLASH
//		if(!rawflag) {
//		printf("\n*** READING FROM FLASH: pc=%08x, address=%08x\n", register_pc, address);
//		}

		return(flash_memory[address]);
	}

	// ram and I/O
	if((address & 0xffff0000) == 0x00100000) {
		// page 10
		return(prog2_memory[address & 0x0000ffff]);
	} else {
		// page 00, just return the byte in the data memory
		return(prog_memory[address & 0x0000ffff]);
	}
}

//
// read a raw byte, is not subject to breakpoints
//
unsigned char get_data_memory_byte_raw(unsigned int address)
{
	return(get_data_memory_byte_internal(address, 1));
}

//
// Read a processed byte from the memory, subject to breakpoints
//
unsigned char get_data_memory_byte(unsigned int address)
{
	return(get_data_memory_byte_internal(address, 0));
}

//
// read a raw byte from "program" memory, not subject to breakpoints
// can set global abnormal termination flag
// and clear the simulations running flag
//
unsigned char get_prog_memory_byte(unsigned int address)
{
	// check
	if(((address & 0x0000ffff) >= XIO_START) && ((address & 0x0000ffff) <= XIO_END)) {
		printf("\n*** FETCHING FROM IO REGION: pc=%08x, address=%08x previous_pc=%08x\n", register_pc, address, previous_register_pc);
		running = 0;
		aabnormal_termination = 1;
		return(0);
	}

	// check
	if(((address & 0x0000ffff) >= IO_START) && ((address & 0x0000ffff) <= IO_END)) {
		printf("\n*** FETCHING FROM IO REGION: pc=%08x, address=%08x previous_pc=%08x\n", register_pc, address, previous_register_pc);
		aabnormal_termination = 1;
		running = 0;
		return(0);
	}

	// check
	if(((address & 0x0000ffff) >= RAM_START) && ((address & 0x0000ffff) <= RAM_END)) {
		printf("\n*** FETCHING FROM RAM REGION: pc=%08x, address=%08x previous_pc=%08x\n", register_pc, address, previous_register_pc);
		aabnormal_termination = 1;
		running = 0;
		return(0);
	}
	
	// flash, ram and io
	if((address & 0xffff0000) == 0x00100000) {
		// fetch from segment 1 (page 10)
		return(prog2_memory[address & 0x0000ffff]);
	} else {
		// page 00
		return(prog_memory[address & 0x0000ffff]);
	}
}

//
// Write a byte to the data memory
// Features:
//	data breakpoints
//  data capture
//  emulated peripherals
//
void put_data_memory_byte_internal(unsigned int address, unsigned char data, int rawflag)
{
	register int x;

	if(!rawflag) {
		x = 0;
		while(x != NUM_DATA_BREAKPOINTS) {
			if(data_breakpoints[x].enable) {
				if((address == data_breakpoints[x].address) && (data_breakpoints[x].type & DBRK_TYPE_WRITE)) {
					// Break triggered
					data_breakpoints[x].triggered = 1;
					data_breakpoint_triggered_number = x;
				}
			}
			x++;
		}

		// check for capture
		if(capture_enable) {
			if(address == capture_address) {
				// write the data to the file
				fprintf(capture_fp, "%02x\n", data);
			}
		}
	}

	//
	// call application specific so it can simulate io space peripherals
	//
	if(application_put_data_memory_byte(address, data)) {
		return;
	}

	//
	// simulated memory
	//
	if(((address & 0x0000ffff) >= XIO_START) && ((address & 0x0000ffff) <= XIO_END)) {
//		printf("\n*** WRITE TO XIO REGION DETECTED: pc=%08x, address=%08x, data=%02x\n", register_pc, address, data);
	}

	if(((address & 0x0000ffff) >= IO_START) && ((address & 0x0000ffff) <= IO_END)) {
//		printf("\n*** WRITE TO IO REGION DETECTED: pc=%08x, address=%08x, data=%02x\n", register_pc, address, data);
	}

	if(((address & 0x0000ffff) >= RAM_START) && ((address & 0x0000ffff) <= RAM_END)) {
//		printf("\n*** WRITE TO RAM REGION DETECTED: pc=%08x, address=%08x, data=%02x\n", register_pc, address, data);
	}

	if(((address & 0x0000ffff) >= FLASH_START) && ((address & 0x0000ffff) <= FLASH_END)) {
//		if(!rawflag) {
//			printf("\n*** WRITE TO FLASH REGION DETECTED: pc=%08x, address=%08x, data=%02x\n", register_pc, address, data);
//		}
		// put the byte in the data memory
		flash_memory[address] = data;
		return;
	}

	// catch writes to read-only space
	if((address & 0x0000ffff) >= ROM_START) {
//		if(!rawflag) {
			printf("\n*** WRITE TO READ ONLY REGION DETECTED: pc=%08x, address=%08x, data=%02x\n", register_pc, address, data);
//			return;
//		}
	}

	if((address & 0xffff0000) == 0x00100000) {
		// page 10
		// put the byte in the data memory
		prog2_memory[address & 0x0000ffff] = data;
	} else {
		// page 0
		// put the byte in the data memory
		prog_memory[address & 0x0000ffff] = data;
	}
}

//
// write a byte to data memory, not subject to breakpoints
//
void put_data_memory_byte_raw(unsigned int address, unsigned char data)
{
	put_data_memory_byte_internal(address, data, 1); // raw
}

//
// write a byte to data memory, not subject to breakpoints
//
void put_data_memory_byte(unsigned int address, unsigned char data)
{
	put_data_memory_byte_internal(address, data, 0);	// cooked
}

//
// simulation time functions
//
void reset_sim_time(void)
{
	unsigned int clock_frequency_hz;					// in hertz
	unsigned int clocks_per_instruction_cycle;			// 
	float ins_dur_f;
	double ins_dur_ns;
	float period_secs;

	// Establish simulation timebase
	clock_frequency_hz = CLOCK_FREQUENCY;
	clocks_per_instruction_cycle = CLOCKS_PER_INSTRUCTION_CYCLE;
	period_secs = (float)(1.0 / clock_frequency_hz);

	ins_dur_f = period_secs * clocks_per_instruction_cycle;	// .000000250
	ins_dur_ns = ins_dur_f * 1000000000.0; 	// 250
	instruction_cycle_duration_ns = ins_dur_ns;

	// reset timers
	sim_time_ns = 0;

	printf("*** Simulation Time Reset ***\n");
}

//
// increment the simulation clock by some number of instruction cycles
//
void inc_sim_time(unsigned int quantity)
{
	if(!quantity) {
		fprintf(stderr, "ERROR: instruction had 0 duration specified\n");
	}
	sim_time_ns += (quantity*instruction_cycle_duration_ns);
}

//
// set the simulation clock
//
void set_sim_time(unsigned long time)
{
	sim_time_ns = time;
}

//
// get the simulation clock
//

unsigned long get_sim_time(void)
{
	return(sim_time_ns);
}

//
// Reset the simulator
//
void reset_simulator(void)
{
	// Clear Ram and I/O
	memset(&prog_memory[XIO_START], 0x00, (XIO_END-XIO_START));

	data_breakpoint_triggered_number = -1;

	reset_sim_time();

	// reset application specific stuff
	application_reset();

	// load initial io values
	application_load_io_and_memory_initial_values();

	printf("*** Simulator Reset ***\n");
}

//
// Clear processor memory
//
// what about ram and io?
//
void clear_memory(void)
{
	memset(prog_memory, 0, MEMSIZE);
	memset(prog2_memory, 0, MEMSIZE);
	memset(flash_memory, 0x0, MEMSIZE);	// set flash to 0x0

	printf("*** Memory Cleared ***\n");
}

//
// Display/log processor registers
//
void display_registers(int pre_post_flag)
{
	if(pre_post_flag == POST) {

		if(trace) {
			printf("Post: ");

			if(run_log_enable) {
				if(run_log_triggered) {
					fprintf(run_log_fp, "Post: ");
				}
			}
		}
	} else if(pre_post_flag == PRE) {
		if(trace) {
			printf("\nPre: ");

			if(run_log_enable) {
				if(run_log_triggered) {
					fprintf(run_log_fp, "\nPre: ");
				}
			}
		}

	} else {
		if(trace) {
			printf("Current: ");

			if(run_log_enable) {
				if(run_log_triggered) {
					fprintf(run_log_fp, "Current: ");
				}
			}
		}
	}

	if(trace) {
		printf("PC=%08x, CC=%02x A=%02x X=%02x Y=%02x SP=%04x Simtime=%uns\n",
			register_pc,register_cc, register_a, register_x, register_y, register_sp, sim_time_ns);
		if(run_log_enable) {
			if(run_log_triggered) {
				fprintf(run_log_fp, "PC=%08x, CC=%02x A=%02x X=%02x Y=%02x SP=%04x Simtime=%uns\n",
					register_pc,register_cc, register_a, register_x, register_y, register_sp, sim_time_ns);
			}
		}
	}
}

//
// Displays processor data memory
//
void display_data_memory(unsigned int address, unsigned short size)
{
	unsigned int bytes;

	bytes = 0;

	printf("%08x: ", address);
	while(size--) {
		printf("%02x ", get_data_memory_byte_raw(address));
		bytes++;
		address++;
		if(bytes == 16) {
			printf("\n%08x: ", address);
			bytes = 0;
		}
	}
	printf("\n");
}

//
// logs processor data memory
// be sure run log file is opened!
//
void display_data_memory_to_run_log(unsigned int address, unsigned short size)
{
	unsigned int bytes;

	bytes = 0;

	fprintf(run_log_fp, "%08x: ", address);
	while(size--) {
		fprintf(run_log_fp, "%02x ", get_data_memory_byte_raw(address));
		bytes++;
		address++;
		if(bytes == 16) {
			fprintf(run_log_fp, "\n%08x: ", address);
			bytes = 0;
		}
	}
	fprintf(run_log_fp, "\n");
}

//
// Step to the next instruction
//
void step(void)
{
	// display registers before the instruction
	if(enable_pre_instruction_register_display) {
		display_registers(PRE);
	} else {
		// just print the program counter and a space
		if(trace) {
			printf("%08x: ", register_pc);
			if(run_log_enable) {
				if(run_log_triggered) {
					fprintf(run_log_fp, "%08x: ", register_pc);
				}
			}
		}
	}

	while(execute()) {	// returns 0 when full instruction is complete or abnormal termination occurs
		;
	}

	instruction_count++;

	// display registers after the instruction
	if(enable_post_instruction_register_display) {
		display_registers(POST);
	}
}

//
// run the code until breakpoint hit, user termination by keypress or abnormal event occurs
// sets stop_reason
//
int run_internals(void)
{
	int x, save_trace;
	unsigned int beginning_tick_count, ending_tick_count;

	// determine if there are any breakpoints enabled and set the global flags accordingly
	// (for code speed-up)
	any_ins_breakpoint_enabled = 0;
	x = 0;
	while(x < NUM_INS_BREAKPOINTS) {
		if(ins_breakpoints[x].enable) {
			any_ins_breakpoint_enabled = 1;
		}
		x++;
	}

	any_data_breakpoint_enabled = 0;
	x = 0;
	while(x < NUM_DATA_BREAKPOINTS) {
		if(data_breakpoints[x].enable) {
			any_data_breakpoint_enabled = 1;
		}
		x++;
	}

	// set the start time
	beginning_tick_count = GetTickCount();

	running = 1;

	instruction_count = 0;

	while(running) {	// running flag is cleared by processor module when some form of abnormal event occurs

		// if key hit, stop running
		if (_kbhit()) {
			printf("*** User Break - pc=%08x!\n", register_pc);
			stop_reason = STOP_USER_BREAK;
			break;	// go immediately to exit
		}

		// Check breakpoints
		if(any_ins_breakpoint_enabled) {
			x = 0;
			while(x < NUM_INS_BREAKPOINTS) {
				if(ins_breakpoints[x].enable) {
					if(register_pc == ins_breakpoints[x].address) {
						printf("\n*** Breakpoint #%d @ pc=%08x hit!\n", x, ins_breakpoints[x].address);
						stop_reason = STOP_INS_BREAK;
						goto run_exit; // go immediately to exit
					}
				}
				x++;
			}
		}

		//
		// Permanent trigger/breakpoints
		//

		// Aplication triggers and breakpoints
		application_triggers_and_breakpoints();

		//
		// Check for application break/trigger point
		// application trigger point is used in send command
		//
		if(application_breakpoint.enable) {
			if(register_pc == application_breakpoint.address) {

				printf("\n*** Application Break/Trigger point @ pc=%08x hit!\n", application_breakpoint.address);

				stop_reason = STOP_APPLICATION_BREAK;
				break;	// go immediately to exit
			}
		}

		// Check if data breakpoint has been triggered
		if(data_breakpoint_triggered_number != -1) {

			// reset the flag
			data_breakpoints[data_breakpoint_triggered_number].triggered = 0;

			printf("\n*** Data breakpoint @ %08x hit - pc=%08x\n", data_breakpoints[data_breakpoint_triggered_number].address, previous_register_pc);

			stop_reason = STOP_DATA_BREAK;
			data_breakpoint_triggered_number = -1;
			break;// go immediately to exit
		}

		// step ahead to next instruction
		// for "step over" feature
		if(step_over) {
			if(in_function_call) {

				step(); // executes one complete instruction wih possible precode can clear running flag on abnormal termination

				if(executed_return_instruction) {
					if(register_sp == in_function_call_sp) { // have we returned to the same stack level as when we left?
						in_function_call = 0;
						trace = save_trace;
					}
				}
			} else {
				step(); // executes one complete instruction wih possible precode can clear running flag on abnormal termination

				if(executed_call_instruction) {
					in_function_call = 1;
					if(trace) {
						printf("Trace disabled in function call, sp=%04x\n", previous_register_sp);
	
						if(run_log_enable) {
							if(run_log_triggered) {
								fprintf(run_log_fp, "Trace disabled in function call, sp=%04x\n", previous_register_sp);
							}
						}
					}
					in_function_call_sp = previous_register_sp;	// record stack level before the call
					save_trace = trace;
					trace = 0;
				}
			}
		} else {
			step(); // executes one complete instruction wih possible precode can clear running flag on abnormal termination

			if(break_on_all_calls) {
				if(executed_call_instruction) {

					printf("Call Instruction Breakpoint hit - pc=%08x\n", previous_register_pc);

					stop_reason = STOP_CALL_INS_BREAK;
					break; // go immediately to exit
				}
			}
		}
	}
run_exit:
	if(aabnormal_termination) {
		stop_reason = STOP_ABNORMAL_TERMINATION;
	}

	ending_tick_count = GetTickCount();

	printf("\n%ld Instructions Executed.\nSimulation Elapsed Time = %ldns, Elapsed Wall Clock Time=%d ticks/ms)\n\n", instruction_count, sim_time_ns, (ending_tick_count-beginning_tick_count));

	// return the reason we stopped
	return(stop_reason);	// I know it's a global
}


//
// Run the code
//
void run(void)
{
	printf("\nExecuting @ %08x...\n", register_pc);

	run_internals();		// this will stop at breakpoints or on user termination by keypress
							// stop reason is ignored here

	display_registers(CURRENT);
}

//
// Capture I/O writes to a file
//
void capture_io_writes_to_a_file(void)
{
	char filename[128];
	int c;

	printf("\n<B>egin capture\n");
	printf("<E>nd capture\n\n");
	printf("> ");

	c = getchar();
	getchar();
	c = tolower(c);

	switch(c) {
	case 'b':
		// Begin capture
		if(capture_enable) {
			printf("Already capturing %08x writes\n", capture_address);
		} else {
			printf("Filename? ");
			scanf("%s", &filename[0]);
			getchar();

			if((capture_fp = fopen(filename, "w")) == (FILE *)NULL) {
				printf("Can't open %s!\n", filename);
				return;
			}

			printf("Address? ");
			scanf("%x", &capture_address);
			getchar();

			// write a file header
			fprintf(capture_fp, "I/O - Memory Write Capture Log - capturing writes to %08x\n\n", capture_address);

			// set capture address and enable
			capture_enable = 1;

			printf("Capturing %08x writes to file: %s\n", capture_address, filename);
		}
		break;

	case 'e':
		// End capture
		if(capture_enable) {
			capture_enable = 0;
			fclose(capture_fp);
			printf("Ending Capture of %08x writes to file: %s\n", capture_address, filename);
		} else {
			printf("Capture not active.\n");
		}
		break;
	}	
}

//
// execute and log
//
void run_log(void)
{
	char filename[128];
	int c;

	printf("\n<B>egin execution log\n");
	printf("<E>nd execution log\n\n");
	printf("> ");

	c = getchar();
	getchar();
	c = tolower(c);

	switch(c) {
	case 'b':
		// Begin log
		if(run_log_enable) {
			printf("Already logging\n");
		} else {
			printf("Filename? ");
			scanf("%s", &filename[0]);
			getchar();

			if((run_log_fp = fopen(filename, "w")) == (FILE *)NULL) {
				printf("Can't open %s!\n", filename);
				return;
			}
			printf("Logging to file: %s\n", filename);
			run_log_enable = 1;
			run_log_triggered = 1;
		}
		break;

	case 'e':
		// End log
		if(run_log_enable) {
			fclose(run_log_fp);
			printf("Ending log\n");
			run_log_enable = 0;
		} else {
			printf("Not logging\n");
		}
		break;
	}	
}

//
// Edit help/menu
//
void edit_help(void)
{
	printf("<R>egister\n");
	printf("<M>emory\n");
	printf("<Q>uit\n");
	printf("<?> Help\n\n");
}

//
// Edit register
//
void edit_register(void)
{
	int c;
	unsigned char byte;

	printf("<A>\n");
	printf("<X>\n");
	printf("<Y>\n");

	printf("> ");
	c = getchar();
	getchar();
	c = tolower(c);

	switch(c) {
	case 'a':
		printf("Data? ");
		scanf("%x", &byte);
		getchar();
		register_a = byte;
		break;
	case 'x':
		printf("Data? ");
		scanf("%x", &byte);
		getchar();
		register_x = byte;
		break;
	case 'y':
		printf("Data? ");
		scanf("%x", &byte);
		getchar();
		register_y = byte;
		break;
	default:
		break;
	}
}

//
// Edit memory
//
void edit_memory(void)
{
	unsigned int address, byte;

	printf("Address? ");
	scanf("%x", &address);
	getchar();

	printf("Data? ");
	scanf("%x", &byte);
	getchar();

	put_data_memory_byte_raw(address, byte);
}

//
// Edit 
//
void edit(void)
{
	int c;

	printf("Edit Function...\n");

	edit_help();

	while(1) {
		printf("> ");
		c = getchar();
		getchar();
		c = tolower(c);

		switch(c) {
		case 'r':
			edit_register();
			break;
		case 'm':
			edit_memory();
			break;
		case 'q':
			goto l_eggs_it;
			break;
		case '?':
			edit_help();
			break;
		default:
			break;
		}
	}
l_eggs_it:
	return;
}

//
// Instruction break points
//
void instruction_breakpoint_menu(void)
{
	int c, bn;

	printf("<S>et/<C>lear/<D>isplay? ");
	c = getchar();
	getchar();
	c = tolower(c);

	// set
	if(c == 's') {
		// find next available one and use it
		for(bn = 0; bn < NUM_INS_BREAKPOINTS; bn++) {
			if(!ins_breakpoints[bn].used) {
				break;
			}
		}
		if(bn == NUM_INS_BREAKPOINTS) {
			printf("No breakpoints available\n");
		} else {
			printf("Address? ");
			scanf("%x", &ins_breakpoints[bn].address);
			getchar();

			ins_breakpoints[bn].used = 1;
			ins_breakpoints[bn].enable = 1;

			printf("Breakpoint #%d at: %08x\n", bn, ins_breakpoints[bn].address);

		}
	} else if(c == 'c') {
		// clear
getitagain1:
		printf("Breakpoint #? ");
		scanf("%d", &bn);
		getchar();

		if(bn > NUM_INS_BREAKPOINTS) {
			printf("Invalid breakpoint number\n");
			goto getitagain1;
		}

		ins_breakpoints[bn].address = 0;
		ins_breakpoints[bn].enable = 0;
		ins_breakpoints[bn].used = 0;
	} else if(c == 'd') {
		// display
		for(bn = 0; bn < NUM_INS_BREAKPOINTS; bn++) {
			if(ins_breakpoints[bn].used) {
				printf("Breakpoint #%d at: %08x\n", bn, ins_breakpoints[bn].address);
			}
		}
	}
}

//
// Data breakpoints
//
void data_breakpoint_menu(void)
{
	int c, bn;

	printf("<S>et/<C>lear/<D>isplay? ");
	c = getchar();
	getchar();
	c = tolower(c);

	if(c == 's') {
		// set
		// find next available one and use it
		for(bn = 0; bn < NUM_DATA_BREAKPOINTS; bn++) {
			if(!data_breakpoints[bn].used) {
				break;
			}
		}
		if(bn == NUM_DATA_BREAKPOINTS) {
			printf("No breakpoints available\n");
		} else {
			printf("Address? ");
			scanf("%x", &data_breakpoints[bn].address);
			getchar();

			printf("Type (R|W|B)? ");
			c = getchar();
			c = tolower(c);
			getchar();
			if(c == 'r') {
				data_breakpoints[bn].type = DBRK_TYPE_READ;
			} else if(c == 'w') {
				data_breakpoints[bn].type = DBRK_TYPE_WRITE;
			} else if(c == 'b') {
				data_breakpoints[bn].type = DBRK_TYPE_RW;
			}
			data_breakpoints[bn].enable = 1;
			data_breakpoints[bn].used = 1;


			printf("Data Breakpoint #%d at: %08x TYPE: ", bn, data_breakpoints[bn].address);
			if(data_breakpoints[bn].type & DBRK_TYPE_READ) {
				printf("R");
			}
			if(data_breakpoints[bn].type & DBRK_TYPE_WRITE) {
				printf("W");
			}
			printf("\n");

		}
	} else if(c == 'c') {
		// clear
getitagainsam:
		printf("Breakpoint #? ");
		scanf("%d", &bn);
		getchar();

		if(bn > NUM_DATA_BREAKPOINTS) {
			printf("Invalid breakpoint number\n");
			goto getitagainsam;
		}

		data_breakpoints[bn].address = 0;
		data_breakpoints[bn].type = 0;
		data_breakpoints[bn].enable = 0;
		data_breakpoints[bn].used = 0;
	} else if(c == 'd') {
		// display
		for(bn = 0; bn < NUM_DATA_BREAKPOINTS; bn++) {
			if(data_breakpoints[bn].used) {
				printf("Data Breakpoint #%d at: %08x TYPE: ", bn, data_breakpoints[bn].address);
				if(data_breakpoints[bn].type & DBRK_TYPE_READ) {
					printf("R");
				}
				if(data_breakpoints[bn].type & DBRK_TYPE_WRITE) {
					printf("W");
				}
				printf("\n");
			}
		}
	}
}

//
// break on calls menu
//
void break_on_all_calls_menu(void)
{
	int c;

	printf("<S>et/<C>lear/<D>isplay? ");
	c = getchar();
	getchar();
	c = tolower(c);
	if(c == 's') {
		break_on_all_calls = 1;
	} else if(c == 'c') {
		break_on_all_calls = 0;
	} else if(c == 'd') {
		if(break_on_all_calls) {
			printf("Break on all calls is enabled\n");
		} else {
			printf("Break on all calls is disabled\n");
		}
	}
}


//
// Main menu help/menu
//
void help(void)
{
	printf("Memory Commands:\n");
	printf("\t<A> Snapshots\n");
	printf("\tFlas<h> Load Flash Text\n");
	printf("\tFlash Load Binay <u>\n");
	printf("\t<0> Load Rom0 Text\n");
	printf("\t<W>rite binary file\n");
	printf("\tBi<n>ary file load\n");
	printf("\t<F>ile load (Motorla S-Record)\n");	
	printf("\t<C>lear Memory\n");
	printf("\t<!> load initial io values and patches\n");

	printf("\nSimulation/Processor Commands:\n");
	printf("\t<E>dit Memory/Registers\n");
	printf("\t<Z>reset processor\n");
	printf("\tSet <P>rogram Counter\n");
	printf("\tDisplay <R>egisters\n");
	printf("\tDisplay Data Memor<Y>\n");
	printf("\t<s>tep\n");
	printf("\t<S>tep Over\n");
	printf("\tE<x>ecute\n");
	printf("\t<L>og execution to file (start/stop)\n");
	printf("\tCap<t>ure I/O or memory writes to a file\n");
	printf("\t<B>reakpoints\n");
	printf("\t<#> Reset Simulation Time\n");
	printf("\t<@> Reset Instruction Scoreboard\n");
	printf("\t<$> Display Instruction Scoreboard\n");
	printf("\t<+> Set Trace Flag\n");
	printf("\t<-> Clear Trace Flag\n");
	printf("\t<(> Set Global StepOver Flag\n");
	printf("\t<)> Clear Global StepOver Flag\n");

	printf("\t<[> Enable Pre-Instruction Regiser Display\n");
	printf("\t<]> Disable Pre-Instruction Regiser Display\n");

	printf("\t<{> Enable Post-Instruction Regiser Display\n");
	printf("\t<}> Disable Post-Instruction Regiser Display\n");

	// for application user interface help text
	application_ui_help();

	printf("<Q>uit\n");
	printf("<?> Help\n\n");
}

//
// Main
//
int main(int argc, char* argv[])
{
	int c, save_trace, save_step_over;
	unsigned int address, length;
	char filename[128];

	printf("ST7/8 Microprocesor Simulator Version 03.00 (9/07/2017)\n\n");

	// Seed the random-number generator with current time so that the numbers will be different every time we run.
	srand( (unsigned)time( NULL ) );

	help();

	// Clear memory
	clear_memory();

	// Reset the simulator
	reset_simulator();

	// Reset the processor
	reset_processor();

	step_over = 0;
	trace = 1;
	break_on_all_calls = 0;
	
	enable_pre_instruction_register_display = 1;
	enable_post_instruction_register_display = 1;

	// process commands
	while(1) {

		printf("> ");

		c = getchar();
		getchar();

		if(application_ui_parse(c)) {
			;
		} else {

		switch(c) {
		case 'a':
			snapshot();
			break;

		case 'h':
			load_flash_text();
			break;

		case '0':
			load_rom_text();
			break;

		case 'w':
			printf("Filename? ");
			scanf("%s", &filename[0]);
			getchar();

			save_bin(filename);
			break;

		case 't':
			capture_io_writes_to_a_file();
			break;

		case 'e':
			edit();
			break;

		case 'f':
			load_srec();
			break;

		case 'n':
			printf("Filename? ");
			scanf("%s", &filename[0]);
			getchar();

			load_bin(filename);
			break;

		case '?':
			help();
			break;

		case 'q':
			goto eggsit;
			break;

		case 'x':
			run();
			break;

		case 'X':
			run_log();
			break;

		case 'z':
			reset_simulator();
			reset_processor();
			break;

		case 'p':
			printf("Address? ");
			scanf("%x", &register_pc);
			getchar();
			break;

		case 'r':
			save_trace = trace;
			trace = 1;
			display_registers(CURRENT);
			trace = save_trace;
			break;

		case 'c':
			clear_memory();
			break;

		case 's':
			// step
			save_trace = trace;
			trace = 1;
			save_step_over = step_over;
			step_over = 0;

			step();

			step_over = save_step_over;
			trace = save_trace;
			break;

		case 'S':
			// step over
			save_trace = trace;
			trace = 1;
			save_step_over = step_over;
			step_over = 1;

			step();

			step_over = save_step_over;
			trace = save_trace;
			break;

		case 'y':
			printf("Address? ");
			scanf("%x", &address);
			printf("Length? ");
			scanf("%d", &length);
			getchar();

			display_data_memory(address, length);
			break;

		case 'b':
			printf("<I>nstruction/<D>ata/<C>alls? ");
			c = getchar();
			getchar();
			c = tolower(c);
			if(c == 'i') {
				instruction_breakpoint_menu();
			} else if(c == 'd') {
				data_breakpoint_menu();
			} else if(c == 'c') {
				break_on_all_calls_menu();
			}
			break;

		case 'u':
			printf("Filename? ");
			scanf("%s", &filename[0]);
			getchar();
			load_flash(filename);
			break;

		case '#':
			reset_sim_time();
			break;

		case '$':
			display_scoreboard();
			break;

		case '@':
			clear_scoreboard();
			break;

		case '+':
			trace = 1;
			break;

		case '-':
			trace = 0;
			break;

		case '(':
			step_over = 1;
			break;

		case ')':
			step_over = 0;
			break;

		case '[':
			enable_pre_instruction_register_display = 1;
			break;

		case ']':
			enable_pre_instruction_register_display = 0;
			break;

		case '{':
			enable_post_instruction_register_display = 1;
			break;

		case '}':
			enable_post_instruction_register_display = 0;
			break;

		default:
			break;
		}
		}
	}

eggsit:
	// cleanup anything that might need it
	if(capture_enable) {
		fclose(capture_fp);
	}
	if(run_log_enable) {
		fclose(run_log_fp);
	}
	// that's all folks
	exit(0);
}

