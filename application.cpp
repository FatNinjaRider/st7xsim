//
//---------------------------------------------------------------------------
//
// ST7x Simulator - application code specific functions
//
// Author: Rick Stievenart
//
// Genesis: 09/8/2017
//
// History:
//
//----------------------------------------------------------------------------
//

#define DEBUG_MACS 1

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

// for target program
#include "types.h"
#include "hptag.h"
#include "aes_ian.h"
#include "aes_cmac.h"

#include "utils.h"
#include "debug.h"

#include "processor_externs.h"

#include "simulator.h"

#include "st7xsim.h"

//
//--------------------------------------------------------
// simulator internals - io simulation mechanism definitions
//--------------------------------------------------------
//
// for simulated peripheral - crc generator
unsigned short crc_generator_output;
unsigned int crc_generator_output_count;

//
//--------------------------------------------------------
// simulator internals - io simulation mechanism definitions
//--------------------------------------------------------
//

// For simulating hardware at 3d00,3d01,3d02
struct hpair {
	unsigned char h3d01_val;
	unsigned char h3d02_val;
};

struct hpair hpairs[] = {
	{ 0x94,0x1B },
	{ 0xF3,0x34 },
	{ 0x61,0xF6 }
};

unsigned int hindex = 0;

//------------------------------------------------------------------
// simulator internals - crc table used for simulated crc generator
//------------------------------------------------------------------
//

const unsigned short crc16tab[256] = {
			0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
			0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7, 
			0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e, 
			0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876, 
			0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd, 
			0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5, 
			0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c, 
			0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974, 
			0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb, 
			0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3, 
			0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
			0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72, 
			0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9, 
			0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1, 
			0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738, 
			0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70, 
			0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7, 
			0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff, 
			0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036, 
			 0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e, 
			 0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5, 
			 0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd, 
			 0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134, 
			 0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c, 
			 0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3, 
			 0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb, 
			 0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232, 
			 0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a, 
			 0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1, 
			 0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9, 
			 0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330, 
			 0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78, 
};

//
//--------------------------------------------------------
// target program specifics
//--------------------------------------------------------
//

unsigned char command_sequence_number = 0;

unsigned char buf[256];
unsigned char G_session_key5[16];
unsigned char G_tags_session_key5[16];
unsigned char G_InPacketLength0;
unsigned char G_InPacketLength1;

// for mac test
unsigned short rx_count;
unsigned short tx_count;
unsigned char rxbuf[256];
unsigned char txbuf[256];
unsigned char jet_status;
uint8 inbound_MAC[16];				// buffer used by GenerateMAC to hold the MAC
uint8 MAC[4];				// buffer used by GenerateMAC to hold the MAC
uint8 prev_MAC[16];					// buffer used to hold input packet MAC

void generate_prev_mac(uint8 *input, uint32 length, uint8 cmd_byte, uint8 *mac);
void generate_inbound_mac(uint8 *input, uint32 length, uint8 cmd_byte, uint8 *mac);
void generate_mac(uint8 *input, uint32 length, uint8 status_byte, uint8 *prev, uint8 *mac);

//
// this displays the hp252 tag codes "do loop" variables
//
void display_do_loop_vars(void)
{
	printf("DoLoopInput: %02x,%02x,%02x", prog_memory[0x33], prog_memory[0x34], prog_memory[0x35]);
	printf(" DoLoopLength: %02x,%02x", prog_memory[0x39], prog_memory[0x3a]);

	printf(" DoLoopOutput: %02x,%02x,%02x\n\n", prog_memory[0x36], prog_memory[0x37], prog_memory[0x38]);
}

//
// this logs the hp252 tag codes "do loop" variables
//
void display_do_loop_vars_to_run_log(void)
{
	fprintf(run_log_fp, "DoLoopInput: %02x,%02x,%02x", prog_memory[0x33], prog_memory[0x34], prog_memory[0x35]);
	fprintf(run_log_fp, " DoLoopLength: %02x,%02x", prog_memory[0x39], prog_memory[0x3a]);

	fprintf(run_log_fp, " DoLoopOutput: %02x,%02x,%02x\n\n", prog_memory[0x36], prog_memory[0x37], prog_memory[0x38]);
}

//
// called by simulator to reset any of our variables
//
void application_reset(void)
{
	// rseset crc generator emulated peripheral
	crc_generator_output = 0xffff;
	crc_generator_output_count = 0;
}

//
// Load io registers and memory with initial values
//
// this can be used to patch memory
//
void application_load_io_and_memory_initial_values(void)
{
	// Io values
	prog_memory[0x0000] = 0x00;	// I2C bit bang

	// extended IO values

	// Flash values

	// Program memory values
	prog_memory[0x8031] = RET;		// patch a return instruction in here to simply bypass the 0x3d00-0x3d02 checker
	prog2_memory[0xbaf8] = RETF;	// patch a return instruction in here to simply bypass the 0x3d00-0x3d02 checker

	prog_memory[0x4d18] = RET; // patch write command so it simply returns - (saves time in multiple command simulations)

	// patch out random reg6 check in doloopstep
	prog_memory[0x929a] = NOP;
	prog_memory[0x929b] = NOP;
	prog_memory[0x929c] = NOP;

	// try patching out the funky crc check at the end of the auth command
	prog_memory[0x658e] = JRA;
	prog_memory[0x6599] = JRA;

	printf("*** Application Initial Values loaded ***\n");
}

//
// This is a simulated peripheral, a crc16 generator
// this updates the running crc16
//
unsigned short generate_crc(unsigned char c, unsigned short oldcrc)
{
     return(crc16tab[(oldcrc ^ c) & 0xff] ^ (oldcrc >> 8));
}

//
// called by simulator to all us to emulate I/O space peripheral reads
//
// return 1 if we intercepted the i/o operation, 0 otherwise
//
int application_get_data_memory_byte(unsigned int address, unsigned char *data)
{
	unsigned short crcval;

	//
	// simulated peripherals
	//

	if(address == 0x4) {
//		printf("\n*** READING FROM reg 4: pc=%08x, address=%08x\n", register_pc, address);
		*data = 0x00;
		return(1);
	}

	// random number generator emulation
	if(address == 0x7) {
//		printf("\n*** READING FROM RND GENERATOR: pc=%08x, address=%08x\n", register_pc, address);
		*data = (rand() & 0xff);
		return(1);
	}

	// random number generator emulation
	if(address == 0xa) {
//		printf("\n*** READING FROM reg a: pc=%08x, address=%08x\n", register_pc, address);
		*data = 0;
		return(1);
	}

	// CRC generator output
	if(address == 0x0f) {
//		printf("\n*** READING FROM CRC GENERATOR: pc=%08x, address=%08x\n", register_pc, address);
		if(crc_generator_output_count == 0) {
			// out put the MSB
			crc_generator_output_count++;			// next time output the LSB
			crcval = ~crc_generator_output>>8; // MSB (the generator output is modified in the write to this peripheral
			*data = (unsigned char)crcval;
			return(1);
		} else {
			// output the LSB
			crc_generator_output_count = 0;			// next time output the MSB
			crcval = ~(crc_generator_output&0xff); //LSB
			crc_generator_output = 0xffff;			// reset the output
			*data = (unsigned char)crcval;
			return(1);
		}
	}

	if(address == 0x3d01) {
//		printf("\n*** READING FROM 3d01: pc=%08x, address=%08x, hindex=%02x\n", register_pc, address, hindex);
		*data = hpairs[hindex].h3d01_val;	// hindex must be <=3
		return(1);
	}
	if(address == 0x3d02) {
//		printf("\n*** READING FROM 3d02: pc=%08x, address=%08x, hindex=%02x\n", register_pc, address, hindex);
		*data = hpairs[hindex].h3d02_val;	// hindex must be <=3
		return(1);
	}

	if(address == 0x3d04) {
//		printf("\n*** READING FROM 3d04: pc=%08x, address=%08x\n", register_pc, address);
		*data = 0x00;
		return(1);
	}
	if(address == 0x3d05) {
//		printf("\n*** READING FROM 3d05: pc=%08x, address=%08x\n", register_pc, address);
		*data = 0x00;
		return(1);
	}
	return(0); // we didn't intercept the i/o operation
}


//
// called by simulator to allow us to emulate I/O space peripheral writes
//
// return 1 if we intercepted the i/o operation, 0 otherwise
//
int application_put_data_memory_byte(unsigned int address, unsigned char data)
{
	//
	// simulated peripherals
	//
	if(address == 0x0f) {
		// writes to the crc generator
//		printf("\n*** WRITE TO CRC GENERATOR DETECTED: pc=%08x, data=%02x\n", register_pc, data);

		crc_generator_output = generate_crc(data, crc_generator_output);
		return(1);
	}

	if(address == 0x3d00) {
//		printf("\n*** WRITE TO 3d00 DETECTED: pc=%08x, data=%02x\n", register_pc, data);
		hindex = data; // hindex must be <=3
		return(1);
	}
	return(0);
}

//
// called by simulator in function run_internals
// to allow us to do things
//
void application_triggers_and_breakpoints(void)
{
		if(register_pc == 0x5b42) {
			printf("\n*** Generate MAC(5) entry - PERMANENT Breakpoint @ pc=%08x hit, previous_pc=%08x\n", 0x5b42, previous_register_pc);
			if(run_log_enable) {
				fprintf(run_log_fp, "\n*** Generate MAC(5) entry - PERMANENT Breakpoint @ pc=%08x hit, previous_pc=%08x\n", 0x5b42, previous_register_pc);

				run_log_triggered = 1;
			}
		}

		if(register_pc == 0x5b24) {
			printf("\n*** Generate MAC(2 or 4) entry - PERMANENT Breakpoint @ pc=%08x hit, previous_pc=%08x\n", 0x5b24, previous_register_pc);
			if(run_log_enable) {
				fprintf(run_log_fp, "\n*** Generate MAC(2 or 4) entry - PERMANENT Breakpoint @ pc=%08x hit, previous_pc=%08x\n", 0x5b24, previous_register_pc);

				run_log_triggered = 1;
			}
		}

		if(register_pc == 0x5dc5) {
			printf("\n*** Generate MAC(4) exit - PERMANENT Breakpoint @ pc=%08x hit, previous_pc=%08x\n", 0x5dc5, previous_register_pc);
			if(run_log_enable) {
				fprintf(run_log_fp, "\n*** Generate MAC(4) exit - PERMANENT Breakpoint @ pc=%08x hit, previous_pc=%08x\n", 0x5dc5, previous_register_pc);

				run_log_triggered = 0;
			}
		}

		if(register_pc == 0x5dc0) {
			printf("\n*** Generate MAC(2) exit - PERMANENT Breakpoint @ pc=%08x hit, previous_pc=%08x\n", 0x5dc0, previous_register_pc);
			if(run_log_enable) {
				printf("run_log_fp, \n*** Generate MAC(2) exit - PERMANENT Breakpoint @ pc=%08x hit, previous_pc=%08x\n", 0x5dc0, previous_register_pc);

				run_log_triggered = 0;
			}
		}

		if(register_pc == 0x5b36) {
			printf("\n*** Generate MAC(1) entry - PERMANENT Breakpoint @ pc=%08x hit, previous_pc=%08x\n", 0x5b36, previous_register_pc);
			if(run_log_enable) {
				fprintf(run_log_fp, "\n*** Generate MAC(1) entry - PERMANENT Breakpoint @ pc=%08x hit, previous_pc=%08x\n", 0x5b36, previous_register_pc);

				run_log_triggered = 1;

				G_InPacketLength0 = get_data_memory_byte_raw(0x20E);
				G_InPacketLength1 = get_data_memory_byte_raw(0x20D);

				fprintf(run_log_fp, "G_InPacketLength0 = %d\n", G_InPacketLength0);

				fprintf(run_log_fp, "fe (packet): ");
				display_data_memory_to_run_log(0xfe, G_InPacketLength0);
			}
		}

		if(register_pc == 0x5ebf) {
			printf("\n*** Generate MAC exit - PERMANENT Breakpoint @ pc=%08x hit, previous_pc=%08x\n", 0x5ebf, previous_register_pc);
			if(run_log_enable) {
				fprintf(run_log_fp, "\n*** Generate MAC exit - PERMANENT Breakpoint @ pc=%08x hit, previous_pc=%08x\n", 0x5ebf, previous_register_pc);

				fprintf(run_log_fp, "251: ");
				display_data_memory_to_run_log(0x251, 16);

				run_log_triggered = 0;
			}
		}

		// UnhandledException
		if(register_pc == 0x99e2) {
			printf("\n*** PERMANENT Breakpoint (Unhandled Exception) @ pc=%08x hit, previous_pc=%08x\n", 0x99e2, previous_register_pc);
			stop_reason = STOP_INS_BREAK;
			return;// go immediately to exit
		}

		// ThrowC5
		if(register_pc == 0x9a90) {
			printf("\n*** PERMANENT Breakpoint (ThrowC5) @ pc=%08x hit, previous_pc=%08x\n", 0x9a90, previous_register_pc);
			stop_reason = STOP_INS_BREAK;
			return;// go immediately to exit
		}

		if(register_pc == 0x7d42) {
			printf("\n*** DoAesEncrypt entry PERMANENT Breakpoint @ pc=%08x hit, previous_pc=%08x\n", 0x7d42, previous_register_pc);
			if(run_log_triggered) {
				fprintf(run_log_fp, "\n*** DoAesEncrypt entry PERMANENT Breakpoint @ pc=%08x hit, previous_pc=%08x\n", 0x7d42, previous_register_pc);

				display_do_loop_vars();
				display_do_loop_vars_to_run_log();

				// display input, and output to the do loop
				fprintf(run_log_fp, "fc: ");
				display_data_memory_to_run_log(0xfc, 24);

				fprintf(run_log_fp, "221: ");
				display_data_memory_to_run_log(0x221, 16);

				fprintf(run_log_fp, "231: ");
				display_data_memory_to_run_log(0x231, 16);

				fprintf(run_log_fp, "251: ");
				display_data_memory_to_run_log(0x251, 16);
			}
		}
		if(register_pc == 0x7dc2) {
			printf("\n*** DoAesEncrypt exit PERMANENT Breakpoint @ pc=%08x hit, previous_pc=%08x\n", 0x7dc2, previous_register_pc);
			if(run_log_triggered) {
				fprintf(run_log_fp, "\n*** DoAesEncrypt exit PERMANENT Breakpoint @ pc=%08x hit, previous_pc=%08x\n", 0x7dc2, previous_register_pc);

				display_do_loop_vars();
				display_do_loop_vars_to_run_log();

				// display input, and output to the do loop
				fprintf(run_log_fp, "fc: ");
				display_data_memory_to_run_log(0xfc, 24);

				fprintf(run_log_fp, "221: ");
				display_data_memory_to_run_log(0x221, 16);

				fprintf(run_log_fp, "231: ");
				display_data_memory_to_run_log(0x231, 16);

				fprintf(run_log_fp, "251: ");
				display_data_memory_to_run_log(0x251, 16);
			}
		}

/*
		if(register_pc == 0x5cca) {
			printf("\n*** clear rest of buffer branch PERMANENT Breakpoint @ pc=%08x hit, previous_pc=%08x\n", 0x5cca, previous_register_pc);
			if(run_log_triggered) {
				fprintf(run_log_fp, "\n*** clear rest of buffer branch PERMANENT Breakpoint @ pc=%08x hit, previous_pc=%08x\n", 0x5cca, previous_register_pc);

				// display input, and the do loop vars
				fprintf(run_log_fp, "fc: ");
				display_data_memory_to_run_log(0xfc, 16);

				fprintf(run_log_fp, "221: ");
				display_data_memory_to_run_log(0x221, 16);
			}
		}

		if(register_pc == 0x5cfa) {
			printf("\n*** clear rest of buffer memset call PERMANENT Breakpoint @ pc=%08x hit, previous_pc=%08x\n", 0x5cfa, previous_register_pc);
			if(run_log_triggered) {
				fprintf(run_log_fp, "\n*** clear rest of buffer memset call PERMANENT Breakpoint @ pc=%08x hit, previous_pc=%08x\n", 0x5cfa, previous_register_pc);

				display_do_loop_vars();
				display_do_loop_vars_to_run_log();

				// display input, and the do loop vars
				fprintf(run_log_fp, "221: ");
				display_data_memory_to_run_log(0x221, 16);
			}
		}

		if(register_pc == 0x5cfd) {
			printf("\n*** clear rest of buffer memset exit PERMANENT Breakpoint @ pc=%08x hit, previous_pc=%08x\n", 0x5cfd, previous_register_pc);
			if(run_log_triggered) {
				fprintf(run_log_fp, "\n*** clear rest of buffer memset exit PERMANENT Breakpoint @ pc=%08x hit, previous_pc=%08x\n", 0x5cfd, previous_register_pc);

				display_do_loop_vars();
				display_do_loop_vars_to_run_log();

				// display input, and the do loop vars
				fprintf(run_log_fp, "221: ");
				display_data_memory_to_run_log(0x221, 16);
			}
		}
*/

		if(register_pc == 0x5d7c) {
			printf("\n*** memcpy at 5d7c PERMANENT Breakpoint @ pc=%08x hit, previous_pc=%08x\n", 0x5d7c, previous_register_pc);
			if(run_log_triggered) {
				fprintf(run_log_fp, "\n*** memcpy at 5d7c PERMANENT Breakpoint @ pc=%08x hit, previous_pc=%08x\n", 0x5d7c, previous_register_pc);

				display_do_loop_vars();
				display_do_loop_vars_to_run_log();

				// display input, and the do loop vars
				fprintf(run_log_fp, "fc: ");
				display_data_memory_to_run_log(0xfc, 24);

				fprintf(run_log_fp, "221: ");
				display_data_memory_to_run_log(0x221, 16);
				fprintf(run_log_fp, "231: ");
				display_data_memory_to_run_log(0x231, 16);
			}
		}

		if(register_pc == 0x5d80) {
			printf("\n*** after memcpy at 5d7c PERMANENT Breakpoint @ pc=%08x hit, previous_pc=%08x\n", 0x5d80, previous_register_pc);
			if(run_log_triggered) {
				fprintf(run_log_fp, "\n*** after memcpy at 5d7c PERMANENT Breakpoint @ pc=%08x hit, previous_pc=%08x\n", 0x5d80, previous_register_pc);

				display_do_loop_vars();
				display_do_loop_vars_to_run_log();

				// display input, and the do loop vars
				fprintf(run_log_fp, "fc: ");
				display_data_memory_to_run_log(0xfc, 24);

				fprintf(run_log_fp, "221: ");
				display_data_memory_to_run_log(0x221, 16);
				fprintf(run_log_fp, "231: ");
				display_data_memory_to_run_log(0x231, 16);
			}
		}

		// session key and sub keys printing
		if(register_pc == 0x5ba8) {
			printf("\n*** AesKeyExpansion exit PERMANENT Breakpoint @ pc=%08x hit, previous_pc=%08x\n", 0x5ba8, previous_register_pc);
			if(run_log_triggered) {
				fprintf(run_log_fp, "\n*** AesKeyExpansion exit PERMANENT Breakpoint @ pc=%08x hit, previous_pc=%08x\n", 0x5ba8, previous_register_pc);


				fprintf(run_log_fp, "284 (sessionkey5): ");
				display_data_memory_to_run_log(0x284, 16);

				fprintf(run_log_fp, "618 (AesTmp0): ");
				display_data_memory_to_run_log(618, 16);

				fprintf(run_log_fp, "628 (AesTmp1): ");
				display_data_memory_to_run_log(628, 16);

				fprintf(run_log_fp, "638 (AesTmp2): ");
				display_data_memory_to_run_log(0x638, 16);
			}
		}
}

//
// display relevant information from the tags memory
//
void tag_information(void)
{
	printf("Tag Information:\n\n");

	printf("Flash I2C Address: %02x\n", get_data_memory_byte_raw(0xc6ca));

	printf("FlashSerialDriverMode: %02x\n", get_data_memory_byte_raw(0xc6cc));

	printf("FlashLockState: %02x\n", get_data_memory_byte_raw(0xc160));

	printf("FlashSecretKeyScrambleDisable: %02x\n", get_data_memory_byte_raw(0xc161));

	printf("FlashSecretKey1:");
	display_data_memory(0xc163, 16);

	printf("FlashSecretKeyF1:");
	display_data_memory(0xc174, 16);

	printf("FlashSecretKeySeed1:");
	display_data_memory(0xc185, 8);
	printf("FlashSecretKeySeed1CRC: %02x\n", get_data_memory_byte_raw(0xc184));

	printf("FlashSecretKey2:");
	display_data_memory(0xc196, 16);

	printf("FlashSecretKeyF2:");
	display_data_memory(0xc1a7, 16);

	printf("FlashSecretKeySeed2:");
	display_data_memory(0xc1b8, 8);
	printf("FlashSecretKeySeedCRC: %02x\n", get_data_memory_byte_raw(0xc1b7));

	printf("FlashAuthCounter1: ");
	display_data_memory(0xc191, 3);

	printf("FlashAuthCounter2: ");
	display_data_memory(0xc1c4, 3);

	printf("\nRam Information:\n");

	printf("AuthCounter:");
	display_data_memory(0x29f, 3);

	printf("LockState=%02x\n",get_data_memory_byte_raw(0x2e0));

	printf("CmdSeqNum=%02x\n",get_data_memory_byte_raw(0x2dc));

	printf("AuthMode=%02x\n",get_data_memory_byte_raw(0x2de));
	printf("AuthState=%02x\n",get_data_memory_byte_raw(0x2dd));

	printf("SessionKey05Valid=%02x\n",get_data_memory_byte_raw(0x283));
	printf("SelectedSessionKey=%02x\n",get_data_memory_byte_raw(0x294));
	printf("SessionKey05:");
	display_data_memory(0x284, 16);

	printf("SecretKeyValid=%02x\n",get_data_memory_byte_raw(0x2a4));
	printf("SelectedSecretKey=%02x\n",get_data_memory_byte_raw(0x2a3));

	printf("SecretKey0:");
	display_data_memory(0x295, 16);

	printf("SecretKeyF:");
	display_data_memory(0x2b6, 16);

	printf("AesScrambledSboxValid=%02x\n",get_data_memory_byte_raw(0x324));
	printf("AesRoundKeysValid=%02x\n",get_data_memory_byte_raw(0x323));

/*
	RJS fix this to test run log enabled
	 printf("Aes stuff: \n");

	fprintf(run_log_fp, "fc: ");
	display_data_memory_to_run_log(0xfc, 16);

	fprintf(run_log_fp, "221: ");
	display_data_memory_to_run_log(0x221, 16);

	fprintf(run_log_fp, "231: ");
	display_data_memory_to_run_log(0x231, 16);

	fprintf(run_log_fp, "251: ");
	display_data_memory_to_run_log(0x251, 16);

	printf("\n\n");
*/
}

//
// send a command to the tag and get a response
//
int send_command(void)
{
	printf("\nSending command...");

	register_pc = 0x0010baa3;	// start execution at exit from jet driver, command assumed to be in correct place

	// set a trigger point to let us know when command is done
	application_breakpoint.address = 0x0010ba4f; // set our application break/trigger point in the jet driver wait loop which will be hit upon completeion of command
	application_breakpoint.enable = 1;

	// send it on it's way
	run_internals();

	if(stop_reason == STOP_APPLICATION_BREAK) {
		// our trigger stopped it

		// display/check results
		printf("Command returned status: %02x, length=%d,%d\n", get_data_memory_byte_raw(0x00000fb), get_data_memory_byte_raw(0x000000fc), get_data_memory_byte_raw(0x000000fd)); 

		if((get_data_memory_byte_raw(0xfb) & STATUS_MASK) != 0x00) {
			return(0); // bad status does not return data
		}
		if(get_data_memory_byte_raw(0x000000fd) != 0) {
			// show returned data
			printf("Returned data:\n");
			display_data_memory(0x000000fe, get_data_memory_byte_raw(0x000000fd));
			printf("\n");
		} else {
			printf("\n");
			application_breakpoint.enable = 0;
			return(1);
		}
	} else {
		// any other stop condition (instruction breakpoints, call breakpoint, data breakpoints or abnormal termination), return failure
		application_breakpoint.enable = 0;
		display_registers(CURRENT);	
		return(0);
	}

	application_breakpoint.enable = 0;

	// return success
	return(1);
}

//
// Load inbound message into I2c Rec buffer, and the execute code until stop trigger
//
void load_inbound_message(void)
{
	int c, x, len;
	unsigned char chips_mac[4];

	printf("<s>tandard initialze (ECHO, aka DEADBEEF)\n");
	printf("<A>uth\n");
	printf("<G>et info\n");
	printf("<W>rite (good)\n");
	printf("<B>rite (bad)\n");
	printf("<C> Write (one bigger)\n");
	printf("<1> Write (good+16)\n");
	printf("<2> Write (bad+16)\n");
	printf("<3> Write (one bigger+16)\n");
	printf("<t> try all sizes\n");
	printf("<I>nbound MAC test\n");

	printf("cmd > ");
	c = getchar();
	getchar();

	switch(c) {

	case 'G':
		// get info

		prog_memory[0xfa] = CMD_GET_INFO;	// DrvCmdByte
		prog_memory[0xfc] = 0x00;		// DrvPacketLength1 length=0
		prog_memory[0xfd] = 0x01;		// DrvPacketLength0 length=8
		prog_memory[0xfe] = 0x02;		// packet 

		if(!send_command()) {
			printf("Send command failed.\n");
		}
		break;

	case 'A':
		// get info
/*
		prog_memory[0xfa] = CMD_GET_INFO;	// DrvCmdByte
		prog_memory[0xfc] = 0x00;		// DrvPacketLength1 length=0
		prog_memory[0xfd] = 0x01;		// DrvPacketLength0 length=8
		prog_memory[0xfe] = 0x02;		// packet 
		prog_memory[0xff] = 0x00;
		if(!send_command()) {
			printf("Send command failed.\n");
		}
*/
		// AUTH
		// 03 01 ad 24 62 68 
		prog_memory[0xfa] = CMD_AUTH;	// DrvCmdByte
		prog_memory[0xfc] = 0x00;		// DrvPacketLength1 length=0
		prog_memory[0xfd] = 0x05;		// DrvPacketLength0 length=8
		prog_memory[0xfe] = 0x01;		// packet auth mode 1
		prog_memory[0xff] = 0xad;		// 4 byte random number
		prog_memory[0x100] = 0x24;
		prog_memory[0x101] = 0x62;
		prog_memory[0x102] = 0x68;

		if(!send_command()) {
			printf("Send command failed.\n");
			break;
		}

		command_sequence_number = 0;

		// get the session key from the tag here
		mymemset(G_session_key5, 0x00, 16);

		G_session_key5[0] = get_data_memory_byte_raw(0x284);
		G_session_key5[1] = get_data_memory_byte_raw(0x285);
		G_session_key5[2] = get_data_memory_byte_raw(0x286);
		G_session_key5[3] = get_data_memory_byte_raw(0x287);
		G_session_key5[4] = get_data_memory_byte_raw(0x288);
		G_session_key5[5] = get_data_memory_byte_raw(0x289);
		G_session_key5[6] = get_data_memory_byte_raw(0x28a);
		G_session_key5[7] = get_data_memory_byte_raw(0x28b);
		G_session_key5[8] = get_data_memory_byte_raw(0x28c);
		G_session_key5[9] = get_data_memory_byte_raw(0x28d);
		G_session_key5[10] = get_data_memory_byte_raw(0x28e);
		G_session_key5[11] = get_data_memory_byte_raw(0x28f);
		G_session_key5[12] = get_data_memory_byte_raw(0x290);
		G_session_key5[13] = get_data_memory_byte_raw(0x291);
		G_session_key5[14] = get_data_memory_byte_raw(0x292);
		G_session_key5[15] = get_data_memory_byte_raw(0x293);

		print_txt("session key=");
		print_hex(G_session_key5, 16);

		break;

	case 'W':

		// write (good)
		// 46 00 01 00 00 00 a3 44 6e 2f 09 61 51 dd

		// clear 64 bytyes of input packet memory
		for(x=0xfa; x < 0x13a; x++) {
			prog_memory[x] = 0x00;
		}

		prog_memory[0xfa] = CMD_WRITE | JET_CMD_BIT6;	// DrvCmdByte
		prog_memory[0xfc] = 0x00;		// DrvPacketLength1 length=0
		prog_memory[0xfd] = 13;			// DrvPacketLength0 length=8
		prog_memory[0xfe] = 0x00;		// packet 
		prog_memory[0xff] = 0x01;
		prog_memory[0x100] = 0x00;
		prog_memory[0x101] = 0x00;
		prog_memory[0x102] = 0x00;
		prog_memory[0x103] = 0xa3;
		prog_memory[0x104] = 0x44;
		prog_memory[0x105] = 0x6e;
		prog_memory[0x106] = 0x2f;
		prog_memory[0x107] = 0x09;
		prog_memory[0x108] = 0x61;
		prog_memory[0x109] = 0x51;
		prog_memory[0x10a] = 0xdd;

		if(!send_command()) {
			printf("Send command failed.\n");
			break;
		} else {

			// get chips MAC
			for(x=0; x<4; x++) {
				chips_mac[x] = get_data_memory_byte_raw(0x000000fe+x);
			}

			mymemset(prev_MAC, 0, 16);
			mymemset(rxbuf, 0x00, 256);

			rxbuf[0] = 0x00;		// packet 
			rxbuf[1] = 0x01;
			rxbuf[2] = 0x00;
			rxbuf[3] = 0x00;
			rxbuf[4] = 0x00;
			rxbuf[5] = 0xa3;
			rxbuf[6] = 0x44;
			rxbuf[7] = 0x6e;
			rxbuf[8] = 0x2f;
			rxbuf[9] = 0x09;
			rxbuf[10] = 0x61;
			rxbuf[11] = 0x51;
			rxbuf[12] = 0xdd;

			generate_prev_mac(rxbuf, 13, (CMD_WRITE | JET_CMD_BIT6), prev_MAC);

			print_txt("prev_MAC=");
			print_hex(prev_MAC, 16);

			txbuf[0] = 0x00;
			generate_mac(txbuf, 0, 0x40, prev_MAC, MAC);

			print_txt("MAC=");
			print_hex(MAC, 4);

			print_txt("Chips MAC=");
			print_hex(chips_mac, 4);

			// compare our mac and the chips mac
			if(mymemcmp(MAC, chips_mac, 4)) {
				printf("MACS DID NOT MATCH\n");
			} else {
				printf("MACS MATCH\n");
			}

		}

		break;

	case 'B':

		// write (bad)
		// 46 00 01 00 00 00 a3 44 6e 2f 09 61 51 dd 11

		// clear 64 bytyes of input packet memory
		for(x=0xfa; x < 0x13a; x++) {
			prog_memory[x] = 0x00;
		}

		prog_memory[0xfa] = CMD_WRITE | JET_CMD_BIT6;	// DrvCmdByte
		prog_memory[0xfc] = 0x00;		// DrvPacketLength1 length=0
		prog_memory[0xfd] = 14;		// DrvPacketLength0 length=8
		prog_memory[0xfe] = 0x00;		// packet 
		prog_memory[0xff] = 0x01;
		prog_memory[0x100] = 0x00;
		prog_memory[0x101] = 0x00;
		prog_memory[0x102] = 0x00;
		prog_memory[0x103] = 0xa3;
		prog_memory[0x104] = 0x44;
		prog_memory[0x105] = 0x6e;
		prog_memory[0x106] = 0x2f;
		prog_memory[0x107] = 0x09;
		prog_memory[0x108] = 0x61;
		prog_memory[0x109] = 0x51;
		prog_memory[0x10a] = 0xdd;
		prog_memory[0x10b] = 0x11;

		if(!send_command()) {
			printf("Send command failed.\n");
		} else {

			// get chips MAC
			for(x=0; x<4; x++) {
				chips_mac[x] = get_data_memory_byte_raw(0x000000fe+x);
			}

			mymemset(prev_MAC, 0, 16);
			mymemset(rxbuf, 0x00, 256);

			rxbuf[0] = 0x00;		// packet 
			rxbuf[1] = 0x01;
			rxbuf[2] = 0x00;
			rxbuf[3] = 0x00;
			rxbuf[4] = 0x00;
			rxbuf[5] = 0xa3;
			rxbuf[6] = 0x44;
			rxbuf[7] = 0x6e;
			rxbuf[8] = 0x2f;
			rxbuf[9] = 0x09;
			rxbuf[10] = 0x61;
			rxbuf[11] = 0x51;
			rxbuf[12] = 0xdd;
			rxbuf[13] = 0x11;

			generate_prev_mac(rxbuf, 14, (CMD_WRITE | JET_CMD_BIT6), prev_MAC);

			print_txt("prev_MAC=");
			print_hex(prev_MAC, 16);

			txbuf[0] = 0x00;
			generate_mac(txbuf, 0, 0x40, prev_MAC, MAC);

			print_txt("MAC=");
			print_hex(MAC, 4);

			print_txt("Chips MAC=");
			print_hex(chips_mac, 4);

			// compare our mac and the chips mac
			if(mymemcmp(MAC, chips_mac, 4)) {
				printf("MACS DID NOT MATCH\n");
			} else {
				printf("MACS MATCH\n");
			}
		}		

		break;

	case 'C':

		// write (good - bigger)
		// 46 00 01 00 00 00 a3 44 6e 2f 09 61 51 dd 11 22

		// clear 64 bytyes of input packet memory
		for(x=0xfa; x < 0x13a; x++) {
			prog_memory[x] = 0x00;
		}

		prog_memory[0xfa] = CMD_WRITE | JET_CMD_BIT6;	// DrvCmdByte
		prog_memory[0xfc] = 0x00;		// DrvPacketLength1 length=0
		prog_memory[0xfd] = 15;		// DrvPacketLength0 length=8
		prog_memory[0xfe] = 0x00;		// packet 
		prog_memory[0xff] = 0x01;
		prog_memory[0x100] = 0x00;
		prog_memory[0x101] = 0x00;
		prog_memory[0x102] = 0x00;
		prog_memory[0x103] = 0xa3;
		prog_memory[0x104] = 0x44;
		prog_memory[0x105] = 0x6e;
		prog_memory[0x106] = 0x2f;
		prog_memory[0x107] = 0x09;
		prog_memory[0x108] = 0x61;
		prog_memory[0x109] = 0x51;
		prog_memory[0x10a] = 0xdd;
		prog_memory[0x10b] = 0x11;
		prog_memory[0x10c] = 0x22;

		if(!send_command()) {
			printf("Send command failed.\n");
		} else {
			// get chips MAC
			for(x=0; x<4; x++) {
				chips_mac[x] = get_data_memory_byte_raw(0x000000fe+x);
			}

			mymemset(prev_MAC, 0, 16);
			mymemset(rxbuf, 0x00, 256);

			rxbuf[0] = 0x00;		// packet 
			rxbuf[1] = 0x01;
			rxbuf[2] = 0x00;
			rxbuf[3] = 0x00;
			rxbuf[4] = 0x00;
			rxbuf[5] = 0xa3;
			rxbuf[6] = 0x44;
			rxbuf[7] = 0x6e;
			rxbuf[8] = 0x2f;
			rxbuf[9] = 0x09;
			rxbuf[10] = 0x61;
			rxbuf[11] = 0x51;
			rxbuf[12] = 0xdd;
			rxbuf[13] = 0x11;
			rxbuf[14] = 0x22;

			generate_prev_mac(rxbuf, 15, (CMD_WRITE | JET_CMD_BIT6), prev_MAC);

			print_txt("prev_MAC=");
			print_hex(prev_MAC, 16);

			txbuf[0] = 0x00;
			generate_mac(txbuf, 0, 0x40, prev_MAC, MAC);

			print_txt("MAC=");
			print_hex(MAC, 4);

			print_txt("Chips MAC=");
			print_hex(chips_mac, 4);

			if(mymemcmp(MAC, chips_mac, 4)) {
				printf("MACS DID NOT MATCH\n");
			} else {
				printf("MACS MATCH\n");
			}
		}
		break;

	case '1':

		// write (good)
		// 46 00 01 00 00 00 a3 44 6e 2f 09 61 51 dd

		// clear 64 bytyes of input packet memory
		for(x=0xfa; x < 0x13a; x++) {
			prog_memory[x] = 0x00;
		}

		prog_memory[0xfa] = CMD_WRITE | JET_CMD_BIT6;	// DrvCmdByte
		prog_memory[0xfc] = 0x00;		// DrvPacketLength1 length=0
		prog_memory[0xfd] = 13+16;			// DrvPacketLength0 length=8
		prog_memory[0xfe] = 0x00;		// packet 
		prog_memory[0xff] = 0x01;
		prog_memory[0x100] = 0x00;
		prog_memory[0x101] = 0x00;
		prog_memory[0x102] = 0x00;
		prog_memory[0x103] = 0xa3;
		prog_memory[0x104] = 0x44;
		prog_memory[0x105] = 0x6e;
		prog_memory[0x106] = 0x2f;
		prog_memory[0x107] = 0x09;
		prog_memory[0x108] = 0x61;
		prog_memory[0x109] = 0x51;
		prog_memory[0x10a] = 0xdd;

		if(!send_command()) {
			printf("Send command failed.\n");
		} else {
			// get chips MAC
			for(x=0; x<4; x++) {
				chips_mac[x] = get_data_memory_byte_raw(0x000000fe+x);
			}

			mymemset(prev_MAC, 0, 16);
			mymemset(rxbuf, 0x00, 256);

			rxbuf[0] = 0x00;		// packet 
			rxbuf[1] = 0x01;
			rxbuf[2] = 0x00;
			rxbuf[3] = 0x00;
			rxbuf[4] = 0x00;
			rxbuf[5] = 0xa3;
			rxbuf[6] = 0x44;
			rxbuf[7] = 0x6e;
			rxbuf[8] = 0x2f;
			rxbuf[9] = 0x09;
			rxbuf[10] = 0x61;
			rxbuf[11] = 0x51;
			rxbuf[12] = 0xdd;

			generate_prev_mac(rxbuf, 13+16, (CMD_WRITE | JET_CMD_BIT6), prev_MAC);

			print_txt("prev_MAC=");
			print_hex(prev_MAC, 16);

			txbuf[0] = 0x00;
			generate_mac(txbuf, 0, 0x40, prev_MAC, MAC);

			print_txt("MAC=");
			print_hex(MAC, 4);

			print_txt("Chips MAC=");
			print_hex(chips_mac, 4);

			if(mymemcmp(MAC, chips_mac, 4)) {
				printf("MACS DID NOT MATCH\n");
			} else {
				printf("MACS MATCH\n");
			}
		}
		break;

	case '2':

		// write (bad)
		// 46 00 01 00 00 00 a3 44 6e 2f 09 61 51 dd 11

		// clear 64 bytyes of input packet memory
		for(x=0xfa; x < 0x13a; x++) {
			prog_memory[x] = 0x00;
		}


		prog_memory[0xfa] = CMD_WRITE | JET_CMD_BIT6;	// DrvCmdByte
		prog_memory[0xfc] = 0x00;		// DrvPacketLength1 length=0
		prog_memory[0xfd] = 14+16;		// DrvPacketLength0 length=8
		prog_memory[0xfe] = 0x00;		// packet 
		prog_memory[0xff] = 0x01;
		prog_memory[0x100] = 0x00;
		prog_memory[0x101] = 0x00;
		prog_memory[0x102] = 0x00;
		prog_memory[0x103] = 0xa3;
		prog_memory[0x104] = 0x44;
		prog_memory[0x105] = 0x6e;
		prog_memory[0x106] = 0x2f;
		prog_memory[0x107] = 0x09;
		prog_memory[0x108] = 0x61;
		prog_memory[0x109] = 0x51;
		prog_memory[0x10a] = 0xdd;
		prog_memory[0x10b] = 0x11;

		if(!send_command()) {
			printf("Send command failed.\n");
		} else {
			// get chips MAC
			for(x=0; x<4; x++) {
				chips_mac[x] = get_data_memory_byte_raw(0x000000fe+x);
			}

			mymemset(prev_MAC, 0, 16);
			mymemset(rxbuf, 0x00, 256);

			rxbuf[0] = 0x00;		// packet 
			rxbuf[1] = 0x01;
			rxbuf[2] = 0x00;
			rxbuf[3] = 0x00;
			rxbuf[4] = 0x00;
			rxbuf[5] = 0xa3;
			rxbuf[6] = 0x44;
			rxbuf[7] = 0x6e;
			rxbuf[8] = 0x2f;
			rxbuf[9] = 0x09;
			rxbuf[10] = 0x61;
			rxbuf[11] = 0x51;
			rxbuf[12] = 0xdd;
			rxbuf[13] = 0x11;

			generate_prev_mac(rxbuf, 14+16, (CMD_WRITE | JET_CMD_BIT6), prev_MAC);

			print_txt("prev_MAC=");
			print_hex(prev_MAC, 16);

			txbuf[0] = 0x00;
			generate_mac(txbuf, 0, 0x40, prev_MAC, MAC);

			print_txt("MAC=");
			print_hex(MAC, 4);

			print_txt("Chips MAC=");
			print_hex(chips_mac, 4);

			if(mymemcmp(MAC, chips_mac, 4)) {
				printf("MACS DID NOT MATCH\n");
			} else {
				printf("MACS MATCH\n");
			}
		}
		break;

	case '3':

		// write (good - bigger)
		// 46 00 01 00 00 00 a3 44 6e 2f 09 61 51 dd 11 22

		// clear 64 bytyes of input packet memory
		for(x=0xfa; x < 0x13a; x++) {
			prog_memory[x] = 0x00;
		}

		prog_memory[0xfa] = CMD_WRITE | JET_CMD_BIT6;	// DrvCmdByte
		prog_memory[0xfc] = 0x00;		// DrvPacketLength1 length=0
		prog_memory[0xfd] = 15+16;		// DrvPacketLength0 length=8
		prog_memory[0xfe] = 0x00;		// packet 
		prog_memory[0xff] = 0x01;
		prog_memory[0x100] = 0x00;
		prog_memory[0x101] = 0x00;
		prog_memory[0x102] = 0x00;
		prog_memory[0x103] = 0xa3;
		prog_memory[0x104] = 0x44;
		prog_memory[0x105] = 0x6e;
		prog_memory[0x106] = 0x2f;
		prog_memory[0x107] = 0x09;
		prog_memory[0x108] = 0x61;
		prog_memory[0x109] = 0x51;
		prog_memory[0x10a] = 0xdd;
		prog_memory[0x10b] = 0x11;
		prog_memory[0x10c] = 0x22;

		if(!send_command()) {
			printf("Send command failed.\n");
		} else {
			// get chips MAC
			for(x=0; x<4; x++) {
				chips_mac[x] = get_data_memory_byte_raw(0x000000fe+x);
			}

			mymemset(prev_MAC, 0, 16);
			mymemset(rxbuf, 0x00, 256);

			rxbuf[0] = 0x00;		// packet 
			rxbuf[1] = 0x01;
			rxbuf[2] = 0x00;
			rxbuf[3] = 0x00;
			rxbuf[4] = 0x00;
			rxbuf[5] = 0xa3;
			rxbuf[6] = 0x44;
			rxbuf[7] = 0x6e;
			rxbuf[8] = 0x2f;
			rxbuf[9] = 0x09;
			rxbuf[10] = 0x61;
			rxbuf[11] = 0x51;
			rxbuf[12] = 0xdd;
			rxbuf[13] = 0x11;
			rxbuf[14] = 0x22;

			generate_prev_mac(rxbuf, 15+16, (CMD_WRITE | JET_CMD_BIT6), prev_MAC);

			print_txt("prev_MAC=");
			print_hex(prev_MAC, 16);

			txbuf[0] = 0x00;
			generate_mac(txbuf, 0, 0x40, prev_MAC, MAC);

			print_txt("MAC=");
			print_hex(MAC, 4);

			print_txt("Chips MAC=");
			print_hex(chips_mac, 4);

			if(mymemcmp(MAC, chips_mac, 4)) {
				printf("MACS DID NOT MATCH\n");
			} else {
				printf("MACS MATCH\n");
			}
		}
		break;

	case 't':

		printf("Trying every length command with outbouind MAC...\n");

		// try every size message
		for(len=6;len < 188; len++) {

			printf("\nTrying length=%d...\n", len);

			for(x=0xfa; x < 0x1fd; x++) {
				prog_memory[x] = 0x00;
			}

			prog_memory[0xfa] = CMD_WRITE | JET_CMD_BIT6;	// DrvCmdByte
			prog_memory[0xfc] = 0x00;		// DrvPacketLength1 length=0
			prog_memory[0xfd] = len;			// DrvPacketLength0 length=8
			prog_memory[0xfe] = 0x00;		// packet 
			prog_memory[0xff] = 0x01;
			prog_memory[0x100] = 0x00;
			prog_memory[0x101] = 0x00;
			prog_memory[0x102] = 0x00;
			prog_memory[0x103] = 0xa3;
			prog_memory[0x104] = 0x44;
			prog_memory[0x105] = 0x6e;
			prog_memory[0x106] = 0x2f;
			prog_memory[0x107] = 0x09;
			prog_memory[0x108] = 0x61;
			prog_memory[0x109] = 0x51;
			prog_memory[0x10a] = 0xdd;

			if(!send_command()) {
				printf("Send command failed.\n");
				break;

			} else {

				// get chips MAC
				for(x=0; x<4; x++) {
					chips_mac[x] = get_data_memory_byte_raw(0x000000fe+x);
				}

				mymemset(prev_MAC, 0, 16);
				mymemset(rxbuf, 0x00, 256);

				rxbuf[0] = 0x00;		// packet 
				rxbuf[1] = 0x01;
				rxbuf[2] = 0x00;
				rxbuf[3] = 0x00;
				rxbuf[4] = 0x00;
				rxbuf[5] = 0xa3;
				rxbuf[6] = 0x44;
				rxbuf[7] = 0x6e;
				rxbuf[8] = 0x2f;
				rxbuf[9] = 0x09;
				rxbuf[10] = 0x61;
				rxbuf[11] = 0x51;
				rxbuf[12] = 0xdd;

				generate_prev_mac(rxbuf, len, (CMD_WRITE | JET_CMD_BIT6), prev_MAC);

				print_txt("prev_MAC=");
				print_hex(prev_MAC, 16);

				txbuf[0] = 0x00;
				generate_mac(txbuf, 0, 0x40, prev_MAC, MAC);

				print_txt("MAC=");
				print_hex(MAC, 4);

				print_txt("Chips MAC=");
				print_hex(chips_mac, 4);

				// compare our mac and the chips mac
				if(mymemcmp(MAC, chips_mac, 4)) {
					printf("MACS DID NOT MATCH\n");
					break;
				} else {
					printf("MACS MATCH\n");
				}
			}
		}
		if(len == 188) {
			printf("\n\n**** ALL TESTS PASSED ****\n");
		}
		break;

	case 'I':
			print_txt("session key=");
			print_hex(G_session_key5, 16);

			// tags session key
			G_tags_session_key5[0] = get_data_memory_byte_raw(0x284);
			G_tags_session_key5[1] = get_data_memory_byte_raw(0x285);
			G_tags_session_key5[2] = get_data_memory_byte_raw(0x286);
			G_tags_session_key5[3] = get_data_memory_byte_raw(0x287);
			G_tags_session_key5[4] = get_data_memory_byte_raw(0x288);
			G_tags_session_key5[5] = get_data_memory_byte_raw(0x289);	
			G_tags_session_key5[6] = get_data_memory_byte_raw(0x28a);
			G_tags_session_key5[7] = get_data_memory_byte_raw(0x28b);
			G_tags_session_key5[8] = get_data_memory_byte_raw(0x28c);
			G_tags_session_key5[9] = get_data_memory_byte_raw(0x28d);
			G_tags_session_key5[10] = get_data_memory_byte_raw(0x28e);
			G_tags_session_key5[11] = get_data_memory_byte_raw(0x28f);
			G_tags_session_key5[12] = get_data_memory_byte_raw(0x290);
			G_tags_session_key5[13] = get_data_memory_byte_raw(0x291);
			G_tags_session_key5[14] = get_data_memory_byte_raw(0x292);
			G_tags_session_key5[15] = get_data_memory_byte_raw(0x293);

			print_txt("tags session key=");
			print_hex(G_tags_session_key5, 16);

			len = 14;

			for(x=0xfa; x < 0x1fd; x++) {
				prog_memory[x] = 0x00;
			}

			prog_memory[0xfa] = CMD_WRITE | JET_CMD_BIT7 | JET_CMD_BIT6;	// DrvCmdByte
			prog_memory[0xfc] = 0x00;						// DrvPacketLength1 length=0
			prog_memory[0xfd] = len+4;						// DrvPacketLength0 length=8
			prog_memory[0xfe] = 0x00;						// packet 
			prog_memory[0xff] = 0x01;
			prog_memory[0x100] = 0x00;
			prog_memory[0x101] = 0x00;
			prog_memory[0x102] = 0x00;
			prog_memory[0x103] = 0xa3;
			prog_memory[0x104] = 0x44;
			prog_memory[0x105] = 0x6e;
			prog_memory[0x106] = 0x2f;
			prog_memory[0x107] = 0x09;
			prog_memory[0x108] = 0x61;
			prog_memory[0x109] = 0x51;
			prog_memory[0x10a] = 0xdd;
			prog_memory[0x10b] = 0x33;

			// generate MAC on this packet
			mymemset(txbuf, 0, 256);

			txbuf[0] = 0x00;		// packet 
			txbuf[1] = 0x01;
			txbuf[2] = 0x00;
			txbuf[3] = 0x00;
			txbuf[4] = 0x00;
			txbuf[5] = 0xa3;
			txbuf[6] = 0x44;
			txbuf[7] = 0x6e;
			txbuf[8] = 0x2f;
			txbuf[9] = 0x09;
			txbuf[10] = 0x61;
			txbuf[11] = 0x51;
			txbuf[12] = 0xdd;
			txbuf[13] = 0x33;

			generate_inbound_mac(txbuf, len, (CMD_WRITE | JET_CMD_BIT7 | JET_CMD_BIT6), inbound_MAC);

			// append the calculated MAC onto the packet being sent
			for(x=0; x<4; x++) {
				prog_memory[0xfe+len+x] = inbound_MAC[x];
			}

			if(!send_command()) {
				printf("Send command failed.\n");
				break;

			} else {

				// get chips MAC
				for(x=0; x<4; x++) {
					chips_mac[x] = get_data_memory_byte_raw(0x000000fe+x);
				}

				command_sequence_number++;

				mymemset(prev_MAC, 0, 16);
				mymemset(rxbuf, 0x00, 256);

				generate_prev_mac(txbuf, len, (CMD_WRITE | JET_CMD_BIT7 | JET_CMD_BIT6), prev_MAC);

				print_txt("prev_MAC=");
				print_hex(prev_MAC, 16);

				txbuf[0] = 0x00;
				generate_mac(txbuf, 0, 0x40, prev_MAC, MAC);

				print_txt("MAC=");
				print_hex(MAC, 4);

				print_txt("Chips MAC=");
				print_hex(chips_mac, 4);

				// compare our mac and the chips mac
				if(mymemcmp(MAC, chips_mac, 4)) {
					printf("MACS DID NOT MATCH\n");
					break;
				} else {
					printf("MACS MATCH\n");
				}
			}
		break;

	case 's':
		prog_memory[0xfa] = CMD_ECHO;	// DrvCmdByte cmd=20
		prog_memory[0xfc] = 0x00;		// DrvPacketLength1 length=0
		prog_memory[0xfd] = 0x08;		// DrvPacketLength0 length=8
		prog_memory[0xfe] = 0x64;		// packet deadbeef
		prog_memory[0xff] = 0x65;
		prog_memory[0x100] = 0x61;
		prog_memory[0x101] = 0x64;
		prog_memory[0x102] = 0x62;
		prog_memory[0x103] = 0x65;
		prog_memory[0x104] = 0x65;
		prog_memory[0x105] = 0x66;
		prog_memory[0x106] = 0x00;
		if(!send_command()) {
			printf("Send command failed.\n");
		}
		break;

	default:
		break;
	}
}


//--------------------------------------------------------------------
//
//	input				input packet payload data
//	length				input packet payload length
//	mac					output used to generate output packet mac
//--------------------------------------------------------------------

void generate_inbound_mac(uint8 *input, uint32 length, uint8 cmd_byte, uint8 *mac)
{
//#ifdef DEBUG_MACS
	print_txt("gim: length=");
	uint32tohex(length);
	print_txt(", cmd_byte=");
	uint8tohex(cmd_byte);
	newline();
	
	print_txt("input=");
	print_hex(input, length);
//#endif

	aes_set_key((uint8 *)&G_session_key5[0], 128); // set key to session key5

	//rjs debug
	mymemset(buf, 0, 256);

	buf[0] = command_sequence_number;
	buf[1] = cmd_byte & 0x1f;
	
	mymemcpy((uint8 *)&buf[2], input, length);

//#ifdef DEBUG_MACS
	print_txt("gim: Buf=");
	print_hex(buf, length+2);
//#endif

	aes_cmac_sign_new(1, buf, (uint16)(length + 2), (uint8 *)0, mac);	// generates 16 bytes

//#ifdef DEBUG_MACS
	print_txt("gim: Caclulated MAC=");
	print_hex(mac, 16);
//#endif
}

//--------------------------------------------------------------------
//
//	input				input packet payload data
//	length				input packet payload length
//	cmd_byte			input packet command byte (all 8 bits)
//	mac					output used to generate output packet mac
//--------------------------------------------------------------------

void generate_prev_mac(uint8 *input, uint32 length, uint8 cmd_byte, uint8 *mac)
{
#ifdef DEBUG_MACS
	print_txt("gpm: length=");
	uint32tohex(length);
	print_txt(", cmd_byte=");
	uint8tohex(cmd_byte);
	newline();
	
	print_txt("input=");
	print_hex(input, length);
#endif

	aes_set_key((uint8 *)&G_session_key5[0], 128); // set key to session key5

	//rjs debug
	mymemset(buf, 0, 256);

	buf[0] = 0x80;
	buf[1] = cmd_byte;
	
	mymemcpy((uint8 *)&buf[2], input, length);


#ifdef DEBUG_MACS
	print_txt("gpm buf=");
	print_hex((uint8 *)&buf[0], length+2);
#endif

	aes_cmac_sign_new(4, (uint8 *)&buf[0], (uint16)(length + 2), (uint8 *)0, mac);	// generates 16 bytes

#ifdef DEBUG_MACS
	print_txt("gpm: Caclulated MAC=");
	print_hex(mac, 16);
#endif
}

//--------------------------------------------------------------------
//
//	input				output packet payload data
//	length				output packet payload length
//	status_byte			output packet status byte (all 8 bits)
//	prev				input packet mac (from generate_prev_mac)
//	mac					output mac
//--------------------------------------------------------------------

uint8 aes_dout[16];

void generate_mac(uint8 *input, uint32 length, uint8 status_byte, uint8 *prev, uint8 *mac)
{
#ifdef DEBUG_MACS
	print_txt("gm(");
	uint32tohex((uint32)input);
	print_txt(",");
	uint32tohex(length);
	print_txt(",");
	uint8tohex(status_byte);
	print_txt(",");
	uint32tohex((uint32)prev);
	print_txt(",");
	uint32tohex((uint32)mac);		
	print_line(")");

	print_txt("gm input=");
	print_hex(input, length);

	print_txt("gm prev=");
	print_hex(prev, 16);
#endif

	aes_set_key((uint8 *)&G_session_key5[0], 128); // set key to session key5	

	//rjs debug
	mymemset(buf, 0, 256);

	buf[0] = 0x00;
	buf[1] = status_byte;

	mymemcpy((uint8 *)&buf[2], input, length);

#ifdef DEBUG_MACS
	print_txt("gm buf=");
	print_hex((uint8 *)&buf[0], length+2);
#endif

	aes_cmac_sign_new(5, (uint8 *)&buf[0], (uint16)(length + 2), prev, (uint8 *)&aes_dout[0]);	// generates 16 bytes

	mymemcpy(mac, (uint8 *)&aes_dout[0], 4);	// we need just the first 4 bytes

#ifdef DEBUG_MACS
	print_txt("gm: Caclulated MAC=");
	print_hex(mac, 4);
#endif
}

//
// mac test
//
void mac_test(void)
{
	unsigned char cmd;

	jet_status = 0;

	// init session key
	G_session_key5[0] = 0x4b;
	G_session_key5[1] = 0xe9;
	G_session_key5[2] = 0x12;
	G_session_key5[3] = 0xf8;
	G_session_key5[4] = 0x14;
	G_session_key5[5] = 0x6e;
	G_session_key5[6] = 0xe5;
	G_session_key5[7] = 0x04;
	G_session_key5[8] = 0x46;
	G_session_key5[9] = 0xaf;
	G_session_key5[10] = 0xc2;
	G_session_key5[11] = 0xe2;
	G_session_key5[12] = 0x48;
	G_session_key5[13] = 0x4f;
	G_session_key5[14] = 0xde;
	G_session_key5[15] = 0x86;
	
	mymemset(prev_MAC, 0, 16);
	mymemset(rxbuf, 0, 256);

	// 13 bytes good
	printf("13 byte message\n");

	cmd = CMD_WRITE | JET_CMD_BIT6;;

	rxbuf[0] = 0x00;		// packet 
	rxbuf[1] = 0x01;
	rxbuf[2] = 0x00;
	rxbuf[3] = 0x00;
	rxbuf[4] = 0x00;
	rxbuf[5] = 0xa3;
	rxbuf[6] = 0x44;
	rxbuf[7] = 0x6e;
	rxbuf[8] = 0x2f;
	rxbuf[9] = 0x09;
	rxbuf[10] = 0x61;
	rxbuf[11] = 0x51;
	rxbuf[12] = 0xdd;

	generate_prev_mac(rxbuf, 13, cmd, prev_MAC); // bad one

	print_txt("prev_MAC=");
	print_hex(prev_MAC, 16);

	txbuf[0] = 0x00;
	generate_mac(txbuf, 0, 0x40, prev_MAC, MAC);

	print_txt("MAC=");
	print_hex(MAC, 4);

	mymemset(prev_MAC, 0, 16);
	mymemset(rxbuf, 0, 256);

	// 14 bytes bad
	printf("14 byte message\n");

	cmd = CMD_WRITE | JET_CMD_BIT6;

	rxbuf[0] = 0x00;		// packet 
	rxbuf[1] = 0x01;
	rxbuf[2] = 0x00;
	rxbuf[3] = 0x00;
	rxbuf[4] = 0x00;
	rxbuf[5] = 0xa3;
	rxbuf[6] = 0x44;
	rxbuf[7] = 0x6e;
	rxbuf[8] = 0x2f;
	rxbuf[9] = 0x09;
	rxbuf[10] = 0x61;
	rxbuf[11] = 0x51;
	rxbuf[12] = 0xdd;
	rxbuf[13] = 0x11;

	generate_prev_mac(rxbuf, 14, cmd, prev_MAC);	// good one

	print_txt("prev_MAC=");
	print_hex(prev_MAC, 16);

	txbuf[0] = 0x00;
	generate_mac(txbuf, 0, 0x40, prev_MAC, MAC);

	print_txt("MAC=");
	print_hex(MAC, 4);

	mymemset(prev_MAC, 0, 16);
	mymemset(rxbuf, 0, 256);

	// 15 bytes good
	printf("15 byte message\n");

	cmd = CMD_WRITE | JET_CMD_BIT6;

	rxbuf[0] = 0x00;		// packet 
	rxbuf[1] = 0x01;
	rxbuf[2] = 0x00;
	rxbuf[3] = 0x00;
	rxbuf[4] = 0x00;
	rxbuf[5] = 0xa3;
	rxbuf[6] = 0x44;
	rxbuf[7] = 0x6e;
	rxbuf[8] = 0x2f;
	rxbuf[9] = 0x09;
	rxbuf[10] = 0x61;
	rxbuf[11] = 0x51;
	rxbuf[12] = 0xdd;
	rxbuf[13] = 0x11;
	rxbuf[14] = 0x22;

	generate_prev_mac(rxbuf, 15, cmd, prev_MAC);	// good one

	print_txt("prev_MAC=");
	print_hex(prev_MAC, 16);

	txbuf[0] = 0x00;
	generate_mac(txbuf, 0, 0x40, prev_MAC, MAC);

	print_txt("MAC=");
	print_hex(MAC, 4);

	mymemset(prev_MAC, 0, 16);
	mymemset(rxbuf, 0, 256);

	// 13+16 bytes good
	printf("13+16 byte message\n");

	cmd = CMD_WRITE | JET_CMD_BIT6;;

	rxbuf[0] = 0x00;		// packet 
	rxbuf[1] = 0x01;
	rxbuf[2] = 0x00;
	rxbuf[3] = 0x00;
	rxbuf[4] = 0x00;
	rxbuf[5] = 0xa3;
	rxbuf[6] = 0x44;
	rxbuf[7] = 0x6e;
	rxbuf[8] = 0x2f;
	rxbuf[9] = 0x09;
	rxbuf[10] = 0x61;
	rxbuf[11] = 0x51;
	rxbuf[12] = 0xdd;

	generate_prev_mac(rxbuf, 13+16, cmd, prev_MAC); // bad one

	print_txt("prev_MAC=");
	print_hex(prev_MAC, 16);

	txbuf[0] = 0x00;
	generate_mac(txbuf, 0, 0x40, prev_MAC, MAC);

	print_txt("MAC=");
	print_hex(MAC, 4);

	mymemset(prev_MAC, 0, 16);
	mymemset(rxbuf, 0, 256);

	// 14+16 bytes bad
	printf("14+16 byte message\n");

	cmd = CMD_WRITE | JET_CMD_BIT6;

	rxbuf[0] = 0x00;		// packet 
	rxbuf[1] = 0x01;
	rxbuf[2] = 0x00;
	rxbuf[3] = 0x00;
	rxbuf[4] = 0x00;
	rxbuf[5] = 0xa3;
	rxbuf[6] = 0x44;
	rxbuf[7] = 0x6e;
	rxbuf[8] = 0x2f;
	rxbuf[9] = 0x09;
	rxbuf[10] = 0x61;
	rxbuf[11] = 0x51;
	rxbuf[12] = 0xdd;

	generate_prev_mac(rxbuf, 14+16, cmd, prev_MAC);	// good one

	print_txt("prev_MAC=");
	print_hex(prev_MAC, 16);

	txbuf[0] = 0x00;
	generate_mac(txbuf, 0, 0x40, prev_MAC, MAC);

	print_txt("MAC=");
	print_hex(MAC, 4);

	mymemset(prev_MAC, 0, 16);
	mymemset(rxbuf, 0, 256);

	// 15+16 bytes good
	printf("15+16 byte message\n");

	cmd = CMD_WRITE | JET_CMD_BIT6;

	rxbuf[0] = 0x00;		// packet 
	rxbuf[1] = 0x01;
	rxbuf[2] = 0x00;
	rxbuf[3] = 0x00;
	rxbuf[4] = 0x00;
	rxbuf[5] = 0xa3;
	rxbuf[6] = 0x44;
	rxbuf[7] = 0x6e;
	rxbuf[8] = 0x2f;
	rxbuf[9] = 0x09;
	rxbuf[10] = 0x61;
	rxbuf[11] = 0x51;
	rxbuf[12] = 0xdd;

	generate_prev_mac(rxbuf, 15+16, cmd, prev_MAC);	// good one

	print_txt("prev_MAC=");
	print_hex(prev_MAC, 16);

	txbuf[0] = 0x00;
	generate_mac(txbuf, 0, 0x40, prev_MAC, MAC);

	print_txt("MAC=");
	print_hex(MAC, 4);
}

//
// application specific trace
//
// logging to file will only occur after application
// specific trigger condition is met
//
// this is easy to effect, simply clear the trigger variable
//
void application_specific_trace(void)
{
	run_log_triggered = 0;	// clear the trigger variable
}

//
// application user interface help
//
void application_ui_help(void)
{
	printf("\nTarget Program Specific Commands:\n");
	printf("\tApplication Speci<f>ic Trace\n");
	printf("\t<I>nject commands\n");
	printf("\t<J> Tag information\n");
	printf("\t<v> Display Do Loop Variables\n");
}

//
// application user interface command parser
// called by simulator command parser
// returns 1 if we handled the command, 0 otherwise
//
int application_ui_parse(int c)
{
	int handled = 0;

	switch(c) {
	case '!':
		application_load_io_and_memory_initial_values();
		handled = 1;
		break;
	
	case 'j':
		tag_information();
		handled = 1;
		break;

	case 'i':
		load_inbound_message();
		handled = 1;
		break;

	case 'v':
		display_do_loop_vars();
		handled = 1;
		break;

	case 'f':
		application_specific_trace();	// logging to file will only occur after application
										// specific trigger condition is met
		handled = 1;
		break;

	default:
		break;
	}
	return(handled);
}
