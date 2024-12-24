//
//---------------------------------------------------------------------------
//
// ST7x Simulator - cpu 
//
// Author: Rick Stievenart
//
// Genesis: 03/04/2011
//
// RJS 09/17/2017 Added instruction scoreboarding
//
//----------------------------------------------------------------------------
//

#include "stdafx.h"
#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <conio.h>			// for kbhit()
#include <stdlib.h>
#include <time.h>
#include <windows.h>

#include "st7xcpu.h"

#include "processor.h"

#include "breakpoints.h"
#include "simulator.h"	// for access to simulation variables
#include "types.h"
#include "debug.h"

// instruction scoreboard mechanism
unsigned char primarys[256];
unsigned char precode_90s[256];
unsigned char precode_91s[256];
unsigned char precode_92s[256];
unsigned char precode_72s[256];

//
// Clear scoreboard
//
void clear_scoreboard(void)
{
	memset(primarys, 0, 256);
	memset(precode_90s, 0, 256);
	memset(precode_91s, 0, 256);
	memset(precode_92s, 0, 256);
	memset(precode_72s, 0, 256);
}

//
// display scoreboard (on stdout only), called by simulator
//
void display_scoreboard(void)
{
	printf("Non-precoded instructions:\n");
	print_hex(primarys, 256);	

	printf("precode_90s instructions:\n");
	print_hex(precode_90s, 256);	

	printf("precode_91s instructions:\n");
	print_hex(precode_91s, 256);

	printf("precode_92s instructions:\n");
	print_hex(precode_92s, 256);

	printf("precode_72s instructions:\n");
	print_hex(precode_72s, 256);
}

//
// Reset the simulated cpu
//
void reset_cpu(void)
{
	// initialize processor registers
	register_pc = PC_INITIAL_VALUE;
	register_a = 0;
	register_x = 0;
	register_y = 0;
	register_sp = SP_INITIAL_VALUE;
	register_cc = (0xe0 | (INTERRUPT_MASK_L0_BIT|INTERRUPT_MASK_L1_BIT));

	printf("*** CPU Reset ***\n"); // (on stdout only)
}

//
// Reset the processor
//
void reset_processor(void)
{
	clear_scoreboard();

	reset_cpu();

	// reset precode flags
	precode_90 = precode_91 = precode_92 = precode_72 = 0;

	printf("*** Processor Reset ***\n"); // (on stdout only)
}

//
// Set processor flags
//
inline void set_flags(unsigned char reg)
{
	if(reg) {
		register_cc &= ~ZERO_BIT;
	} else {
		register_cc |= ZERO_BIT;
	}

	if(reg & 0x80) {
		register_cc |= NEGATIVE_BIT;
	} else {
		register_cc &= ~NEGATIVE_BIT;
	}
}

//
//----------------------------------------------
// CPU ALU OPS
//----------------------------------------------
//

//
// Divide
//
void div(void)
{
	unsigned short alu_temp, alu_temp1, alu_temp2;

	// form 16 bit value from x:a register pair
	alu_temp = register_x << 8;
	alu_temp |= register_a;

	// get intermediates
	alu_temp1 = alu_temp / register_y;
	alu_temp2 = alu_temp % register_y;

//	alu_temp1 = alu_temp * register_y;	// debug for testing result
	
	// set result into x:a regiser pair
	register_x = alu_temp2;
	register_a = alu_temp1;
}

//
// Add setting condition code flags
//
void add(unsigned char val)
{
	unsigned short alu_temp;

	// for half-carry bit setting
	alu_temp = register_a & 0x0f;
	alu_temp += (unsigned short)(val &0x0f);
	if(alu_temp & 0x0010) {
		register_cc |= HALF_CARRY_BIT;
	} else {
		register_cc &= ~HALF_CARRY_BIT;
	}

	alu_temp = register_a;
	alu_temp += (unsigned short)val;
	if(alu_temp & 0x0100) {
		register_cc |= CARRY_BIT;
	} else {
		register_cc &= ~CARRY_BIT;
	}
	register_a = (unsigned char)(alu_temp & 0x00ff);

	set_flags(register_a);
}

//
// Add with Carry setting condition code flags
//
void adc(unsigned char val)
{
	unsigned short alu_temp;

	// for half-carry bit setting
	alu_temp = register_a & 0x0f;
	alu_temp += (unsigned short)(val & 0x0f);
	if(register_cc & CARRY_BIT) {
		alu_temp += 1;
	}
	if(alu_temp & 0x0010) {
		register_cc |= HALF_CARRY_BIT;
	} else {
		register_cc &= ~HALF_CARRY_BIT;
	}
	
	alu_temp = register_a;
	alu_temp += (unsigned short)val;

	if(register_cc & CARRY_BIT) {
		alu_temp += 1;
	}

	if(alu_temp & 0x0100) {
		register_cc |= CARRY_BIT;
	} else {
		register_cc &= ~CARRY_BIT;
	}
	register_a = (unsigned char)(alu_temp & 0x00ff);

	set_flags(register_a);
}

//
// Subtract setting condition code flags
//
void sub(unsigned char val)
{
	unsigned short alu_temp;

	alu_temp = register_a;
	alu_temp -= (unsigned short)val;

	if(alu_temp & 0x0100) {
		register_cc |= CARRY_BIT;
	} else {
		register_cc &= ~CARRY_BIT;
	}
	register_a = (unsigned char)(alu_temp & 0x00ff);

	set_flags(register_a);
}

//
// Subtract with carry (borrow) setting condition code flags
//
void sbc(unsigned char val)
{
	unsigned short alu_temp;

	alu_temp = register_a;
	alu_temp -= (unsigned short)val;
	if(register_cc & CARRY_BIT) {
		alu_temp -= 1;
	}
	if(alu_temp & 0x0100) {
		register_cc |= CARRY_BIT;
	} else {
		register_cc &= ~CARRY_BIT;
	}
	register_a = (unsigned char)(alu_temp & 0x00ff);

	set_flags(register_a);
}

//
// Rotate left through carry setting condition code flags
//
unsigned char rlc(unsigned char val)
{
	unsigned short alu_temp;
	unsigned char retval;

	alu_temp = (unsigned short)val;

	alu_temp <<= 1;

	if(register_cc & CARRY_BIT) {
		alu_temp |= 0x0001;
	} else {
		alu_temp &= 0xfffe;
	}

	if(alu_temp & 0x0100) {
		register_cc |= CARRY_BIT;
	} else {
		register_cc &= ~CARRY_BIT;
	}

	retval = (unsigned char)(alu_temp & 0x00ff);

	set_flags(retval);

	return(retval);
}

//
// Rotate Right through Carry setting condition code flags
//
unsigned char rrc(unsigned char val)
{
	unsigned short alu_temp;
	unsigned char retval;
	int saved_bit;

	alu_temp = (unsigned short)val;

	if(alu_temp & 0x0001) {
		saved_bit = 1;
	} else {
		saved_bit = 0;
	}

	// do the shift
	alu_temp >>= 1;

	if(register_cc & CARRY_BIT) {
		alu_temp |= 0x80;
	} else {
		alu_temp &= 0x7f;
	}

	if(saved_bit) {
		register_cc |= CARRY_BIT;
	} else {
		register_cc &= ~CARRY_BIT;
	}

	retval = (unsigned char)(alu_temp & 0x00ff);

	set_flags(retval);
	
	return(retval);
}

//
// Shift Left Arithmetic setting condition code flags
//
unsigned char sla(unsigned char val)
{
	unsigned short alu_temp;
	unsigned char retval;

	alu_temp = (unsigned short)val;
	alu_temp <<= 1;
	if(alu_temp & 0x0100) {
		register_cc |= CARRY_BIT;
	} else {
		register_cc &= ~CARRY_BIT;
	}
	retval = (unsigned char)(alu_temp & 0x00ff);
	set_flags(retval);
	return(retval);
}

//
// Shift Right Arithmetic setting condition code flags
//
unsigned char sra(unsigned char val)
{
	unsigned short alu_temp;
	unsigned char retval;
	int signbit;

	alu_temp = (unsigned short)val;

	if(alu_temp & 0x0001) {
		register_cc |= CARRY_BIT;
	} else {
		register_cc &= ~CARRY_BIT;
	}
	if(alu_temp & 0x0080) {
		signbit = 1;
	} else {
		signbit = 0;
	}

	// do the shift
	alu_temp >>= 1;

	if(signbit) {
		alu_temp |= 0x0080;
	}

	retval = (unsigned char)(alu_temp & 0x00ff);

	set_flags(retval);

	return(retval);
}

//
// Shift Right Logical setting condition code flags
//
unsigned char srl(unsigned char val)
{
	unsigned short alu_temp;
	unsigned char retval;

	alu_temp = (unsigned short)val;
	if(alu_temp & 0x0001) {
		register_cc |= CARRY_BIT;
	} else {
		register_cc &= ~CARRY_BIT;
	}
	alu_temp >>= 1;
	retval = (unsigned char)(alu_temp & 0x00ff);
	set_flags(retval);
	return(retval);
}

//
// Execute an instruction, return 0 if done, 1 to fetch and execute the next byte
// This os the heart of the simulator
// can set global abnormal termination flag
//
// any printing output should go the the simulators print buffer
// so it can decide what to do with it
//
int execute(void)
{
	register unsigned char instruction, short_address, indirect_address, short_src_address, short_dst_address;
	unsigned char temp, bcp_temp, bit_mask;
	unsigned int long_address, multiply_result, long_src_address, long_dst_address;
	unsigned int dest, bit, dword_address;
	short displacement;
	unsigned short short_indirect_address;

	print_buffer[0] = 0;	// so i can catch empty buffer in output guy

	executed_call_instruction = 0;
	executed_return_instruction = 0;
	aabnormal_termination = 0;

	// Save pc to previous_pc
	previous_register_pc = register_pc;

	// Save pc to previous_sp
	previous_register_sp = register_sp;

	// Fetch next instruction
	instruction = get_prog_memory_byte(register_pc); // can fail, clears running flag
	if(!running) {									// if it failed, and its an abnormal termination event stop simulator
		if(aabnormal_termination) {
			return(0);	// tell caller we are done, don't call us again
		}
	}

	// handle precodes
	//
	// precodes are handled by setting the appropiate global variable and telling the caller to call us again
	// to get the next byte (which is the instruction)
	//
	switch(instruction) {
	case PRECODE_72:
		sprintf((char *)print_buffer, "PRECODE_72 ");
		simulator_output();

		precode_72 = 1;
		// increment pc
		register_pc++;
		inc_sim_time(2);
		return(1); // tell caller to  call us again
		break;

	case PRECODE_90:
		sprintf((char *)print_buffer, "PRECODE_90 ");
		simulator_output();

		precode_90 = 1;
		// increment pc
		register_pc++;
		inc_sim_time(2);
		return(1); // tell caller to  call us again
		break;

	case PRECODE_91:
		sprintf((char *)print_buffer, "PRECODE_91 ");
		simulator_output();

		precode_91 = 1;
		// increment pc
		register_pc++;
		inc_sim_time(2);
		return(1); // tell caller to  call us again
		break;

	case PRECODE_92:
		sprintf((char *)print_buffer, "PRECODE_92 ");
		simulator_output();

		precode_92 = 1;
		// increment pc
		register_pc++;
		inc_sim_time(2);
		return(1);; // tell caller to  call us again
		break;
	}

	// scoreboard the instruction
	if(precode_90) {
		precode_90s[instruction] = 1;
	} else if(precode_91) {
		precode_91s[instruction] = 1;
	} else if(precode_92) {
		precode_92s[instruction] = 1;
	} else if(precode_72) {
		precode_72s[instruction] = 1;
	} else {
		primarys[instruction] = 1;
	}

	// decode and execute the instruction
	switch(instruction) {
	case EXGW:
		sprintf((char *)print_buffer, "EXGW  X,Y\n");
		temp = register_y;
		register_y = register_x;
		register_x = temp;
		// increment pc
		register_pc++;
		inc_sim_time(2);
		break;

	case EXG_A_X:
		sprintf((char *)print_buffer, "EXG  A,XL\n");
		
		temp = register_a;
		register_a = register_x;
		register_x = temp;
		// increment pc
		register_pc++;
		inc_sim_time(2);
		break;

	case EXG_A_Y:
		sprintf((char *)print_buffer, "EXG	 A,YL\n");
		
		temp = register_a;
		register_a = register_y;
		register_y = temp;
		// increment pc
		register_pc++;
		inc_sim_time(2);
		break;

	case EXG_A_LONG:
		long_address = get_data_memory_byte(register_pc+1) << 8;
		long_address |= get_data_memory_byte(register_pc+2);
		temp = get_data_memory_byte(long_address);

		sprintf((char *)print_buffer, "EXG	 A,%08x\n", long_address);
		
		put_data_memory_byte(long_address, register_a);
		register_a = temp;
		// increment pc
		register_pc +=3;
		inc_sim_time(2);
		break;

	// LDF	A,x
	case LDF_A_FAR:				//0xbc
		long_address = get_data_memory_byte(register_pc+1) << 16;
		long_address |= get_data_memory_byte(register_pc+2) << 8;
		long_address |= get_data_memory_byte(register_pc+3);

		sprintf((char *)print_buffer, "LDF	 A,%08x\n", long_address);
		
		register_a = get_data_memory_byte(long_address);
		set_flags(register_a);
		// increment pc
		register_pc += 4;
		inc_sim_time(1);
		break;

	case LDF_A_REG_IND:			//0xaf
		if(precode_72) {
			// not in standard st8
			short_indirect_address = get_data_memory_byte(register_pc+1) << 8;
			short_indirect_address |= get_data_memory_byte(register_pc+2);

			sprintf((char *)print_buffer, "LDF	 A,([%04x],X)\n", short_indirect_address);
			
			long_address = get_data_memory_byte(short_indirect_address) << 16;
			long_address |= get_data_memory_byte(short_indirect_address+1) << 8;
			long_address |= get_data_memory_byte(short_indirect_address+2);
			register_a = get_data_memory_byte(long_address+register_x);
			set_flags(register_a);	// increment pc
			register_pc += 3;
			inc_sim_time(1);
			precode_72 = 0;
		} else if(precode_90) {
			// standard st8 has this
			sprintf((char *)print_buffer, "LDF A, (extoff,Y) goes here\n");
			
			// unhandled precode handler will catch this

		} else if(precode_91) {
			// standard st8 has this
			sprintf((char *)print_buffer, "LDF A, ([longptr.e],Y) goes here\n");
			
			// unhandled precode handler will catch this

		} else if(precode_92) {
			// standard st8 has this
			sprintf((char *)print_buffer, "LDF A, ([longptr.e],Y) goes here\n");

			// unhandled precode handler will catch this

		} else {
			long_address = get_data_memory_byte(register_pc+1) << 16;
			long_address |= get_data_memory_byte(register_pc+2) << 8;
			long_address |= get_data_memory_byte(register_pc+3);

			sprintf((char *)print_buffer, "LDF	 A,(%08x,X)\n", long_address);
			

			register_a = get_data_memory_byte(long_address+register_x);
			set_flags(register_a);	// increment pc
			register_pc += 4;
			inc_sim_time(1);
		}
		break;

	// LDF x,A
	case LDF_FAR_A:				//0xbd
		if(precode_91) {
			// standard st8 has this: LDF([xxxx.e,Y),A
			sprintf((char *)print_buffer, "LDF [longptr.e], A)  goes here\n");

			// unhandled precode handler will catch this

		} else {
			long_address = get_data_memory_byte(register_pc+1) << 16;
			long_address |= get_data_memory_byte(register_pc+2) << 8;
			long_address |= get_data_memory_byte(register_pc+3);

			sprintf((char *)print_buffer, "LDF	 %08x,A\n", long_address);

			put_data_memory_byte(long_address, register_a);
			set_flags(register_a);
			// increment pc
			register_pc += 4;
			inc_sim_time(1);
		}
		break;

	case LDF_REG_IND_A:			//0xa7
		if(precode_72) {
			// not standard in ST7/8
			short_indirect_address = get_data_memory_byte(register_pc+1) << 8;
			short_indirect_address |= get_data_memory_byte(register_pc+2);

			sprintf((char *)print_buffer, "LDF	([%04x],X),A\n", short_indirect_address);
			
			long_address = get_data_memory_byte(short_indirect_address) << 16;
			long_address |= get_data_memory_byte(short_indirect_address+1) << 8;
			long_address |= get_data_memory_byte(short_indirect_address+2);
			put_data_memory_byte(long_address+register_x, register_a);
			set_flags(register_a);	// increment pc
			register_pc += 3;
			inc_sim_time(1);
			precode_72 = 0;
		} else if(precode_90) {
			// standard st8 has this
			sprintf((char *)print_buffer, "LDF (extoff,Y), A goes here\n");

			// unhandled precode handler will catch this
			
		} else if(precode_91) {
			// standard st8 has this
			sprintf((char *)print_buffer, "LDF ([longptr.e],Y), A goes here\n");

			// unhandled precode handler will catch this
			
		} else if(precode_92) {
			// standard st8 has this
			sprintf((char *)print_buffer, "LDF ([longptr.e],Y), A goes here\n");
			
			// unhandled precode handler will catch this
		
		} else {
			long_address = get_data_memory_byte(register_pc+1) << 16;
			long_address |= get_data_memory_byte(register_pc+2) << 8;
			long_address |= get_data_memory_byte(register_pc+3);

			sprintf((char *)print_buffer, "LDF	 (%08x,X),A\n", long_address);
			
			put_data_memory_byte((long_address+register_x), register_a );
			set_flags(register_a);
			// increment pc
			register_pc += 4;
			inc_sim_time(1);
		}
		break;

	case MOV_LONG_IMMED: // 35
		temp = get_data_memory_byte(register_pc+1);
		long_address = get_data_memory_byte(register_pc+2) << 8;
		long_address |= get_data_memory_byte(register_pc+3);
		sprintf((char *)print_buffer, "MOV	 %08x,#%02x\n", long_address, temp);
		
		put_data_memory_byte(long_address, temp);
		// increment pc
		register_pc += 4;
		inc_sim_time(1);
		break;

	case MOV_SHORT_SHORT:	// 45
		short_src_address = get_data_memory_byte(register_pc+1) ;
		short_dst_address = get_data_memory_byte(register_pc+2);
		temp = get_data_memory_byte(short_src_address);
		sprintf((char *)print_buffer, "MOV %02x,%02x (%02x)\n", short_dst_address, short_src_address, temp);
		
		put_data_memory_byte(short_dst_address, temp);
		register_pc += 3;
		inc_sim_time(2);
		break;

	case MOV_LONG_LONG:	// 55
		long_src_address = get_data_memory_byte(register_pc+1) << 8;
		long_src_address |= get_data_memory_byte(register_pc+2);
		long_dst_address = get_data_memory_byte(register_pc+3) << 8;
		long_dst_address |= get_data_memory_byte(register_pc+4);
		temp = get_data_memory_byte(long_src_address);
		put_data_memory_byte(long_dst_address, temp);
		sprintf((char *)print_buffer, "MOV %08x,%08x (%02x)\n", long_dst_address, long_src_address, temp);
		
		register_pc += 5;
		inc_sim_time(2);
		break;

	case OPCODE_0x65:
		sprintf((char *)print_buffer, "*** Unimplemented opcode @ %08x - %02x (DIVW)\n", register_pc, instruction);
		
		register_pc++;
		inc_sim_time(2);
		running = 0;
		break;

	case OPCODE_0x75:
		sprintf((char *)print_buffer, "*** Unimplemented opcode @ %08x - %02x\n", register_pc, instruction);
		
		register_pc++;
		inc_sim_time(2);
		running = 0;
		break;

/*
	case OPCODE_0x19:
		// on ST8 this is ADC A.(shortoff,SP) on ours it is like ST7 -> BRES_4
		sprintf((char *)print_buffer, "*** Unimplemented opcode @ %08x - %02x\n", register_pc, instruction);
		
		sprintf((char *)print_buffer, "ST8 -> ADC A.(shortoff,SP)\n");
		
		register_pc++;
		inc_sim_time(2);
		running =0;
		break;
*/
	case HALT:
		sprintf((char *)print_buffer, "HALT\n");
		
		register_cc &= ~(INTERRUPT_MASK_L0_BIT|INTERRUPT_MASK_L1_BIT);
		inc_sim_time(2);
		running = 0;
//		while(1)
//			;
		break;

	case ADD_SP:	//					0x5b
		// custom non-st7/8 implementation
		temp = get_data_memory_byte(register_pc+1);
		sprintf((char *)print_buffer, "LD  X:A,SP\n");
		
		register_x = (register_sp >> 8) & 0xff;
		register_a = register_sp & 0xff;
		// increment pc
		register_pc += 1;
		inc_sim_time(2);
		break;

	case RSP:
		sprintf((char *)print_buffer, "RSP\n");
		
		register_sp = SP_INITIAL_VALUE;
		// increment pc
		register_pc++;
		inc_sim_time(2);
		break;

	case LDW_SP_X:
		// this is non standard - this is BREAK on the ST8
		long_address = register_x << 8;
		long_address |= register_a;
		sprintf((char *)print_buffer, "LDW	SP,X:A (%04x)\n", long_address);
		
		register_sp = long_address;
		// increment pc
		register_pc++;
		inc_sim_time(2);
		break;

	case DIV:
		sprintf((char *)print_buffer, "DIV X,A\n");
		
		div();
		inc_sim_time(17);
		// increment pc
		register_pc++;
		break;

	case MUL:
	case MUL1:
		if(precode_90) {
			sprintf((char *)print_buffer, "MUL Y,A\n");
			
			multiply_result = register_y*register_a;
			register_y = (unsigned char)(multiply_result >> 8);
			register_a = (unsigned char)(multiply_result & 0x00ff);
			inc_sim_time(12);
			precode_90 = 0;

		} else {
			sprintf((char *)print_buffer, "MUL X,A\n");
			
			multiply_result = register_x*register_a;
			register_x = (unsigned char)(multiply_result >> 8);
			register_a = (unsigned char)(multiply_result & 0x00ff);
			inc_sim_time(11);

		}
		register_cc &= ~CARRY_BIT;
		register_cc &= ~HALF_CARRY_BIT;
		// increment pc
		register_pc++;
		break;

	case RCF:
		sprintf((char *)print_buffer, "RCF\n");
		
		register_cc &= ~CARRY_BIT;
		// increment pc
		register_pc++;
		inc_sim_time(2);
		break;

	case SCF:
		sprintf((char *)print_buffer, "SCF\n");
		
		register_cc |= CARRY_BIT;
		// increment pc
		register_pc++;
		inc_sim_time(2);
		break;

	case CCF:
		sprintf((char *)print_buffer, "CCF\n");
		
		if(register_cc & CARRY_BIT) {
			register_cc &= ~CARRY_BIT;
		} else {
			register_cc |= CARRY_BIT;
		}
		// increment pc
		register_pc++;
		inc_sim_time(3);
		break;

	case RIM:
		sprintf((char *)print_buffer, "RIM\n");
		
		register_cc &= ~(INTERRUPT_MASK_L0_BIT|INTERRUPT_MASK_L1_BIT);
		// increment pc
		register_pc++;
		inc_sim_time(2);
		break;

	case SIM:
		sprintf((char *)print_buffer, "SIM\n");
		
		register_cc |= (INTERRUPT_MASK_L0_BIT|INTERRUPT_MASK_L1_BIT);
		// increment pc
		register_pc++;
		inc_sim_time(2);
		break;

	case BRES_0:	// 0x11
	case BRES_1:	// 0x13
	case BRES_2:	// 0x15
	case BRES_3:	// 0x17
	case BRES_4:	// 0x19
	case BRES_5:	// 0x1b
	case BRES_6:	// 0x1d
	case BRES_7:	// 0x1f
		if(precode_90) {
			// st8 this is BCCM #xxx,#x Copy Carry Bit to Memory
			bit = get_data_memory_byte(register_pc);
			bit = (bit & 0x0f) / 2;
			bit--;
			
			long_address = get_data_memory_byte(register_pc+1) << 8;
			long_address |= get_data_memory_byte(register_pc+2);

			sprintf((char *)print_buffer, "BCCM %04x,#%d\n", long_address, bit);
			
			temp = get_data_memory_byte(long_address);
			temp &= ~(1 << bit);	// clear the target bit
			temp |= (register_cc & CARRY_BIT) << bit;	// move carry bit to correct bit position
			put_data_memory_byte(long_address, temp);

			// inc pc
			register_pc += 2;
			inc_sim_time(1);
			precode_90 = 0;
		} else if(precode_92) {
			// st7 this is "BRES [short]
			sprintf((char *)print_buffer, "BRES [short] (st7) goes here\n");

			// unhandled precode will catch this

		} else if(precode_72) {
			// like the ST8, we have this
			bit = (instruction & 0x0f) / 2;
			
			long_address = get_data_memory_byte(register_pc+1) << 8;
			long_address |= get_data_memory_byte(register_pc+2);

			sprintf((char *)print_buffer, "BRES %04x,#%d\n", long_address, bit);
			
			inc_sim_time(5);

			precode_72 = 0;

			temp = get_data_memory_byte(long_address);	
			temp &= ~1 << bit;
			put_data_memory_byte(long_address, temp);

			// inc pc
			register_pc += 3;
		} else {
			// st7 has this, we have this too
			bit = (instruction & 0x0f) / 2;

			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "BRES %02x,#%d\n", short_address, bit);
			
			inc_sim_time(5);

			temp = get_data_memory_byte(short_address);
			temp &= ~1 << bit;
			put_data_memory_byte(short_address, temp);

			// inc pc
			register_pc += 2;
		}
		break;

	case BSET_0:	// 0x10
	case BSET_1:	// 0x12
	case BSET_2:	// 0x14
	case BSET_3:	// 0x16
	case BSET_4:	// 0x18
	case BSET_5:	// 0x1a
	case BSET_6:	// 0x1c
	case BSET_7:	// 0x1e
		if(precode_90) {
			// st8 this is BCPL #xxx,#x Bit complement
			bit = get_data_memory_byte(register_pc);
			bit = (bit & 0x0f) / 2;
			
			long_address = get_data_memory_byte(register_pc+1) << 8;
			long_address |= get_data_memory_byte(register_pc+2);

			sprintf((char *)print_buffer, "BCPL %04x,#%d\n", long_address, bit);
			
			temp = get_data_memory_byte(long_address);
			bit_mask = (1 << bit);
			if(temp & bit_mask) {
				// bit is a 1, make it a 0
				temp &= ~bit_mask;
			} else {
				// bit is a 0, make it a 1
				temp |= bit_mask;
			}

			put_data_memory_byte(long_address, temp);

			// inc pc
			register_pc += 2;
			inc_sim_time(1);
			precode_90 = 0;
		} else if(precode_92) {
			// st7 this is "BSET [short]
			sprintf((char *)print_buffer, "BSET [short] (st7)\n");

			// unhandled precode handler will catch this

		} else if(precode_72) {
			// like the ST8, we have this
			bit = (instruction & 0x0f) / 2;

			long_address = get_data_memory_byte(register_pc+1) << 8;
			long_address |= get_data_memory_byte(register_pc+2);

			sprintf((char *)print_buffer, "BSET %04x,#%d\n", long_address, bit);
			
			inc_sim_time(5);

			precode_72 = 0;

			temp = get_data_memory_byte(long_address);	
			temp |= 1 << bit;
			put_data_memory_byte(long_address, temp);

			// inc pc
			register_pc += 3;
		
		}  else {
			// st7 has this, we have this too
			bit = (instruction & 0x0f) / 2;

			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "BSET %02x,#%d\n", short_address, bit);
			
			inc_sim_time(5);

			temp = get_data_memory_byte(short_address);	
			temp |= 1 << bit;
			put_data_memory_byte(short_address, temp);

			// inc pc
			register_pc += 2;
		}
		break;

	case BTJF_0:
	case BTJF_1:
	case BTJF_2:
	case BTJF_3:
	case BTJF_4:
	case BTJF_5:
	case BTJF_6:
	case BTJF_7:
		bit = (instruction & 0x0f) / 2;
		if(precode_72) {
			long_address = get_data_memory_byte(register_pc+1) << 8;
			long_address |= get_data_memory_byte(register_pc+2);
			displacement = get_data_memory_byte(register_pc+3);
			if(displacement & 0x0080) {
				displacement |= 0xff00;
			}

			// inc pc
			register_pc += 4;

			temp = get_data_memory_byte(long_address);
			if((temp & (1 << bit)) == 0) {
				register_pc += displacement;
				register_cc &= ~CARRY_BIT;
				sprintf((char *)print_buffer, "BTJF %08x,#%d,%d EA=%04x (Branch Taken)\n", long_address, bit, displacement, register_pc);
			} else {
				register_cc |= CARRY_BIT;
				sprintf((char *)print_buffer, "BTJF %08x,#%d,%d (Branch NOT Taken)\n", long_address, bit, displacement);
			}
			precode_72 = 0;
		} else {
			displacement = get_data_memory_byte(register_pc+2);
			if(displacement & 0x0080) {
				displacement |= 0xff00;
			}
			short_address = get_data_memory_byte(register_pc+1);

			inc_sim_time(5);

			// inc pc
			register_pc += 3;

			temp = get_data_memory_byte(short_address);
			if((temp & (1 << bit)) == 0) {
				register_pc += displacement;
				register_cc &= ~CARRY_BIT;
				sprintf((char *)print_buffer, "BTJF %02x,#%d,%d EA=%04x (Branch Taken)\n", short_address, bit, displacement, register_pc);
			} else {
				register_cc |= CARRY_BIT;
				sprintf((char *)print_buffer, "BTJF %02x,#%d,%d (Branch NOT Taken)\n", short_address, bit, displacement);
			}
		}
		break;

	case BTJT_0:
	case BTJT_1:
	case BTJT_2:
	case BTJT_3:
	case BTJT_4:
	case BTJT_5:
	case BTJT_6:
	case BTJT_7:
		bit = (instruction & 0x0f) / 2;

		if(precode_72) {
			long_address = get_data_memory_byte(register_pc+1) << 8;
			long_address |= get_data_memory_byte(register_pc+2);
			displacement = get_data_memory_byte(register_pc+3);
			if(displacement & 0x0080) {
				displacement |= 0xff00;
			}

			// inc pc
			register_pc += 4;

			temp = get_data_memory_byte(long_address);
			if((temp & (1 << bit))) {
				register_pc += displacement;
				register_cc |= CARRY_BIT;
				sprintf((char *)print_buffer, "BTJT %08x,#%d,%d EA=%04x (Branch Taken)\n", long_address, bit, displacement, register_pc);
			} else {
				register_cc &= ~CARRY_BIT;
				sprintf((char *)print_buffer, "BTJT %08x,#%d,%d (Branch NOT Taken)\n", long_address, bit, displacement);
			}
			precode_72 = 0;
		} else {
			displacement = get_data_memory_byte(register_pc+2);
			if(displacement & 0x0080) {
				displacement |= 0xff00;
			}
			short_address = get_data_memory_byte(register_pc+1);
			
			inc_sim_time(5);

			// inc pc
			register_pc += 3;

			temp = get_data_memory_byte(short_address);
			if((temp & (1 << bit))) {
				register_pc += displacement;
				register_cc |= CARRY_BIT;
				sprintf((char *)print_buffer, "BTJT %02x,#%d,%d EA=%04x (Branch Taken)\n", short_address, bit, displacement, register_pc);
				
			} else {
				register_cc &= ~CARRY_BIT;
				sprintf((char *)print_buffer, "BTJT %02x,#%d,%d (Branch NOT Taken)\n", short_address, bit, displacement);
			}
		}
		break;

	// JR's
	case JRC:
	case JREQ:
	case JRF:
	case JRH:
	case JRIH:
	case JRIL:
	case JRM:
	case JRMI:
	case JRNC:
	case JRNE:
	case JRNH:
	case JRNM:
	case JRPL:
	case JRUGT:
	case JRULE:
		inc_sim_time(5);

		switch(instruction) {

		case JRC:
			if(register_cc & CARRY_BIT) {
				displacement = get_data_memory_byte(register_pc+1);
				if(displacement & 0x0080) {
					displacement |= 0xff00;
				}
				register_pc += 2;
				register_pc += displacement;

				sprintf((char *)print_buffer, "JRC %04x (Branch Taken)\n", register_pc);
			} else {
				displacement = get_data_memory_byte(register_pc+1);
				if(displacement & 0x0080) {
					displacement |= 0xff00;
				}
				// increment pc
				register_pc += 2;
				sprintf((char *)print_buffer, "JRC %04x (Branch NOT Taken)\n", register_pc+displacement);
			}
			break;

		case JREQ:
			if(register_cc & ZERO_BIT) {
				displacement = get_data_memory_byte(register_pc+1);
				if(displacement & 0x0080) {
					displacement |= 0xff00;
				}
				register_pc += 2;
				register_pc += displacement;
				sprintf((char *)print_buffer, "JREQ %04x (Branch Taken)\n", register_pc);
			} else {
				// increment pc
				displacement = get_data_memory_byte(register_pc+1);
				if(displacement & 0x0080) {
					displacement |= 0xff00;
				}
				register_pc += 2;
				sprintf((char *)print_buffer, "JREQ %04x (Branch NOT Taken)\n", register_pc+displacement);
			}
			break;

		case JRF:
			displacement = get_data_memory_byte(register_pc+1);
			if(displacement & 0x0080) {
				displacement |= 0xff00;
			}
			register_pc += 2;
//			register_pc += displacement;	// never jump!
			sprintf((char *)print_buffer, "JRF %04x (Branch NOT Taken)\n", register_pc+displacement);
			break;

		case JRH:
			if(register_cc & HALF_CARRY_BIT) {
				displacement = get_data_memory_byte(register_pc+1);
				if(displacement & 0x0080) {
					displacement |= 0xff00;
				}
				register_pc += 2;
				register_pc += displacement;
				sprintf((char *)print_buffer, "JRH %04x (Branch Taken)\n", register_pc);
				
			} else {
				displacement = get_data_memory_byte(register_pc+1);
				if(displacement & 0x0080) {
					displacement |= 0xff00;
				}
				// increment pc
				register_pc += 2;
				sprintf((char *)print_buffer, "JRH %04x (Branch NOT Taken)\n", register_pc+displacement);
			}
			break;

		case JRIH:
			// this is not implemented verbatim, we have no interrupt line so alway take the jump for now
			displacement = get_data_memory_byte(register_pc+1);
			if(displacement & 0x0080) {
				displacement |= 0xff00;
			}
			register_pc += 2;
			register_pc += displacement;
			sprintf((char *)print_buffer, "JRIH %04x (Branch Taken)\n", register_pc);
			break;

		case JRIL:
			// this is not implemented verbatim, we have no interrupt line so alway take the jump for now
			displacement = get_data_memory_byte(register_pc+1);
			if(displacement & 0x0080) {
				displacement |= 0xff00;
			}
			register_pc += 2;
			register_pc += displacement;
			sprintf((char *)print_buffer, "JRIL %04x (Branch Taken)\n", register_pc);
			
			break;

		case JRM:
			if(register_cc & (INTERRUPT_MASK_L0_BIT|INTERRUPT_MASK_L1_BIT)) {
				displacement = get_data_memory_byte(register_pc+1);
				if(displacement & 0x0080) {
					displacement |= 0xff00;
				}
				register_pc += 2;
				register_pc += displacement;
				sprintf((char *)print_buffer, "JRM %04x (Branch Taken)\n", register_pc);
			} else {
				displacement = get_data_memory_byte(register_pc+1);
				if(displacement & 0x0080) {
					displacement |= 0xff00;
				}

				// increment pc
				register_pc += 2;
				sprintf((char *)print_buffer, "JRM %04x (Branch NOT Taken)\n", register_pc+displacement);
			}
			break;

		case JRMI:
			if(register_cc & NEGATIVE_BIT) {
				displacement = get_data_memory_byte(register_pc+1);
				if(displacement & 0x0080) {
					displacement |= 0xff00;
				}
				register_pc += 2;
				register_pc += displacement;
				sprintf((char *)print_buffer, "JRMI %04x (Branch Taken)\n", register_pc);
			} else {
				displacement = get_data_memory_byte(register_pc+1);
				if(displacement & 0x0080) {
					displacement |= 0xff00;
				}

				// increment pc
				register_pc += 2;
				sprintf((char *)print_buffer, "JRMI %04x (Branch NOT Taken)\n", register_pc+displacement);
			}
			break;

		case JRNC:
			if((register_cc & CARRY_BIT) == 0) {
				displacement = get_data_memory_byte(register_pc+1);
				if(displacement & 0x0080) {
					displacement |= 0xff00;
				}
				register_pc += 2;
				register_pc += displacement;
				sprintf((char *)print_buffer, "JRNC %04x (Branch Taken)\n", register_pc);
			} else {
				displacement = get_data_memory_byte(register_pc+1);
				if(displacement & 0x0080) {
					displacement |= 0xff00;
				}

				// increment pc
				register_pc += 2;
				sprintf((char *)print_buffer, "JRNC %04x (Branch NOT Taken)\n", register_pc+displacement);
			}
			break;

		case JRNE:
			if((register_cc & ZERO_BIT) == 0) {
				displacement = get_data_memory_byte(register_pc+1);
				if(displacement & 0x0080) {
					displacement |= 0xff00;
				}
				register_pc += 2;
				register_pc += displacement;
				sprintf((char *)print_buffer, "JRNE %04x (Branch Taken)\n", register_pc);
			} else {
				displacement = get_data_memory_byte(register_pc+1);
				if(displacement & 0x0080) {
					displacement |= 0xff00;
				}

				// increment pc
				register_pc += 2;
				sprintf((char *)print_buffer, "JRNE %04x (Branch NOT Taken)\n", register_pc+displacement);
			}
			break;

		case JRNH:
			if((register_cc & HALF_CARRY_BIT) == 0) {
				displacement = get_data_memory_byte(register_pc+1);
				if(displacement & 0x0080) {
					displacement |= 0xff00;
				}
				register_pc += 2;
				register_pc += displacement;	
				sprintf((char *)print_buffer, "JRNH %04x (Branch Taken)\n", register_pc);
			} else {
				displacement = get_data_memory_byte(register_pc+1);
				if(displacement & 0x0080) {
					displacement |= 0xff00;
				}

				// increment pc
				register_pc += 2;
				sprintf((char *)print_buffer, "JRNH %04x (Branch NOT Taken)\n", register_pc+displacement);
			}
			break;

		case JRNM:
			if((register_cc & (INTERRUPT_MASK_L0_BIT|INTERRUPT_MASK_L1_BIT)) == 0) {
				displacement = get_data_memory_byte(register_pc+1);
				if(displacement & 0x0080) {
					displacement |= 0xff00;
				}
				register_pc += 2;
				register_pc += displacement;
				sprintf((char *)print_buffer, "JRNM %04x (Branch Taken)\n", register_pc);
				
			} else {
				displacement = get_data_memory_byte(register_pc+1);
				if(displacement & 0x0080) {
					displacement |= 0xff00;
				}
				// increment pc
				register_pc += 2;
				sprintf((char *)print_buffer, "JRNM %04x (Branch NOT Taken)\n", register_pc+displacement);
				
			}

			break;

		case JRPL:
			if((register_cc & NEGATIVE_BIT) == 0) {
				displacement = get_data_memory_byte(register_pc+1);
				if(displacement & 0x0080) {
					displacement |= 0xff00;
				}
				register_pc += 2;
				register_pc += displacement;

				sprintf((char *)print_buffer, "JRPL %04x (Branch Taken)\n", register_pc);
				
			} else {
				displacement = get_data_memory_byte(register_pc+1);
				if(displacement & 0x0080) {
					displacement |= 0xff00;
				}
				// increment pc
				register_pc += 2;
				sprintf((char *)print_buffer, "JRPL %04x (Branch NOT Taken)\n", register_pc+ displacement);
				
			}
			break;

		case JRUGT:
			if(((register_cc & CARRY_BIT) | (register_cc & ZERO_BIT)) == 0) {
				displacement = get_data_memory_byte(register_pc+1);
				if(displacement & 0x0080) {
					displacement |= 0xff00;
				}
				register_pc += 2;
				register_pc += displacement;
				sprintf((char *)print_buffer, "JRUGT %04x (Branch Taken)\n", register_pc);
				
			} else {
				displacement = get_data_memory_byte(register_pc+1);
				if(displacement & 0x0080) {
					displacement |= 0xff00;
				}
				// increment pc
				register_pc += 2;
				sprintf((char *)print_buffer, "JRUGT %04x (Branch NOT Taken)\n", register_pc+ displacement);
				
			}
			break;

		case JRULE:
			if(((register_cc & CARRY_BIT) || (register_cc & ZERO_BIT))) {
				displacement = get_data_memory_byte(register_pc+1);
				if(displacement & 0x0080) {
					displacement |= 0xff00;
				}
				register_pc += 2;
				register_pc += displacement;
				sprintf((char *)print_buffer, "JRULE %04x (Branch Taken)\n", register_pc);
				
			} else {
				displacement = get_data_memory_byte(register_pc+1);
				if(displacement & 0x0080) {
					displacement |= 0xff00;
				}
				// increment pc
				register_pc += 2;
				sprintf((char *)print_buffer, "JRULE %04x (Branch NOT Taken)\n", register_pc+displacement);
				
			}
			break;
		}
		break;

	case JRA:	// samae as JRT
		displacement = get_data_memory_byte(register_pc+1);
		if(displacement & 0x0080) {
			displacement |= 0xff00;
		}
		register_pc += 2;
		register_pc += displacement;
		sprintf((char *)print_buffer, "JRA %04x (Branch Taken)\n", register_pc);
		inc_sim_time(3);
		break;


	case JP_LONG:
		if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			dest = get_data_memory_byte(indirect_address) << 8;
			dest |= get_data_memory_byte(indirect_address+1);
			register_pc &= 0xffff0000;
			register_pc |= dest;
			sprintf((char *)print_buffer, "JP [%02x.w]=%04x : pc=%08x\n", indirect_address, dest, register_pc);
			inc_sim_time(5);
			precode_92 = 0;

		} else {
			dest = get_data_memory_byte(register_pc+1) << 8;
			dest |= get_data_memory_byte(register_pc+2);
			register_pc &= 0xffff0000;
			register_pc |= dest;
			sprintf((char *)print_buffer, "JP %04x : pc=%08x\n", dest, register_pc);
			inc_sim_time(3);
		}
		break;

	case JP_FAR:
		if(precode_92) {
			short_indirect_address = get_data_memory_byte(register_pc+1) << 8;
			short_indirect_address |= get_data_memory_byte(register_pc+2);
			dest = get_data_memory_byte(short_indirect_address) << 16;
			dest |= get_data_memory_byte(short_indirect_address+1) << 8;
			dest |= get_data_memory_byte(short_indirect_address+1);
			register_pc = dest;
			sprintf((char *)print_buffer, "JPF [%04x.w]=%08x : pc=%08x\n", short_indirect_address, dest, register_pc);
			inc_sim_time(6);
			precode_92 = 0;

		} else {
			dest = get_data_memory_byte(register_pc+1) << 16;
			dest |= get_data_memory_byte(register_pc+2) << 8;
			dest |= get_data_memory_byte(register_pc+3);
			register_pc = dest;
			sprintf((char *)print_buffer, "JPF %04x : pc=%08x\n", dest, register_pc);
			inc_sim_time(2);
		}
		break;

	case JP_REG_IND:
		if(precode_90) {
			// (Y)
			dest = register_y;
			register_pc &= 0xffff0000;
			register_pc |= dest;
			sprintf((char *)print_buffer, "JP (Y)=%04x : pc=%08x\n", dest, register_pc);
			inc_sim_time(3);
			precode_90 = 0;

		} else {
			// (X)
			dest = register_x;
			register_pc &= 0xffff0000;
			register_pc |= dest;
			sprintf((char *)print_buffer, "JP (X)=%04x : pc=%08x\n", dest, register_pc);
			inc_sim_time(2);

		}
		break;

	case JP_REG_IND_OFF_SHORT:
		if(precode_90) {
			// (short,Y)
			short_address = get_data_memory_byte(register_pc+1);
		
			temp = register_y;

			dest = short_address;
			dest += temp;
		
			register_pc &= 0xffff0000;
			register_pc |= dest;

			sprintf((char *)print_buffer, "JP (%02x,Y)=%04x : pc=%08x\n", short_address, dest, register_pc);

			inc_sim_time(3);
			precode_90 = 0;

		} else if(precode_92) {
			// ([short],X)
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			
			temp = register_x;
			dest = short_address;
			dest += temp;
		
			register_pc &= 0xffff0000;
			register_pc |= dest;

			sprintf((char *)print_buffer, "JP ([%02x],X)=%04x : pc=%08x\n", indirect_address, dest, register_pc);

			inc_sim_time(5);
			precode_92 = 0;

		} else if(precode_91) {
			// ([short],Y)
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			
			temp = register_y;
			dest = short_address;
			dest += temp;
		
			register_pc &= 0xffff0000;
			register_pc |= dest;

			sprintf((char *)print_buffer, "JP ([%02x],Y)=%04x : pc=%08x\n", indirect_address, dest, register_pc);

			inc_sim_time(5);
			precode_91 = 0;

		} else {
			// (short,X)
			short_address = get_data_memory_byte(register_pc+1);
			
			temp = register_x;
			dest = short_address;
			dest += temp;
		
			register_pc &= 0xffff0000;
			register_pc |= dest;
			sprintf((char *)print_buffer, "JP (%02x,X)=%04x : pc=%08x\n", short_address, dest, register_pc);

			inc_sim_time(3);
		}
		break;

	case JP_REG_IND_OFF_LONG:	// 0xdc
		if(precode_90) {
			// (longoff,Y)
			long_address = get_data_memory_byte(register_pc+1) << 8;
			long_address |= get_data_memory_byte(register_pc+2);
			
			temp = register_y;

			dest = long_address;
			dest += temp;
		
			register_pc &= 0xffff0000;
			register_pc |= dest;

			sprintf((char *)print_buffer, "JP (%04x,Y)  [la=%04x temp=%02x]=%04x : pc=%08x\n", long_address, long_address, temp, dest, register_pc);

			precode_90 = 0;

		} else if(precode_92) {
			// ([shortptr.w],X)
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = get_data_memory_byte(indirect_address) << 8;
			long_address |= get_data_memory_byte(indirect_address+1);
			
			temp = register_x;

			dest = long_address;
			dest += temp;
		
			register_pc &= 0xffff0000;
			register_pc |= dest;
			sprintf((char *)print_buffer, "JP ([%02x.w],X)  [la=%04x temp=%02x]=%04x : pc=%08x\n", indirect_address, long_address, temp, dest, register_pc);

			precode_92 = 0;

		} else if(precode_91) {
			// ([shortptr.w],Y)
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = get_data_memory_byte(indirect_address) << 8;
			long_address = get_data_memory_byte(indirect_address)+1;
			
			temp = register_y;
		
			dest = long_address;
			dest += temp;
		
			register_pc &= 0xffff0000;
			register_pc |= dest;
			sprintf((char *)print_buffer, "JP ([%02x.w],Y)  [la=%04x temp=%02x]=%04x : pc=%08x\n", indirect_address, long_address, temp, dest, register_pc);

			precode_91= 0;

		} else {
			// (longoff,X)
			long_address = get_data_memory_byte(register_pc+1) << 8;
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "JP (%04x,X)", long_address);
			
			temp = register_x;

			dest = long_address;
			dest += temp;
		
			register_pc &= 0xffff0000;
			register_pc |= dest;

			sprintf((char *)print_buffer, "JP (%04x,X)  [la=%04x temp=%02x]=%04x : pc=%08x\n", long_address, long_address, temp, dest, register_pc);
			
		}
		inc_sim_time(6);
		break;

	// NOP
	case NOP:
		sprintf((char *)print_buffer, "NOP\n");
		
		// increment pc
		register_pc++;
		inc_sim_time(2);
		break;

	// ADC A,x
	case ADC_IMMED: // 0xa9
		sprintf((char *)print_buffer, "ADC A,#%02x\n", get_data_memory_byte(register_pc+1));
		adc(get_data_memory_byte(register_pc+1));
		// increment pc
		register_pc += 2;
		inc_sim_time(2);
		
		break;

	case ADC_SHORT: // 0xb9
		if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "ADC A,[%02x]\n", indirect_address);
			
			inc_sim_time(5);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "ADC A,%02x\n", short_address);
			
			inc_sim_time(3);
		}
		adc(get_data_memory_byte(short_address));
		// increment pc
		register_pc += 2;
		break;

	case ADC_LONG:	// 0xc9
		if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "ADC A,[%02x.w]\n", indirect_address);
			
			adc(get_data_memory_byte(long_address));
			// increment pc
			register_pc += 2;
			inc_sim_time(6);
			precode_92 = 0;

		} else {
			// longmem
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "ADC A,%04x\n", long_address);
			
			adc(get_data_memory_byte(long_address));
			// increment pc
			register_pc += 3;
			inc_sim_time(4);

		}
		break;

	case ADC_REG_IND:
		if(precode_90) {
			sprintf((char *)print_buffer, "ADC A,(Y)\n");
			adc(get_data_memory_byte(register_y));
			inc_sim_time(4);
			precode_90 = 0;
		} else {
			sprintf((char *)print_buffer, "ADC A,(X)\n");
			adc(get_data_memory_byte(register_x));
			inc_sim_time(3);
		}
		// increment pc
		register_pc++;
		
		break;

	case ADC_REG_IND_OFF_SHORT:	// 0xe9
		if(precode_90) {
			// shortoff,y
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "ADC A,(%02x,Y)\n", short_address);
			adc(get_data_memory_byte(short_address+register_y));
			precode_90 = 0;

		} else if(precode_91) {
			// [shortptr.w],y
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "ADC A,([%02x],Y)\n", indirect_address);
			adc(get_data_memory_byte(short_address+register_y));
			precode_91 = 0;

		} else if(precode_92) {
			// [shortptr.w],x
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "ADC A,([%02x],X)\n", indirect_address);
			adc(get_data_memory_byte(short_address+register_x));
			precode_92 = 0;

		} else {
			// shortoff,X
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "ADC A,(%02x,X)\n", short_address);
			adc(get_data_memory_byte(short_address+register_x));
			register_a += get_data_memory_byte(short_address+register_x);
		}
		// increment pc
		register_pc += 2;
		
		break;

	case ADC_REG_IND_OFF_LONG:	// 0xd9
		if(precode_90) {
			// longoff,y
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "ADC A,(%04x,Y)\n", long_address);
			adc(get_data_memory_byte(long_address+register_y));
			// increment pc
			register_pc += 3;
			inc_sim_time(4);
			precode_90 = 0;

		} else if(precode_91) {
			// [shortptr.w],y
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "ADC A,([%02x.w],Y)\n", indirect_address);
			adc(get_data_memory_byte(long_address+register_y));
			// increment pc
			register_pc += 2;
			inc_sim_time(6);
			precode_91 = 0;

		} else if(precode_92) {
			//[shortptr.w],x
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "ADC A,([%02x.w],X)\n", indirect_address);
			adc(get_data_memory_byte(long_address+register_x));
			// increment pc
			register_pc += 2;
			inc_sim_time(6);
			precode_92 = 0;

		} else {
			// longoff,X
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "ADC A,(%04x, X)\n", long_address);
			adc(get_data_memory_byte(long_address+register_x));
			// increment pc
			register_pc += 3;
			inc_sim_time(4);

		}
		break;

	// ADD A,x
	case ADD_IMMED:
		sprintf((char *)print_buffer, "ADD A,#%02x\n", get_data_memory_byte(register_pc+1));
		add(get_data_memory_byte(register_pc+1));
		// increment pc
		register_pc += 2;
		inc_sim_time(2);
		break;

	case ADD_SHORT:	// 0xbb
		if(precode_92) {
			// [short]
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "ADD A,[%02x]\n", indirect_address);
			inc_sim_time(5);
			precode_92 = 0;
		} else {
			// short
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "ADD A,%02x\n", short_address);
			inc_sim_time(3);
		}
		add(get_data_memory_byte(short_address));
		// increment pc
		register_pc += 2;
		break;

	case ADD_LONG:	// 0xcb
		if(precode_92) {
			// [shortptr.w]
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "ADD A,[%02x.w]\n", indirect_address);
			add(get_data_memory_byte(long_address));
			// increment pc
			register_pc += 2;
			inc_sim_time(6);
			precode_92 = 0;
		} else {
			// longmem
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "ADD A,%04x\n", long_address);
			add(get_data_memory_byte(long_address));
			// increment pc
			register_pc += 3;
			inc_sim_time(4);
		}
		break;

	case ADD_REG_IND:	// 0xfb
		if(precode_90) {
			// (Y)
			sprintf((char *)print_buffer, "ADD A,(Y)\n");
			add(get_data_memory_byte(register_y));
			inc_sim_time(4);
			precode_90 = 0;
		} else {
			// (X)
			sprintf((char *)print_buffer, "ADD A,(X)\n");
			add(get_data_memory_byte(register_x));
			inc_sim_time(3);
		}
		// increment pc
		register_pc++;
		break;

	case ADD_REG_IND_OFF_SHORT:	// 0xeb
		if(precode_90) {
			// (shortoff,Y)
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "ADD A,(%02x,Y)\n", short_address);
			add(get_data_memory_byte(short_address+register_y));
			inc_sim_time(4);
			precode_90 = 0;

		} else if(precode_91) {
			// ([short],Y) - Present in ST7, but not ST8
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "ADD A,([%02x],Y)\n", indirect_address);
			add(get_data_memory_byte(short_address+register_y));
			inc_sim_time(6);
			precode_91 = 0;

		} else if(precode_92) {
			// ([short],X) - Present in ST7, but not ST8
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "ADD A,([%02x],X)\n", indirect_address);
			add(get_data_memory_byte(short_address+register_x));
			inc_sim_time(6);
			precode_92 = 0;

		} else {
			// (shortoff,X)
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "ADD A,(%02x,X)\n", short_address);
			add(get_data_memory_byte(short_address+register_x));
			inc_sim_time(4);
		}
		// increment pc
		register_pc += 2;
		break;

	case ADD_REG_IND_OFF_LONG:	// 0xdb
		if(precode_90) {
			// (longoff,Y)
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "ADD A,(%04x,Y)\n", long_address);
			add(get_data_memory_byte(long_address+register_y));
			// increment pc
			register_pc += 3;
			inc_sim_time(6);
			precode_90 = 0;

		} else if(precode_91) {
			// ([shortptr.w],Y)
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "ADD A,([%02x.w],Y)\n", indirect_address);
			add(get_data_memory_byte(long_address+register_y));
			// increment pc
			register_pc += 2;
			inc_sim_time(7);
			precode_91 = 0;

		} else if(precode_92) {
			// ([shortptr.w],X)
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "ADD A,([%02x.w],X)\n", indirect_address);
			add(get_data_memory_byte(long_address+register_x));
			// increment pc
			register_pc += 2;
			inc_sim_time(7);
			precode_92 = 0;

		} else {
			// (longoff,X)
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "ADD A,(%04x, X)\n", long_address);
			add(get_data_memory_byte(long_address+register_x));
			// increment pc
			register_pc += 3;
			inc_sim_time(6);

		}
		break;

	// AND A,x
	case AND_IMMED:	// 0xa4
		sprintf((char *)print_buffer, "AND A,#%02x\n", get_data_memory_byte(register_pc+1));
		register_a &= get_data_memory_byte(register_pc+1);
		set_flags(register_a);
		// increment pc
		register_pc += 2;
		inc_sim_time(2);
		break;

	case AND_SHORT:	// 0xb4
		if(precode_92) {
			// [short] - Present in ST7, but not ST8
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "AND A,[%02x]\n", indirect_address);
			inc_sim_time(5);
			precode_92 = 0;

		} else {
			// shortmem
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "AND A,%02x\n", short_address);
			inc_sim_time(3);

		}
		register_a &= get_data_memory_byte(short_address);
		set_flags(register_a);
		// increment pc
		register_pc += 2;
		break;

	case AND_LONG:	// 0xc4
		if(precode_92) {
			// [shortptr.w]
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "AND A,[%02x.w]\n", indirect_address);
			register_a &= get_data_memory_byte(long_address);
			// increment pc
			register_pc += 2;
			inc_sim_time(6);
			precode_92 = 0;

		} else {
			// longmem
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "AND A,%04x\n", long_address);
			register_a &= get_data_memory_byte(long_address);
			// increment pc
			register_pc += 3;
			inc_sim_time(4);

		}
		set_flags(register_a);
		
		break;

	case AND_REG_IND:
		if(precode_90) {
			// (Y)
			sprintf((char *)print_buffer, "AND A,(Y)\n");
			register_a &= get_data_memory_byte(register_y);
			inc_sim_time(4);
			precode_90 = 0;

		} else {
			// (X)
			sprintf((char *)print_buffer, "AND A,(X)\n");
			register_a &= get_data_memory_byte(register_x);
			inc_sim_time(3);

		}
		set_flags(register_a);
		// increment pc
		register_pc++;
		
		break;

	case AND_REG_IND_OFF_SHORT:	// 0xe4
		if(precode_90) {
			// (shortoff,Y)
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "AND A,(%02x,Y)\n", short_address);
			register_a &= get_data_memory_byte(short_address+register_y);
			inc_sim_time(4);
			precode_90 = 0;

		} else if(precode_91) {
			// ([short],Y) - Present in ST7 but not st8
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "AND A,([%02x],Y)\n", indirect_address);
			register_a &= get_data_memory_byte(short_address+register_y);
			inc_sim_time(6);
			precode_91 = 0;

		} else if(precode_92) {
			// ([short],X) - - Present in ST7 but not st8
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "AND A,([%02x],X)\n", indirect_address);
			register_a &= get_data_memory_byte(short_address+register_x);
			inc_sim_time(6);
			precode_92 = 0;

		} else {
			// (shortoff,X)
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "AND A,(%02x,X)\n", short_address);
			register_a &= get_data_memory_byte(short_address+register_x);
			inc_sim_time(4);

		}
		set_flags(register_a);
		// increment pc
		register_pc += 2;
		
		break;

	case AND_REG_IND_OFF_LONG:	// 0xd4
		if(precode_90) {
			// (longoff,Y)
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "AND A,(%04x,Y)\n", long_address);
			register_a &= get_data_memory_byte(long_address+register_y);
			// increment pc
			register_pc += 3;
			inc_sim_time(5);
			precode_90 = 0;

		} else if(precode_91) {
			// ([shortptr.w],Y)
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "AND A,([%02x.w],Y)\n", indirect_address);
			register_a &= get_data_memory_byte(long_address+register_y);
			// increment pc
			register_pc += 2;
			inc_sim_time(7);
			precode_91 = 0;

		} else if(precode_92) {
			// ([shortptr.w],X)
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "AND A,([%02x.w],X)\n", indirect_address);
			register_a &= get_data_memory_byte(long_address+register_x);
			// increment pc
			register_pc += 2;
			inc_sim_time(7);
			precode_92 = 0;

		} else {
			// (longoff,X)
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "AND A,(%04x, X)\n", long_address);
			register_a &= get_data_memory_byte(long_address+register_x);
			// increment pc
			register_pc += 3;
			inc_sim_time(5);

		}
		set_flags(register_a);
		
		break;

	// BCP A,x
	case BCP_IMMED:
		sprintf((char *)print_buffer, "BCP A,#%02x\n", get_data_memory_byte(register_pc+1));
		bcp_temp = register_a;
		bcp_temp &= get_data_memory_byte(register_pc+1);
		set_flags(bcp_temp);
		// increment pc
		register_pc += 2;
		inc_sim_time(2);
		
		break;

	case BCP_SHORT:
		if(precode_92) {
			// [short] - Present in ST7, but not ST8
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "BCP A,[%02x]\n", indirect_address);
			inc_sim_time(5);
			precode_92 = 0;

		} else {
			// shortmem
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "BCP A,%02x\n", short_address);
			inc_sim_time(3);

		}
		bcp_temp = register_a;
		bcp_temp &= get_data_memory_byte(short_address);
		set_flags(bcp_temp);
		// increment pc
		register_pc += 2;
		
		break;

	case BCP_LONG:
		if(precode_92) {
			// [shortptr.w]
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "BCP A,[%02x.w]\n", indirect_address);
			bcp_temp = register_a;
			bcp_temp &= get_data_memory_byte(long_address);
			// increment pc
			register_pc += 2;
			inc_sim_time(6);
			precode_92 = 0;

		} else {
			// longmem
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "BCP A,%04x\n", long_address);
			bcp_temp = register_a;
			bcp_temp &= get_data_memory_byte(long_address);
			// increment pc
			register_pc += 3;
			inc_sim_time(4);

		}
		set_flags(bcp_temp);
		
		break;

	case BCP_REG_IND:
		if(precode_90) {
			// (Y)
			sprintf((char *)print_buffer, "BCP A,(Y)\n");
			bcp_temp = register_a;
			bcp_temp &= get_data_memory_byte(register_y);
			inc_sim_time(4);
			precode_90 = 0;

		} else {
			// (X)
			sprintf((char *)print_buffer, "BCP A,(X)\n");
			bcp_temp = register_a;
			bcp_temp &= get_data_memory_byte(register_x);
			inc_sim_time(3);

		}
		set_flags(bcp_temp);
		// increment pc
		register_pc++;
		
		break;

	case BCP_REG_IND_OFF_SHORT:
		if(precode_90) {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "BCP A,(%02x,Y)\n", short_address);
			bcp_temp = register_a;
			bcp_temp &= get_data_memory_byte(short_address+register_y);
			inc_sim_time(4);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "BCP A,([%02x],Y)\n", indirect_address);
			bcp_temp = register_a;
			bcp_temp &= get_data_memory_byte(short_address+register_y);
			inc_sim_time(6);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "BCP A,([%02x],X)\n", indirect_address);
			bcp_temp = register_a;
			bcp_temp &= get_data_memory_byte(short_address+register_x);
			inc_sim_time(6);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "BCP A,(%02x,X)\n", short_address);
			bcp_temp = register_a;
			bcp_temp &= get_data_memory_byte(short_address+register_x);
			inc_sim_time(4);
		}		
		set_flags(bcp_temp);
		// increment pc
		register_pc += 2;
		
		break;

	case BCP_REG_IND_OFF_LONG:
		if(precode_90) {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "BCP A,(%04x,Y)\n", long_address);
			bcp_temp = register_a;
			bcp_temp &= get_data_memory_byte(long_address+register_y);
			// increment pc
			register_pc += 3;
			inc_sim_time(5);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "BCP A,([%02x.w],Y)\n", indirect_address);
			bcp_temp = register_a;
			bcp_temp &= get_data_memory_byte(long_address+register_y);
			// increment pc
			register_pc += 2;
			inc_sim_time(7);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "BCP A,([%02x.w],X)\n", indirect_address);
			bcp_temp = register_a;
			bcp_temp &= get_data_memory_byte(long_address+register_x);
			// increment pc
			register_pc += 2;
			inc_sim_time(6);
			precode_92 = 0;

		} else {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "BCP A,(%04x, X)\n", long_address);
			bcp_temp = register_a;
			bcp_temp &= get_data_memory_byte(long_address+register_x);
			// increment pc
			register_pc += 3;
			inc_sim_time(5);

		}
		set_flags(bcp_temp);
		
		break;

	// CP A,x
	case CP_IMMED:
		sprintf((char *)print_buffer, "CP A,#%02x\n", get_data_memory_byte(register_pc+1));
		bcp_temp = get_data_memory_byte(register_pc+1);
		if(bcp_temp > register_a) {
			register_cc |= CARRY_BIT;
		} else {
			register_cc &= ~CARRY_BIT;
		}
		bcp_temp = register_a - bcp_temp;
		set_flags(bcp_temp);
		// increment pc
		register_pc += 2;
		inc_sim_time(2);
		
		break;

	case CP_SHORT:
		if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "CP A,[%02x]\n", indirect_address);
			inc_sim_time(5);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "CP A,%02x\n", short_address);
			inc_sim_time(3);

		}
		bcp_temp = get_data_memory_byte(short_address);
		if(bcp_temp > register_a) {
			register_cc |= CARRY_BIT;
		} else {
			register_cc &= ~CARRY_BIT;
		}
		bcp_temp = register_a - bcp_temp;
		set_flags(bcp_temp);
		// increment pc
		register_pc += 2;
		
		break;

	case CP_LONG:
		if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);

			
			bcp_temp = get_data_memory_byte(long_address);
			if(bcp_temp > register_a) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_a - bcp_temp;
			sprintf((char *)print_buffer, "CP A,[%02x.w] []=%04x (%04x)=%02x {%02x-%02x=%02x}\n", indirect_address, long_address, long_address, get_data_memory_byte(long_address), register_a, get_data_memory_byte(long_address), bcp_temp);
			
			// increment pc
			register_pc += 2;
			inc_sim_time(6);
			precode_92 = 0;

		} else {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			
			bcp_temp = get_data_memory_byte(long_address);
			if(bcp_temp > register_a) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_a - bcp_temp;
			sprintf((char *)print_buffer, "CP A,%04x {%02x-%02x=%02x}\n", long_address, register_a, get_data_memory_byte(long_address), bcp_temp);
			
			// increment pc
			register_pc += 3;
			inc_sim_time(4);

		}
		set_flags(bcp_temp);
		break;

	case CP_REG_IND:
		if(precode_90) {
			sprintf((char *)print_buffer, "CP A,(Y)\n");
			bcp_temp = get_data_memory_byte(register_y);
			if(bcp_temp > register_a) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_a - bcp_temp;
			inc_sim_time(4);
			precode_90 = 0;

		} else {
			sprintf((char *)print_buffer, "CP A,(X)\n");
			bcp_temp = get_data_memory_byte(register_x);
			if(bcp_temp > register_a) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_a - bcp_temp;
			inc_sim_time(4);

		}
		set_flags(bcp_temp);
		// increment pc
		register_pc++;
		
		break;

	case CP_REG_IND_OFF_SHORT:
		if(precode_90) {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "CP A,(%02x,Y)\n", short_address);
			bcp_temp = get_data_memory_byte(short_address+register_y);
			if(bcp_temp > register_a) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_a - bcp_temp;
			inc_sim_time(4);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "CP A,([%02x],Y)\n", indirect_address);
			bcp_temp = get_data_memory_byte(short_address+register_y);
			if(bcp_temp > register_a) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_a - bcp_temp;
			inc_sim_time(6);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "CP A,([%02x],X)\n", indirect_address);
			bcp_temp = get_data_memory_byte(short_address+register_x);
			if(bcp_temp > register_a) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_a - bcp_temp;
			inc_sim_time(6);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "CP A,(%02x,X)\n", short_address);
			bcp_temp = get_data_memory_byte(short_address+register_x);
			if(bcp_temp > register_a) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_a - bcp_temp;
			inc_sim_time(4);

		}
		set_flags(bcp_temp);
		// increment pc
		register_pc += 2;
		
		break;

	case CP_REG_IND_OFF_LONG:
		if(precode_72) {

			short_indirect_address = (get_data_memory_byte(register_pc+1) << 8);
			short_indirect_address |= get_data_memory_byte(register_pc+2);

			long_address = get_data_memory_byte(short_indirect_address) << 8;
			long_address |= get_data_memory_byte(short_indirect_address+1);
			sprintf((char *)print_buffer, "CP A,([%04x],X) (LA=%08x, EA=%08x)\n", short_indirect_address, long_address, long_address+register_x);
			bcp_temp = get_data_memory_byte(long_address+register_x);
			if(bcp_temp > register_a) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_a - bcp_temp;
			// increment pc
			register_pc += 3;
			inc_sim_time(8);
			precode_72 = 0;

		} else if(precode_90) {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "CP A,(%04x,Y)\n", long_address);
			bcp_temp = get_data_memory_byte(long_address+register_y);
			if(bcp_temp > register_a) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_a - bcp_temp;
			// increment pc
			register_pc += 3;
			inc_sim_time(5);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "CP A,([%02x.w],Y)\n", indirect_address);
			bcp_temp = get_data_memory_byte(long_address+register_y);
			if(bcp_temp > register_a) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_a - bcp_temp;
			// increment pc
			register_pc += 2;
			inc_sim_time(7);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "CP A,([%02x.w],X)\n", indirect_address);
			bcp_temp = get_data_memory_byte(long_address+register_x);
			if(bcp_temp > register_a) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_a - bcp_temp;
			// increment pc
			register_pc += 2;
			inc_sim_time(7);
			precode_92 = 0;

		} else {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "CP A,(%04x, X)\n", long_address);
			bcp_temp = get_data_memory_byte(long_address+register_x);
			if(bcp_temp > register_a) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_a - bcp_temp;
			// increment pc
			register_pc += 3;
			inc_sim_time(5);

		}
		set_flags(bcp_temp);
		if(bcp_temp > register_a) {
			register_cc |= CARRY_BIT;
		} else {
			register_cc &= ~CARRY_BIT;
		}
		
		break;

	// CP X,x
	case CP_X_IMMED:
		if(precode_90) {
			sprintf((char *)print_buffer, "CP Y,#%02x\n", get_data_memory_byte(register_pc+1));
			bcp_temp = get_data_memory_byte(register_pc+1);
			if(bcp_temp > register_y) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_y - bcp_temp;
			set_flags(bcp_temp);
			inc_sim_time(3);
			precode_90 = 0;

		} else {
			sprintf((char *)print_buffer, "CP X,#%02x\n", get_data_memory_byte(register_pc+1));
			bcp_temp = get_data_memory_byte(register_pc+1);
			if(bcp_temp > register_x) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_x - bcp_temp;
			set_flags(bcp_temp);
			inc_sim_time(2);
		}
		// increment pc
		register_pc += 2;
		
		break;

	case CP_X_SHORT:
		if(precode_90) {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "CP Y,%02x\n", short_address);
			bcp_temp = get_data_memory_byte(short_address);
			if(bcp_temp > register_y) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_y - bcp_temp;
			set_flags(bcp_temp);
			inc_sim_time(4);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "CP Y,[%02x]\n", indirect_address);
			bcp_temp = get_data_memory_byte(short_address);
			if(bcp_temp > register_y) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_y - bcp_temp;
			set_flags(bcp_temp);
			inc_sim_time(5);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "CP X,[%02x]\n", indirect_address);
			bcp_temp = get_data_memory_byte(short_address);
			if(bcp_temp > register_x) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_x - bcp_temp;
			set_flags(bcp_temp);
			inc_sim_time(5);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "CP X,%02x\n", short_address);
			bcp_temp = get_data_memory_byte(short_address);
			if(bcp_temp > register_x) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_x - bcp_temp;
			set_flags(bcp_temp);
			inc_sim_time(3);
		}
		// increment pc
		register_pc += 2;
		
		break;

	case CP_X_LONG:
		if(precode_90) {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "CP Y,%04x\n", long_address);
			bcp_temp = get_data_memory_byte(long_address);
			if(bcp_temp > register_y) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_y - bcp_temp;
			set_flags(bcp_temp);
			// increment pc
			register_pc += 3;
			inc_sim_time(5);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "CP Y,[%02x.w]\n", indirect_address);
			bcp_temp = get_data_memory_byte(long_address);
			if(bcp_temp > register_a) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_y - bcp_temp;
			set_flags(bcp_temp);
			// increment pc
			register_pc += 2;
			inc_sim_time(6);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "CP X,[%02x.w]\n", indirect_address);
			bcp_temp = get_data_memory_byte(long_address);
			if(bcp_temp > register_x) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_x - bcp_temp;
			set_flags(bcp_temp);
			// increment pc
			register_pc += 2;
			inc_sim_time(6);
			precode_92 = 0;

		} else {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "CP X,%04x\n", long_address);
			bcp_temp = get_data_memory_byte(long_address);
			if(bcp_temp > register_x) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_x - bcp_temp;
			set_flags(bcp_temp);
			// increment pc
			register_pc += 3;
			inc_sim_time(4);

		}
		
		break;

	case CP_X_REG_IND:
		if(precode_90) {
			sprintf((char *)print_buffer, "CP Y,(Y)\n");
			bcp_temp = get_data_memory_byte(register_y);
			if(bcp_temp > register_y) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_y - bcp_temp;
			set_flags(bcp_temp);
			inc_sim_time(4);
			precode_90 = 0;

		} else {
			sprintf((char *)print_buffer, "CP X,(X)\n");
			bcp_temp = get_data_memory_byte(register_x);
			if(bcp_temp > register_x) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_x - bcp_temp;
			set_flags(bcp_temp);
			inc_sim_time(3);
		}
		// increment pc
		register_pc++;
		
		break;

	case CP_X_REG_IND_OFF_SHORT:
		if(precode_90) {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "CP Y,(%02x,Y)\n", short_address);
			bcp_temp = get_data_memory_byte(short_address+register_y);
			if(bcp_temp > register_y) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_y - bcp_temp;
			inc_sim_time(5);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "CP Y,([%02x],Y)\n", indirect_address);
			bcp_temp = get_data_memory_byte(short_address+register_y);
			if(bcp_temp > register_y) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_y - bcp_temp;
			inc_sim_time(5);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "CP X,([%02x],X)\n", indirect_address);
			bcp_temp = get_data_memory_byte(short_address+register_x);
			if(bcp_temp > register_x) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_x - bcp_temp;
			inc_sim_time(4);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "CP X,(%02x,X)\n", short_address);
			bcp_temp = get_data_memory_byte(short_address+register_x);
			if(bcp_temp > register_x) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_x - bcp_temp;
			inc_sim_time(4);

		}
		set_flags(bcp_temp);
		// increment pc
		register_pc += 2;
		
		break;

	case CP_X_REG_IND_OFF_LONG:
		if(precode_90) {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "CP Y,(%04x,Y)\n", long_address);
			bcp_temp = get_data_memory_byte(long_address+register_y);
			if(bcp_temp > register_y) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_y - bcp_temp;
			// increment pc
			register_pc += 3;
			inc_sim_time(6);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "CP Y,([%02x.w],Y)\n", indirect_address);
			bcp_temp = get_data_memory_byte(long_address+register_y);
			if(bcp_temp > register_y) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_y - bcp_temp;
			// increment pc
			register_pc += 2;
			inc_sim_time(7);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "CP X,([%02x.w],X)\n", indirect_address);
			bcp_temp = get_data_memory_byte(long_address+register_x);
			if(bcp_temp > register_x) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_x - bcp_temp;
			// increment pc
			register_pc += 2;
			inc_sim_time(5);
			precode_92 = 0;

		} else {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "CP X,(%04x, X)\n", long_address);
			bcp_temp = get_data_memory_byte(long_address+register_x);
			if(bcp_temp > register_x) {
				register_cc |= CARRY_BIT;
			} else {
				register_cc &= ~CARRY_BIT;
			}
			bcp_temp = register_x - bcp_temp;
			// increment pc
			register_pc += 3;
			inc_sim_time(5);

		}
		set_flags(bcp_temp);
		
		break;

	// OR A,x
	case OR_IMMED:
		sprintf((char *)print_buffer, "OR A,#%02x\n", get_data_memory_byte(register_pc+1));
		register_a |= get_data_memory_byte(register_pc+1);
		set_flags(register_a);
		// increment pc
		register_pc += 2;
		inc_sim_time(2);
		
		break;

	case OR_SHORT:
		if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "OR A,[%02x]\n", indirect_address);
			inc_sim_time(4);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "OR A,%02x\n", short_address);
			inc_sim_time(3);

		}
		register_a |= get_data_memory_byte(short_address);
		set_flags(register_a);
		// increment pc
		register_pc += 2;
		
		break;

	case OR_LONG:
		if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "OR A,[%02x.w]\n", indirect_address);
			register_a |= get_data_memory_byte(long_address);

			// increment pc
			register_pc += 2;
			inc_sim_time(7);
			precode_92 = 0;

		} else {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "OR A,%04x\n", long_address);
			register_a |= get_data_memory_byte(long_address);
			// increment pc
			register_pc += 3;
			inc_sim_time(5);

		}
		set_flags(register_a);
		
		break;

	case OR_REG_IND:
		if(precode_90) {
			sprintf((char *)print_buffer, "OR A,(Y)\n");
			register_a |= get_data_memory_byte(register_y);
			inc_sim_time(4);
			precode_90 = 0;

		} else {
			sprintf((char *)print_buffer, "OR A,(X)\n");
			register_a |= get_data_memory_byte(register_x);
			inc_sim_time(3);

		}
		set_flags(register_a);
		// increment pc
		register_pc++;
		
		break;

	case OR_REG_IND_OFF_SHORT:
		if(precode_90) {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "OR A,(%02x,Y)\n", short_address);
			register_a |= get_data_memory_byte(short_address+register_y);
			inc_sim_time(5);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "OR A,([%02x],Y)\n", indirect_address);
			register_a |= get_data_memory_byte(short_address+register_y);
			inc_sim_time(6);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "OR A,([%02x],X)\n", indirect_address);
			register_a |= get_data_memory_byte(short_address+register_x);
			inc_sim_time(6);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "OR A,(%02x,X)\n", short_address);
			register_a |= get_data_memory_byte(short_address+register_x);
			inc_sim_time(4);

		}
		set_flags(register_a);
		// increment pc
		register_pc += 2;
		
		break;

	case OR_REG_IND_OFF_LONG:
		if(precode_90) {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "OR A,(%04x,Y)\n", long_address);
			register_a |= get_data_memory_byte(long_address+register_y);
			// increment pc
			register_pc += 3;
			inc_sim_time(6);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "OR A,([%02x.w],Y)\n", indirect_address);
			register_a |= get_data_memory_byte(long_address+register_y);
			// increment pc
			register_pc += 2;
			inc_sim_time(7);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "OR A,([%02x.w],X)\n", indirect_address);
			register_a |= get_data_memory_byte(long_address+register_x);
			// increment pc
			register_pc += 2;
			inc_sim_time(7);
			precode_92 = 0;

		} else {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "OR A,(%04x, X)\n", long_address);
			register_a |= get_data_memory_byte(long_address+register_x);
			// increment pc
			register_pc += 3;
			inc_sim_time(5);

		}
		set_flags(register_a);
		
		break;

	// XOR A,x
	case XOR_IMMED:
		sprintf((char *)print_buffer, "XOR A,#%02x\n", get_data_memory_byte(register_pc+1));
		register_a ^= get_data_memory_byte(register_pc+1);
		set_flags(register_a);
		// increment pc
		register_pc += 2;
		inc_sim_time(2);
		
		break;

	case XOR_SHORT:
		if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "XOR A,[%02x]\n", indirect_address);
			inc_sim_time(5);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "XOR A,%02x\n", short_address);
			inc_sim_time(3);

		}
		register_a ^= get_data_memory_byte(short_address);
		set_flags(register_a);
		// increment pc
		register_pc += 2;
		
		break;

	case XOR_LONG:
		if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "XOR A,[%02x.w]\n", indirect_address);
			register_a ^= get_data_memory_byte(long_address);
			// increment pc
			register_pc += 2;
			inc_sim_time(7);
			precode_92 = 0;

		} else {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "XOR A,%04x\n", long_address);
			register_a ^= get_data_memory_byte(long_address);
			// increment pc
			register_pc += 3;
			inc_sim_time(5);

		}
		set_flags(register_a);
		
		break;

	case XOR_REG_IND:
		if(precode_90) {
			sprintf((char *)print_buffer, "XOR A,(Y)\n");
			register_a ^= get_data_memory_byte(register_y);
			inc_sim_time(4);
			precode_90 = 0;

		} else {
			sprintf((char *)print_buffer, "XOR A,(X)\n");
			register_a ^= get_data_memory_byte(register_x);
			inc_sim_time(3);

		}
		set_flags(register_a);
		// increment pc
		register_pc++;
		
		break;

	case XOR_REG_IND_OFF_SHORT:
		if(precode_90) {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "XOR A,(%02x,Y)\n", short_address);
			register_a ^= get_data_memory_byte(short_address+register_y);
			inc_sim_time(4);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "XOR A,([%02x],Y)\n", indirect_address);
			register_a ^= get_data_memory_byte(short_address+register_y);
			inc_sim_time(4);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "XOR A,([%02x],X)\n", indirect_address);
			register_a ^= get_data_memory_byte(short_address+register_x);
			inc_sim_time(4);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "XOR A,(%02x,X)\n", short_address);
			register_a ^= get_data_memory_byte(short_address+register_x);
			inc_sim_time(4);

		}
		set_flags(register_a);
		// increment pc
		register_pc += 2;
		
		break;

	case XOR_REG_IND_OFF_LONG:
		if(precode_90) {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "XOR A,(%04x,Y)\n", long_address);
			register_a ^= get_data_memory_byte(long_address+register_y);
			// increment pc
			register_pc += 3;
			inc_sim_time(7);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "XOR A,([%02x.w],Y)\n", indirect_address);
			register_a ^= get_data_memory_byte(long_address+register_y);
			// increment pc
			register_pc += 2;
			inc_sim_time(7);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "XOR A,([%02x.w],X)\n", indirect_address);
			register_a ^= get_data_memory_byte(long_address+register_x);
			// increment pc
			register_pc += 2;
			inc_sim_time(7);
			precode_92 = 0;

		} else {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "XOR A,(%04x, X)\n", long_address);
			register_a ^= get_data_memory_byte(long_address+register_x);
			// increment pc
			register_pc += 3;
			inc_sim_time(6);

		}
		set_flags(register_a);
		
		break;

	// SBC
	case SBC_IMMED:
		sprintf((char *)print_buffer, "SBC A,#%02x\n", get_data_memory_byte(register_pc+1));
		sbc(get_data_memory_byte(register_pc+1));
		// increment pc
		register_pc += 2;
		inc_sim_time(3);
		
		break;

	case SBC_SHORT:
		if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "SBC A,[%02x]\n", indirect_address);
			inc_sim_time(5);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "SBC A,%02x\n", short_address);
			inc_sim_time(3);

		}
		sbc(get_data_memory_byte(short_address));
		// increment pc
		register_pc += 2;
		
		break;

	case SBC_LONG:
		if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "SBC A,[%02x.w]\n", indirect_address);
			// increment pc
			register_pc += 2;
			inc_sim_time(5);
			precode_92 = 0;

		} else {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "SBC A,%04x\n", long_address);
			// increment pc
			register_pc += 3;
			inc_sim_time(4);

		}
		sbc(get_data_memory_byte(long_address));
		
		break;

	case SBC_REG_IND:
		if(precode_90) {
			sprintf((char *)print_buffer, "SBC A,(Y)\n");
			sbc(get_data_memory_byte(register_y));
			inc_sim_time(4);
			precode_90 = 0;

		} else {
			sprintf((char *)print_buffer, "SBC A,(X)\n");
			sbc(get_data_memory_byte(register_x));
			inc_sim_time(3);

		}
		// increment pc
		register_pc++;
		
		break;

	case SBC_REG_IND_OFF_SHORT:
		if(precode_90) {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "SBC A,(%02x,Y)\n", short_address);
			sbc(get_data_memory_byte(short_address+register_y));
			inc_sim_time(5);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "SBC A,([%02x],Y)\n", indirect_address);
			sbc(get_data_memory_byte(short_address+register_y));
			inc_sim_time(6);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "SBC A,([%02x],X)\n", indirect_address);
			sbc(get_data_memory_byte(short_address+register_x));
			inc_sim_time(6);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "SBC A,(%02x,X)\n", short_address);
			sbc(get_data_memory_byte(short_address+register_x));
			inc_sim_time(4);
		}
		// increment pc
		register_pc += 2;
		
		break;

	case SBC_REG_IND_OFF_LONG:
		if(precode_90) {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "SBC A,(%04x,Y)\n", long_address);
			sbc(get_data_memory_byte(long_address+register_y));
			// increment pc
			register_pc += 3;
			inc_sim_time(6);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "SBC A,([%02x.w],Y)\n", indirect_address);
			sbc(get_data_memory_byte(long_address+register_y));
			// increment pc
			register_pc += 2;
			inc_sim_time(7);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "SBC A,([%02x.w],X)\n", indirect_address);
			sbc(get_data_memory_byte(long_address+register_x));
			// increment pc
			register_pc += 2;
			inc_sim_time(7);
			precode_92 = 0;

		} else {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "SBC A,(%04x, X)\n", long_address);
			sbc(get_data_memory_byte(long_address+register_x));
			// increment pc
			register_pc += 3;
			inc_sim_time(5);

		}
		register_a -= register_cc & CARRY_BIT;
		set_flags(register_a);
		
		break;

	// SUB
	case SUB_IMMED:
		sprintf((char *)print_buffer, "SUB A,#%02x\n", get_data_memory_byte(register_pc+1));
		bcp_temp = get_data_memory_byte(register_pc+1);
		if(bcp_temp > register_a) {
			register_cc |= CARRY_BIT;
		} else {
			register_cc &= ~CARRY_BIT;
		}
		register_a -= bcp_temp;
		set_flags(register_a);
		// increment pc
		register_pc += 2;
		inc_sim_time(2);
		
		break;

	case SUB_SHORT:
		if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "SUB A,[%02x]\n", indirect_address);
			inc_sim_time(5);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "SUB A,%02x\n", short_address);
			inc_sim_time(3);

		}
		bcp_temp = get_data_memory_byte(short_address);
		if(bcp_temp > register_a) {
			register_cc |= CARRY_BIT;
		} else {
			register_cc &= ~CARRY_BIT;
		}
		register_a -= bcp_temp;
		set_flags(register_a);
		// increment pc
		register_pc += 2;
		
		break;

	case SUB_LONG:
		if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "SUB A,[%02x.w]\n", indirect_address);
			bcp_temp = get_data_memory_byte(long_address);
			// increment pc
			register_pc += 2;
			inc_sim_time(7);
			precode_92 = 0;

		} else {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "SUB A,%04x\n", long_address);
			bcp_temp = get_data_memory_byte(long_address);
			// increment pc
			register_pc += 3;
			inc_sim_time(5);

		}
		if(bcp_temp > register_a) {
			register_cc |= CARRY_BIT;
		} else {
			register_cc &= ~CARRY_BIT;
		}
		register_a -= bcp_temp;
		set_flags(register_a);
		
		break;

	case SUB_REG_IND:
		if(precode_90) {
			sprintf((char *)print_buffer, "SUB A,(Y)\n");
			bcp_temp = get_data_memory_byte(register_y);
			inc_sim_time(4);
			precode_90 = 0;

		} else {
			sprintf((char *)print_buffer, "SUB A,(X)\n");
			bcp_temp = get_data_memory_byte(register_x);
			inc_sim_time(3);

		}
		if(bcp_temp > register_a) {
			register_cc |= CARRY_BIT;
		} else {
			register_cc &= ~CARRY_BIT;
		}
		register_a -= bcp_temp;
		set_flags(register_a);
		// increment pc
		register_pc++;
		
		break;

	case SUB_REG_IND_OFF_SHORT:
		if(precode_90) {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "SUB A,(%02x,Y)\n", short_address);
			bcp_temp = get_data_memory_byte(short_address+register_y);
			inc_sim_time(6);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "SUB A,([%02x],Y)\n", indirect_address);
			bcp_temp = get_data_memory_byte(short_address+register_y);
			inc_sim_time(7);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "SUB A,([%02x],X)\n", indirect_address);
			bcp_temp = get_data_memory_byte(short_address+register_x);
			inc_sim_time(7);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "SUB A,(%02x,X)\n", short_address);
			bcp_temp = get_data_memory_byte(short_address+register_x);
			inc_sim_time(5);

		}
		if(bcp_temp > register_a) {
			register_cc |= CARRY_BIT;
		} else {
			register_cc &= ~CARRY_BIT;
		}
		register_a -= bcp_temp;
		set_flags(register_a);
		// increment pc
		register_pc += 2;
		
		break;

	case SUB_REG_IND_OFF_LONG:
		if(precode_90) {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "SUB A,(%04x,Y)\n", long_address);
			bcp_temp = get_data_memory_byte(long_address+register_y);
			// increment pc
			register_pc += 3;
			inc_sim_time(7);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "SUB A,([%02x.w],Y)\n", indirect_address);
			bcp_temp = get_data_memory_byte(long_address+register_y);
			// increment pc
			register_pc += 2;
			inc_sim_time(8);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "SUB A,([%02x.w],X)\n", indirect_address);
			bcp_temp = get_data_memory_byte(long_address+register_x);
			// increment pc
			register_pc += 2;
			inc_sim_time(8);
			precode_92 = 0;

		} else {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "SUB A,(%04x, X)\n", long_address);
			bcp_temp = get_data_memory_byte(long_address+register_x);
			// increment pc
			register_pc += 3;
			inc_sim_time(6);

		}
		if(bcp_temp > register_a) {
			register_cc |= CARRY_BIT;
		} else {
			register_cc &= ~CARRY_BIT;
		}
		register_a -= bcp_temp;
		set_flags(register_a);
		
		break;

	case LD_A_X:
		if(precode_90) {
			sprintf((char *)print_buffer, "LD A,Y\n");
			register_a = register_y;
			precode_90 = 0;

		} else {
			sprintf((char *)print_buffer, "LD A,X\n");
			register_a = register_x;
		}
		set_flags(register_a);
		// increment pc
		register_pc++;
		inc_sim_time(2);
		
		break;

	case LD_X_A:
		if(precode_90) {
			sprintf((char *)print_buffer, "LD Y,A\n");
			register_y = register_a;
			precode_90 = 0;

		} else {
			sprintf((char *)print_buffer, "LD X,A\n");
			register_x = register_a;
		}
		set_flags(register_a);
		// increment pc
		register_pc++;
		inc_sim_time(2);
		
		break;

	case LD_X_Y:
		if(precode_90) {
			sprintf((char *)print_buffer, "LD Y,X\n");
			register_y = register_x;
			precode_90 = 0;

		} else {
			sprintf((char *)print_buffer, "LD X,Y\n");
			register_x = register_y;
		}
		set_flags(register_x);
		// increment pc
		register_pc++;
		inc_sim_time(2);
		
		break;

	case LD_A_S:
		sprintf((char *)print_buffer, "LD A,S\n");
		register_a = register_sp & 0xff;
		set_flags(register_a);
		// increment pc
		register_pc++;
		inc_sim_time(2);
		
		break;

	case LD_S_A:
		sprintf((char *)print_buffer, "LD S,A\n");
		register_sp &= 0xff00;
		register_sp |= register_a;
		set_flags(register_a);
		// increment pc
		register_pc++;
		inc_sim_time(2);
		
		break;

	case LD_X_S:
		sprintf((char *)print_buffer, "LD X,S\n");
		register_x = register_sp & 0xff;
		set_flags(register_a);
		// increment pc
		register_pc++;
		inc_sim_time(2);
		
		break;

	case LD_S_X:
		sprintf((char *)print_buffer, "LD S,X\n");
		register_sp &= 0xff00;
		register_sp |= register_x;
		set_flags(register_x);
		// increment pc
		register_pc++;
		inc_sim_time(2);
		
		break;

	case LD_A_IMMED:
		sprintf((char *)print_buffer, "LD A,#%02x\n", get_data_memory_byte(register_pc+1));
		register_a = get_data_memory_byte(register_pc+1);
		set_flags(register_a);
		// increment pc
		register_pc += 2;
		inc_sim_time(2);
		
		break;

	case LD_A_SHORT:
		if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "LD A,[%02x]\n", indirect_address);
			short_address = get_data_memory_byte(indirect_address);
			inc_sim_time(6);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "LD A,%02x\n", short_address);
			inc_sim_time(6);

		}
		register_a = get_data_memory_byte(short_address);
		set_flags(register_a);
		// increment pc
		register_pc += 2;
		
		break;

	case LD_A_LONG:			// 0xc6
		if(precode_72) {

			sprintf((char *)print_buffer, "LD A,[xxxx.w] (st8) goes here\n");

			// unhandled precode handler will catch this

		} else if(precode_92) {
			// [long.w]
			indirect_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "LD A,[%02x.w]\n", indirect_address);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			// increment pc
			register_pc += 2;
			inc_sim_time(7);
			precode_92 = 0;

		} else {
			// long
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "LD A,%04x\n", long_address);
			// increment pc
			register_pc += 3;
			inc_sim_time(6);

		}
		register_a = get_data_memory_byte(long_address);
		set_flags(register_a);
		
		break;

	case LD_A_REG_IND:
		if(precode_90) {
			sprintf((char *)print_buffer, "LD A,(Y)\n");
			register_a = get_data_memory_byte(register_y);
			inc_sim_time(7);
			precode_90 = 0;

		} else {
			sprintf((char *)print_buffer, "LD A,(X)\n");
			register_a = get_data_memory_byte(register_x);
			inc_sim_time(5);

		}
		set_flags(register_a);
		// increment pc
		register_pc++;
		
		break;

	case LD_A_REG_IND_OFF_SHORT:
		if(precode_90) {
			short_address = get_data_memory_byte(register_pc+1);

			sprintf((char *)print_buffer, "LD A,(%02x,Y)\n", short_address);
			register_a = get_data_memory_byte(short_address+register_y);
			inc_sim_time(6);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1); 
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "LD A,([%02x],Y)\n", indirect_address);
			register_a = get_data_memory_byte(short_address+register_y);
			inc_sim_time(7);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1); 
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "LD A,([%02x],X)\n", indirect_address);
			register_a = get_data_memory_byte(short_address+register_x);
			inc_sim_time(7);
			precode_92 = 0;
	
		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "LD A,(%02x,X)\n", short_address);
			register_a = get_data_memory_byte(short_address+register_x);
			inc_sim_time(5);
		}
		set_flags(register_a);
		// increment pc
		register_pc += 2;
		
		break;

	case LD_A_REG_IND_OFF_LONG:
		if(precode_90) {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "LD A,(%04x,Y)\n", long_address);
			register_a = get_data_memory_byte(long_address+register_y);
			// increment pc
			register_pc += 3;
			inc_sim_time(8);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "LD A,([%02x.w],Y) []=%02x LA=%04x Y=%02x EA=%08x\n", indirect_address, indirect_address, long_address, register_y, long_address+register_y);
			register_a = get_data_memory_byte(long_address+register_y);
			// increment pc
			register_pc += 2;
			inc_sim_time(8);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "LD A,([%02x.w],X)\n", indirect_address);
			register_a = get_data_memory_byte(long_address+register_x);
			// increment pc
			register_pc += 2;
			inc_sim_time(7);
			precode_92 = 0;

		} else {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "LD A,(%04x,X)\n", long_address);
			register_a = get_data_memory_byte(long_address+register_x);
			// increment pc
			register_pc += 3;
			inc_sim_time(5);

		}
		set_flags(register_a);
		
		break;

	case LD_A_SP_IND:	//				0x7b
		short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "LD A,(%02x,SP)\n", short_address);
		register_a = get_data_memory_byte(short_address+register_sp);
		inc_sim_time(5);
		set_flags(register_a);
		// increment pc
		register_pc += 2;
		
		break;

	// LD x,A
	case LD_SHORT_A:
		if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "LD [%02x],A\n", indirect_address);
			inc_sim_time(5);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "LD %02x,A\n", short_address);
			inc_sim_time(4);

		}
		put_data_memory_byte(short_address, register_a);
		set_flags(register_a);
		// increment pc
		register_pc += 2;
		
		break;

	case LD_LONG_A:
		if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "LD [%02x.w],A\n", indirect_address);
			put_data_memory_byte(long_address, register_a);
			set_flags(register_a);
			// increment pc
			register_pc += 2;
			inc_sim_time(7);
			precode_92 = 0;

		} else {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "LD %04x,A\n", long_address);
			put_data_memory_byte(long_address, register_a);
			set_flags(register_a);			
			// increment pc
			register_pc += 3;
			inc_sim_time(5);
		}
		
		break;

	case LD_REG_IND_A:
		if(precode_90) {
			sprintf((char *)print_buffer, "LD (Y),A\n");
			put_data_memory_byte(register_y, register_a);
			inc_sim_time(7);
			precode_90 = 0;

		} else {
			sprintf((char *)print_buffer, "LD (X),A\n");
			put_data_memory_byte(register_x, register_a);
			inc_sim_time(5);

		}
		set_flags(register_a);
		// increment pc
		register_pc++;
		
		break;

	case LD_REG_IND_OFF_SHORT_A:
		if(precode_90) {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "LD (%02x,Y),A\n", short_address);
			put_data_memory_byte(short_address+register_y, register_a);
			inc_sim_time(6);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "LD ([%02x],Y),A\n", indirect_address);
			put_data_memory_byte(short_address+register_y, register_a);
			inc_sim_time(7);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "LD ([%02x],X),A\n", indirect_address);
			put_data_memory_byte(short_address+register_x, register_a);
			inc_sim_time(7);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "LD (%02x,X),A\n", short_address);
			put_data_memory_byte(short_address+register_x, register_a);
			inc_sim_time(5);

		}
		set_flags(register_a);
		// increment pc
		register_pc += 2;
		
		break;

	case LD_REG_IND_OFF_LONG_A:
		if(precode_72) {
			short_indirect_address = (get_data_memory_byte(register_pc+1) << 8);
			short_indirect_address |= get_data_memory_byte(register_pc+2);
			long_address = get_data_memory_byte(short_indirect_address) << 8;
			long_address |= get_data_memory_byte(short_indirect_address+1);
			sprintf((char *)print_buffer, "LD ([%04x],X),A (LA=%08x, EA=%08x)\n", short_indirect_address, long_address, long_address+register_x);
			put_data_memory_byte(long_address+register_x, register_a);
			// increment pc
			register_pc += 3;
			inc_sim_time(8);
			precode_72 = 0;

		} else if(precode_90) {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "LD (%04x,Y),A\n", long_address);
			put_data_memory_byte(long_address+register_y, register_a);
			// increment pc
			register_pc += 3;
			inc_sim_time(8);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "LD ([%02x.w],Y),A\n", indirect_address);
			put_data_memory_byte(long_address+register_y, register_a);
			// increment pc
			register_pc += 2;
			inc_sim_time(8);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "LD ([%02x.w],X),A\n", indirect_address);
			put_data_memory_byte(long_address+register_x, register_a);
			// increment pc
			register_pc += 2;
			inc_sim_time(7);
			precode_92 = 0;

		} else {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "LD (%04x,X),A\n", long_address);
			put_data_memory_byte(long_address+register_x, register_a);
			// increment pc
			register_pc += 3;
			inc_sim_time(7);
		}
		set_flags(register_a);
		
		break;

	case LD_SP_IND_A:	//				0x6b
		// st8
		short_address = get_data_memory_byte(register_pc+1);
		sprintf((char *)print_buffer, "LD (%02x,SP),A\n", short_address);
		put_data_memory_byte(short_address+register_sp, register_a);
		inc_sim_time(5);
		set_flags(register_a);
		// increment pc
		register_pc += 2;
		
		break;

	// LD X, x	// 0xae
	case LD_X_IMMED:
		if(precode_90) {
			sprintf((char *)print_buffer, "LD Y,#%02x\n", get_data_memory_byte(register_pc+1));
			register_y = get_data_memory_byte(register_pc+1);
			set_flags(register_y);
			inc_sim_time(3);
			precode_90 = 0;
		} else {
			sprintf((char *)print_buffer, "LD X,#%02x\n", get_data_memory_byte(register_pc+1));
			register_x = get_data_memory_byte(register_pc+1);
			set_flags(register_x);
			inc_sim_time(2);
		}
		// increment pc
		register_pc += 2;
		
		break;

	case LD_X_SHORT:
		if(precode_90) {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "LD Y,%02x\n", short_address);
			register_y = get_data_memory_byte(short_address);
			set_flags(register_y);
			inc_sim_time(4);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "LD Y,[%02x]\n", indirect_address);
			register_y = get_data_memory_byte(short_address);
			set_flags(register_y);
			inc_sim_time(5);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "LD X,[%02x]\n", indirect_address);
			register_x = get_data_memory_byte(short_address);
			set_flags(register_x);
			inc_sim_time(5);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "LD X,%02x\n", short_address);
			register_x = get_data_memory_byte(short_address);
			set_flags(register_x);
			inc_sim_time(3);

		}
		// increment pc
		register_pc += 2;
		
		break;

	case LD_X_LONG:
		if(precode_90) {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "LD Y,%04x\n", long_address);
			register_y = get_data_memory_byte(long_address);
			set_flags(register_y);
			// increment pc
			register_pc += 3;
			inc_sim_time(5);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "LD Y,[%02x.w]\n", indirect_address);
			register_y = get_data_memory_byte(long_address);
			set_flags(register_y);
			// increment pc
			register_pc += 2;
			inc_sim_time(5);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "LD X,[%02x.w]\n", indirect_address);
			register_x = get_data_memory_byte(long_address);
			set_flags(register_x);
			// increment pc
			register_pc += 2;
			inc_sim_time(5);
			precode_92 = 0;

		} else {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "LD X,%04x\n", long_address);
			register_x = get_data_memory_byte(long_address);
			set_flags(register_x);
			// increment pc
			register_pc += 3;
			inc_sim_time(4);
		}
		
		break;

	case LD_X_REG_IND:
		if(precode_90) {
			sprintf((char *)print_buffer, "LD Y,(Y)\n");
			register_y = get_data_memory_byte(register_y);
			set_flags(register_y);
			inc_sim_time(4);
			precode_90 = 0;
		} else {
			sprintf((char *)print_buffer, "LD X,(X)\n");
			register_x = get_data_memory_byte(register_x);
			set_flags(register_x);
			inc_sim_time(3);
		}
		// increment pc
		register_pc++;
		
		break;

	case LD_X_REG_IND_OFF_SHORT:
		if(precode_90) {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "LD Y,(%02x,Y)\n", short_address);
			register_y = get_data_memory_byte(short_address+register_y);
			set_flags(register_y);
			inc_sim_time(6);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "LD Y,([%02x],Y)\n", indirect_address);
			register_y = get_data_memory_byte(short_address+register_y);
			set_flags(register_y);
			inc_sim_time(7);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "LD X,([%02x],X)\n", indirect_address);
			register_x = get_data_memory_byte(short_address+register_x);
			set_flags(register_x);
			inc_sim_time(5);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "LD X,(%02x,X)\n", short_address);
			register_x = get_data_memory_byte(short_address+register_x);
			set_flags(register_x);
			inc_sim_time(4);

		}
		// increment pc
		register_pc += 2;
		
		break;

	case LD_X_REG_IND_OFF_LONG:
		if(precode_90) {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "LD Y,(%04x,Y)\n", long_address);
			register_y = get_data_memory_byte(long_address+register_y);
			set_flags(register_y);
			// increment pc
			register_pc += 3;
			inc_sim_time(6);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "LD Y,([%02x.w],Y)\n", indirect_address);
			register_y = get_data_memory_byte(long_address+register_y);
			set_flags(register_y);
			// increment pc
			register_pc += 2;
			inc_sim_time(7);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "LD X,([%02x.w],X)\n", indirect_address);
			register_x = get_data_memory_byte(long_address+register_x);
			set_flags(register_x);
			// increment pc
			register_pc += 2;
			inc_sim_time(7);
			precode_92 = 0;

		} else {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "LD X,(%04x,X)\n", long_address);
			register_x = get_data_memory_byte(long_address+register_x);
			set_flags(register_x);
			// increment pc
			register_pc += 3;
			inc_sim_time(5);
		}
		
		break;

	// LD x,X
	case LD_SHORT_X:
		if(precode_90) {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "LD %02x,Y\n", short_address);
			put_data_memory_byte(short_address, register_y);
			set_flags(register_y);
			inc_sim_time(5);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "LD [%02x],Y\n", indirect_address);
			put_data_memory_byte(short_address, register_y);
			set_flags(register_y);
			inc_sim_time(7);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "LD [%02x],X\n", indirect_address);
			put_data_memory_byte(short_address, register_x);
			set_flags(register_x);
			inc_sim_time(6);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "LD %02x,X\n", short_address);
			put_data_memory_byte(short_address, register_x);
			set_flags(register_x);
			inc_sim_time(4);
		}
		// increment pc
		register_pc += 2;
		
		break;

	case LD_LONG_X:
		if(precode_90) {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "LD %04x,Y\n", long_address);
			put_data_memory_byte(long_address, register_y);
			set_flags(register_y);
			// increment pc
			register_pc += 3;
			inc_sim_time(6);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "LD [%02x.w],Y\n", indirect_address);
			put_data_memory_byte(long_address, register_y);
			set_flags(register_y);
			// increment pc
			register_pc += 2;
			inc_sim_time(7);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "LD [%02x.w],X\n", indirect_address);
			put_data_memory_byte(long_address, register_x);
			set_flags(register_x);
			// increment pc
			register_pc += 2;
			inc_sim_time(6);
			precode_92 = 0;

		} else {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "LD %04x,X\n", long_address);
			put_data_memory_byte(long_address, register_x);
			set_flags(register_x);
			// increment pc
			register_pc += 3;
			inc_sim_time(5);

		}
		
		break;

	case LD_REG_IND_X:
		if(precode_90) {
			sprintf((char *)print_buffer, "LD (Y),Y\n");
			put_data_memory_byte(register_y, register_y);
			set_flags(register_y);
			precode_90 = 0;

		} else {
			sprintf((char *)print_buffer, "LD (X),X\n");
			put_data_memory_byte(register_x, register_x);
			set_flags(register_x);
		}
		// increment pc
		register_pc++;
		inc_sim_time(4);
		
		break;

	case LD_REG_IND_OFF_SHORT_X:
		if(precode_90) {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "LD (%02x,Y),Y\n", short_address);
			put_data_memory_byte(short_address+register_y, register_y);
			set_flags(register_y);
			inc_sim_time(5);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "LD ([%02x],Y),Y\n", indirect_address);
			put_data_memory_byte(short_address+register_y, register_y);
			set_flags(register_y);
			inc_sim_time(7);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "LD ([%02x],X),Y\n", indirect_address);
			put_data_memory_byte(short_address+register_x, register_x);
			set_flags(register_x);
			inc_sim_time(6);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "LD (%02x,X),X\n", short_address);
			put_data_memory_byte(short_address+register_x, register_x);
			set_flags(register_x);
			inc_sim_time(4);

		}
		// increment pc
		register_pc += 2;
		
		break;

	case LD_REG_IND_OFF_LONG_X:
		if(precode_90) {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "LD (%04x,Y),Y\n", long_address);
			put_data_memory_byte(long_address+register_y, register_y);
			set_flags(register_y);
			// increment pc
			register_pc += 3;
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "LD ([%02x],Y),Y\n", indirect_address);
			put_data_memory_byte(long_address+register_y, register_y);
			set_flags(register_y);
			// increment pc
			register_pc += 2;
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = (get_data_memory_byte(indirect_address) << 8);
			long_address |= get_data_memory_byte(indirect_address+1);
			sprintf((char *)print_buffer, "LD ([%02x],X),X\n", indirect_address);
			put_data_memory_byte(long_address+register_x, register_x);
			set_flags(register_x);
			// increment pc
			register_pc += 2;
			precode_92 = 0;

		} else {
			long_address = (get_data_memory_byte(register_pc+1) << 8);
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "LD (%04x,X),X\n", long_address);
			put_data_memory_byte(long_address+register_x, register_x);
			set_flags(register_x);
			// increment pc
			register_pc += 3;
		}
		set_flags(register_a);
		inc_sim_time(8);
		
		break;

	// CLR
	case CLR_A:
		if(precode_72) {
			// st8 has
			long_address = get_data_memory_byte(register_pc+1) << 8;
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "CLR (%08x,x)\n", long_address);
			put_data_memory_byte(long_address+register_x, 0);
			inc_sim_time(4);
			precode_72 = 0;
			// increment pc
			register_pc += 3;
			set_flags(0);

		} else  {
			sprintf((char *)print_buffer, "CLR A\n");
			register_a = 0;
			set_flags(register_a);
			// increment pc
			register_pc++;
			inc_sim_time(2);
		}
		
		break;

	case CLR_X:
		if(precode_72) {
			long_address = get_data_memory_byte(register_pc+1) << 8;
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "CLR [%08x]\n", long_address);
			put_data_memory_byte(long_address, 0);
			inc_sim_time(4);
			precode_72 = 0;
			// ncrement pc
			register_pc += 3;

		} else if(precode_90) {
			sprintf((char *)print_buffer, "CLR Y\n");
			register_y = 0;
			inc_sim_time(4);
			precode_90 = 0;
			// increment pc
			register_pc++;

		} else {
			sprintf((char *)print_buffer, "CLR X\n");
			register_x = 0;
			inc_sim_time(3);
			// increment pc
			register_pc++;
		}
		register_cc &= ~NEGATIVE_BIT;
		register_cc |= ZERO_BIT;
		
		break;

	case CLR_REG_IND:
		if(precode_90) {
			sprintf((char *)print_buffer, "CLR (Y)\n");
			put_data_memory_byte(register_y, 0);
			inc_sim_time(6);
			precode_90 = 0;

		} else {
			sprintf((char *)print_buffer, "CLR (X)\n");
			put_data_memory_byte(register_x, 0);
			inc_sim_time(5);

		}
		register_cc &= ~NEGATIVE_BIT;
		register_cc |= ZERO_BIT;
		// increment pc
		register_pc++;
		
		break;

	case CLR_SHORT:		// 0x3f
		if(precode_72) {
			// added 09/06/2017);
			short_indirect_address = get_data_memory_byte(register_pc+1) << 8;
			short_indirect_address |= get_data_memory_byte(register_pc+2);
			short_address = get_data_memory_byte(short_indirect_address);
			sprintf((char *)print_buffer, "CLR [%04x]\n", short_indirect_address);
			inc_sim_time(9);
			precode_72 = 0;
			register_pc++;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "CLR [%02x]\n", indirect_address);
			inc_sim_time(7);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "CLR %02x\n", short_address);
			inc_sim_time(5);

		}
		put_data_memory_byte(short_address, 0);
		register_cc &= ~NEGATIVE_BIT;
		register_cc |= ZERO_BIT;
		// increment pc
		register_pc += 2;
		
		break;

	case CLR_REG_IND_OFF_SHORT:
		if(precode_90) {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "CLR (%02x,Y)\n", short_address);
			put_data_memory_byte(short_address+register_y, 0);
			inc_sim_time(7);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "CLR ([%02x],Y)\n", indirect_address);
			put_data_memory_byte(short_address+register_y, 0);
			inc_sim_time(8);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "CLR ([%02x],X)\n", indirect_address);
			put_data_memory_byte(short_address+register_x, 0);
			inc_sim_time(8);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "CLR (%02x,X)\n", short_address);
			put_data_memory_byte(short_address+register_x, 0);
			inc_sim_time(6);

		}
		register_cc &= ~NEGATIVE_BIT;
		register_cc |= ZERO_BIT;
		// increment pc
		register_pc += 2;
		
		break;

	// RLC
	case RLC_A:	// 0x49
		sprintf((char *)print_buffer, "RLC A\n");
		register_a = rlc(register_a);
		// increment pc
		register_pc++;
		inc_sim_time(3);
		
		break;

	case RLC_X:	// 0x59
		if(precode_72) {
			// added 8/25/17 - st8
			// long
			long_address = get_data_memory_byte(register_pc+1) << 8;
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "RLC (%04x)\n", long_address);
			inc_sim_time(5);
			put_data_memory_byte(long_address, rlc(get_data_memory_byte(long_address)));
			precode_72 = 0;
			// increment pc
			register_pc += 2;
		} else if(precode_90) {
			// Y
			sprintf((char *)print_buffer, "RLC Y\n");
			register_y = rlc(register_y);
			inc_sim_time(4);
			precode_90 = 0;

		} else {
			// X
			sprintf((char *)print_buffer, "RLC X\n");
			register_x = rlc(register_x);
			inc_sim_time(3);

		}
		// increment pc
		register_pc++;
		
		break;

	case RLC_REG_IND:	// 0x79
		if(precode_90) {
			sprintf((char *)print_buffer, "RLC (Y)\n");
			put_data_memory_byte(register_y, rlc(get_data_memory_byte(register_y)));
			precode_90 = 0;
			inc_sim_time(6);

		} else {
			sprintf((char *)print_buffer, "RLC (X)\n");
			put_data_memory_byte(register_x, rlc(get_data_memory_byte(register_x)));
			inc_sim_time(5);

		}
		// increment pc
		register_pc++;
		
		break;

	case RLC_SHORT:	// 0x39
		if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "RLC [%02x]\n", indirect_address);
			inc_sim_time(7);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "RLC %02x\n", short_address);
			inc_sim_time(5);

		}
		put_data_memory_byte(short_address, rlc(get_data_memory_byte(short_address)));
		// increment pc
		register_pc += 2;
		
		break;

	case RLC_REG_IND_OFF_SHORT:	// 0x69
		if(precode_90) {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "RLC (%02x,Y)\n", short_address);
			put_data_memory_byte(short_address+register_y, rlc(get_data_memory_byte(short_address+register_y)));
			inc_sim_time(8);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "RLC ([%02x],Y)\n", indirect_address);
			put_data_memory_byte(short_address+register_y, rlc(get_data_memory_byte(short_address+register_y)));
			inc_sim_time(8);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "RLC ([%02x],X)\n", indirect_address);
			put_data_memory_byte(short_address+register_x, rlc(get_data_memory_byte(short_address+register_x)));
			inc_sim_time(7);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "RLC (%02x,X)\n", short_address);
			put_data_memory_byte(short_address+register_x, rlc(get_data_memory_byte(short_address+register_x)));
			inc_sim_time(7);

		}
		// increment pc
		register_pc += 2;
		
		break;

	// RRC
	case RRC_A:
		sprintf((char *)print_buffer, "RRC A\n");
		register_a = rrc(register_a);
		// increment pc
		register_pc++;
		inc_sim_time(3);
		
		break;

	case RRC_X:
		if(precode_90) {
			sprintf((char *)print_buffer, "RRC Y\n");
			register_y = rrc(register_y);
			inc_sim_time(4);
			precode_90 = 0;

		} else {
			sprintf((char *)print_buffer, "RRC X\n");
			register_x = rrc(register_x);
			inc_sim_time(3);

		}
		// increment pc
		register_pc++;
		
		break;

	case RRC_REG_IND:
		if(precode_90) {
			sprintf((char *)print_buffer, "RRC (Y)\n");
			put_data_memory_byte(register_y, rrc(get_data_memory_byte(register_y)));
			inc_sim_time(6);
			precode_90 = 0;

		} else {
			sprintf((char *)print_buffer, "RRC (X)\n");
			put_data_memory_byte(register_x, rrc(get_data_memory_byte(register_x)));
			inc_sim_time(5);

		}
		// increment pc
		register_pc++;
		
		break;

	case RRC_SHORT:
		if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "RRC [%02x]\n", indirect_address);
			inc_sim_time(7);
			precode_92 = 0;
	
		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "RRC %02x\n", short_address);
			inc_sim_time(5);

		}
		put_data_memory_byte(short_address, rrc(get_data_memory_byte(short_address)));
		// increment pc
		register_pc += 2;
		
		break;

	case RRC_REG_IND_OFF_SHORT:
		if(precode_90) {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "RRC (%02x,Y)\n", short_address);
			put_data_memory_byte(short_address+register_y, rrc(get_data_memory_byte(short_address+register_y)));
			inc_sim_time(8);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "RRC ([%02x],Y)\n", indirect_address);
			put_data_memory_byte(short_address+register_y, rrc(get_data_memory_byte(short_address+register_y)));
			inc_sim_time(8);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "RRC ([%02x],X)\n", indirect_address);
			put_data_memory_byte(short_address+register_x, rrc(get_data_memory_byte(short_address+register_x)));
			inc_sim_time(7);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "RRC (%02x,X)\n", short_address);
			put_data_memory_byte(short_address+register_x, rrc(get_data_memory_byte(short_address+register_x)));
			inc_sim_time(6);

		}
		register_cc &= ~NEGATIVE_BIT;
		register_cc |= ZERO_BIT;
		// increment pc
		register_pc += 2;
		
		break;

	// SLA
	case SLA_A:	// 0x48
		sprintf((char *)print_buffer, "SLA A\n");
		register_a = sla(register_a);
		// increment pc
		register_pc++;
		inc_sim_time(3);
		
		break;

	case SLA_X:	// 0x58
		if(precode_72) {
			// added 8/25/17 st8
			// long
			long_address = get_data_memory_byte(register_pc+1) << 8;
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "SLA (%04x)\n", long_address);
			inc_sim_time(5);
			put_data_memory_byte(long_address, sla(get_data_memory_byte(long_address)));
			precode_72 = 0;
			// increment pc
			register_pc += 2;
		} else if(precode_90) {
			// Y
			sprintf((char *)print_buffer, "SLA Y\n");
			register_y = sla(register_y);
			inc_sim_time(4);
			precode_90 = 0;

		} else {
			// X
			sprintf((char *)print_buffer, "SLA X\n");
			register_x = sla(register_x);
			inc_sim_time(3);

		}
		// increment pc
		register_pc++;
		
		break;

	case SLA_REG_IND:	// 0x78
		if(precode_90) {
			sprintf((char *)print_buffer, "SLA (Y)\n");
			put_data_memory_byte(register_y, sla(get_data_memory_byte(register_y)));
			inc_sim_time(6);
			precode_90 = 0;

		} else {
			sprintf((char *)print_buffer, "SLA (X)\n");
			put_data_memory_byte(register_x, sla(get_data_memory_byte(register_x)));
			inc_sim_time(5);

		}
		// increment pc
		register_pc++;
		
		break;

	case SLA_SHORT:	// 0x38
		if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "SLA [%02x]\n", indirect_address);
			inc_sim_time(7);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "SLA %02x\n", short_address);
			inc_sim_time(5);

		}
		put_data_memory_byte(short_address, sla(get_data_memory_byte(short_address)));
		// increment pc
		register_pc += 2;
		
		break;

	case SLA_REG_IND_OFF_SHORT:	// 0x68
		if(precode_90) {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "SLA (%02x,Y)\n", short_address);
			put_data_memory_byte(short_address+register_y, sla(get_data_memory_byte(short_address+register_y)));
			inc_sim_time(7);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "SLA ([%02x],Y)\n", indirect_address);
			put_data_memory_byte(short_address+register_y, sla(get_data_memory_byte(short_address+register_y)));
			inc_sim_time(8);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "SLA ([%02x],X)\n", indirect_address);
			put_data_memory_byte(short_address+register_x, sla(get_data_memory_byte(short_address+register_x)));
			inc_sim_time(8);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "SLA (%02x,X)\n", short_address);
			put_data_memory_byte(short_address+register_x, sla(get_data_memory_byte(short_address+register_x)));
			inc_sim_time(6);

		}
		// increment pc
		register_pc += 2;
		
		break;

	// SRA
	case SRA_A:	// 0x47
		sprintf((char *)print_buffer, "SRA A\n");
		register_a = sra(register_a);
		// increment pc
		register_pc++;
		inc_sim_time(3);
		
		break;

	case SRA_X:	// 0x57
		if(precode_90) {
			sprintf((char *)print_buffer, "SRA Y\n");
			register_y = sra(register_y);
			inc_sim_time(4);
			precode_90 = 0;

		} else {
			sprintf((char *)print_buffer, "SRA X\n");
			register_x = sra(register_x);
			inc_sim_time(3);

		}
		// increment pc
		register_pc++;
		
		break;

	case SRA_REG_IND:	// 0x77
		if(precode_90) {
			sprintf((char *)print_buffer, "SRA (Y)\n");
			put_data_memory_byte(register_y, sra(get_data_memory_byte(register_y)));
			inc_sim_time(6);
			precode_90 = 0;

		} else {
			sprintf((char *)print_buffer, "SRA (X)\n");
			put_data_memory_byte(register_x, sra(get_data_memory_byte(register_x)));
			inc_sim_time(5);
		}
		// increment pc
		register_pc++;
		
		break;

	case SRA_SHORT:	// 0x37
		if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "SRA [%02x]\n", indirect_address);
			inc_sim_time(7);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "SRA %02x\n", short_address);
			inc_sim_time(5);

		}
		put_data_memory_byte(short_address, sra(get_data_memory_byte(short_address)));
		// increment pc
		register_pc += 2;
		
		break;

	case SRA_REG_IND_OFF_SHORT:	// 0x67
		if(precode_90) {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "SRA (%02x,Y)\n", short_address);
			put_data_memory_byte(short_address+register_y, sra(get_data_memory_byte(short_address+register_y)));
			inc_sim_time(7);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "SRA ([%02x],Y)\n", indirect_address);
			put_data_memory_byte(short_address+register_y, sra(get_data_memory_byte(short_address+register_y)));
			inc_sim_time(8);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "SRA ([%02x],X)\n", indirect_address);
			put_data_memory_byte(short_address+register_x, sra(get_data_memory_byte(short_address+register_x)));
			inc_sim_time(8);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "SRA (%02x,X)\n", short_address);
			put_data_memory_byte(short_address+register_x, sra(get_data_memory_byte(short_address+register_x)));
			inc_sim_time(6);

		}
		// increment pc
		register_pc += 2;
		break;

	// SRL
	case SRL_A:	// 0x44
		sprintf((char *)print_buffer, "SRL A\n");
		register_a = srl(register_a);
		// increment pc
		register_pc++;
		inc_sim_time(3);
		break;

	case SRL_X:	// 0x54
		if(precode_90) {
			sprintf((char *)print_buffer, "SRL Y\n");
			register_y = srl(register_y);
			inc_sim_time(4);
			precode_90 = 0;

		} else {
			sprintf((char *)print_buffer, "SRL X\n");
			register_x = srl(register_x);
			inc_sim_time(3);

		}
		// increment pc
		register_pc++;
		break;

	case SRL_REG_IND:	// 0x74
		if(precode_90) {
			sprintf((char *)print_buffer, "SRL (Y)\n");
			put_data_memory_byte(register_y, srl(get_data_memory_byte(register_y)));
			inc_sim_time(7);
			precode_90 = 0;

		} else {
			sprintf((char *)print_buffer, "SRL (X)\n");
			put_data_memory_byte(register_x, srl(get_data_memory_byte(register_x)));
			inc_sim_time(5);

		}
		// increment pc
		register_pc++;
		break;

	case SRL_SHORT:	// 0x34
		if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "SRL [%02x]\n", indirect_address);
			inc_sim_time(7);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "SRL %02x\n", short_address);
			inc_sim_time(5);

		}
		put_data_memory_byte(short_address, srl(get_data_memory_byte(short_address)));
		// increment pc
		register_pc += 2;
		break;

	case SRL_REG_IND_OFF_SHORT:	// 0x64
		if(precode_90) {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "SRL (%02x,Y)\n", short_address);
			put_data_memory_byte(short_address+register_y, srl(get_data_memory_byte(short_address+register_y)));
			inc_sim_time(7);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "SRL ([%02x],Y)\n", indirect_address);
			put_data_memory_byte(short_address+register_y, srl(get_data_memory_byte(short_address+register_y)));
			inc_sim_time(8);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "SRL ([%02x],X)\n", indirect_address);
			put_data_memory_byte(short_address+register_x, srl(get_data_memory_byte(short_address+register_x)));
			inc_sim_time(8);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "SRL (%02x,X)\n", short_address);
			put_data_memory_byte(short_address+register_x, srl(get_data_memory_byte(short_address+register_x)));
			inc_sim_time(6);

		}
		// increment pc
		register_pc += 2;
		break;

	// SWAP
	case SWAP_A:
		sprintf((char *)print_buffer, "SWAP A\n");
		temp = register_a;
		register_a = (temp >> 4);
		register_a |= (temp << 4);
		set_flags(register_a);

		// increment pc
		register_pc++;
		inc_sim_time(3);
		break;

	case SWAP_X:
		if(precode_90) {
			sprintf((char *)print_buffer, "SWAP Y\n");
			temp = register_y;
			register_y = (temp >> 4);
			register_y |= (temp << 4);
			set_flags(register_y);
			inc_sim_time(4);
			precode_90 = 0;

		} else {
			sprintf((char *)print_buffer, "SWAP X\n");
			temp = register_x;
			register_x = (temp >> 4);
			register_x |= (temp << 4);
			set_flags(register_x);
			inc_sim_time(3);

		}
		// increment pc
		register_pc++;
		
		break;

	case SWAP_REG_IND:
		if(precode_90) {
			sprintf((char *)print_buffer, "SWAP (Y)\n");
			temp = get_data_memory_byte(register_y);
			put_data_memory_byte(register_y, ((temp >> 4) | (temp << 4)));
			set_flags(prog_memory[register_y]);
			inc_sim_time(6);
			precode_90 = 0;

		} else {
			sprintf((char *)print_buffer, "SWAP (X)\n");
			temp = get_data_memory_byte(register_x);
			put_data_memory_byte(register_x, ((temp >> 4) | (temp << 4)));
			set_flags(prog_memory[register_x]);
			inc_sim_time(5);

		}
		// increment pc
		register_pc++;
		
		break;

	case SWAP_SHORT:
		if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "SWAP [%02x]\n", indirect_address);
			temp = get_data_memory_byte(short_address);
			put_data_memory_byte(short_address, ((temp >> 4) | (temp << 4)));
			set_flags(prog_memory[short_address]);
			inc_sim_time(7);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "SWAP %02x\n", short_address);
			temp = get_data_memory_byte(short_address);
			put_data_memory_byte(short_address, ((temp >> 4) | (temp << 4)));
			set_flags(prog_memory[short_address]);
			inc_sim_time(5);

		}
		// increment pc
		register_pc += 2;
		
		break;

	case SWAP_REG_IND_OFF_SHORT:
		if(precode_90) {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "SWAP (%02x,Y)\n", short_address);
			temp = get_data_memory_byte(short_address+register_y);
			put_data_memory_byte(short_address+register_y, ((temp >> 4) | (temp << 4)));
			set_flags(prog_memory[short_address+register_y]);
			inc_sim_time(7);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "SWAP ([%02x],Y)\n", indirect_address);
			temp = get_data_memory_byte(short_address+register_y);
			put_data_memory_byte(short_address+register_y, ((temp >> 4) | (temp << 4)));
			set_flags(prog_memory[short_address+register_y]);
			inc_sim_time(8);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "SWAP ([%02x],X)\n", indirect_address);
			temp = get_data_memory_byte(short_address+register_x);
			put_data_memory_byte(short_address+register_x, ((temp >> 4) | (temp << 4)));
			set_flags(prog_memory[short_address+register_x]);
			inc_sim_time(8);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "SWAP (%02x,X)\n", short_address);
			temp = get_data_memory_byte(short_address+register_x);
			put_data_memory_byte(short_address+register_x, ((temp >> 4) | (temp << 4)));
			set_flags(prog_memory[short_address+register_x]);
			inc_sim_time(6);

		}
		// increment pc
		register_pc += 2;
		
		break;

	// INC
	case INC_A:
		sprintf((char *)print_buffer, "INC A\n");
		register_a++;
		set_flags(register_a);
		// increment pc
		register_pc++;
		inc_sim_time(3);
		
		break;

	case INC_X:
		if(precode_72) {
			long_address = get_data_memory_byte(register_pc+1) << 8;
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "INC %08x\n", long_address);
			put_data_memory_byte(long_address, (get_data_memory_byte(long_address) + 1));
			set_flags(prog_memory[long_address]);
			inc_sim_time(6);
			precode_72 = 0;
			// increment pc
			register_pc += 3;

		} else if(precode_90) {
			sprintf((char *)print_buffer, "INC Y\n");
			register_y++;
			set_flags(register_y);
			inc_sim_time(4);
			precode_90 = 0;
			// increment pc
			register_pc++;

		} else {
			sprintf((char *)print_buffer, "INC X\n");
			register_x++;
			set_flags(register_x);
			inc_sim_time(3);
			// increment pc
			register_pc++;
		}
		
		break;

	case INC_REG_IND:
		if(precode_90) {
			sprintf((char *)print_buffer, "INC (Y)\n");
			put_data_memory_byte(register_y, (get_data_memory_byte(register_y) + 1));
			set_flags(prog_memory[register_y]);
			inc_sim_time(6);
			precode_90 = 0;

		} else {
			sprintf((char *)print_buffer, "INC (X)\n");
			put_data_memory_byte(register_x, (get_data_memory_byte(register_x) + 1));
			set_flags(prog_memory[register_x]);
			inc_sim_time(5);

		}
		// increment pc
		register_pc++;
		
		break;

	case INC_SHORT:		// 0x3c
		if(precode_72) {
			// added 09/06/2107 st8
			// [longptr.w]
			short_indirect_address = get_data_memory_byte(register_pc+1) << 8;
			short_indirect_address |= get_data_memory_byte(register_pc+2);
	
			short_address = get_data_memory_byte(short_indirect_address);

			sprintf((char *)print_buffer, "INC [%04x]\n", short_indirect_address);
			inc_sim_time(9);
			precode_72 = 0;
			register_pc++;
	
		} else if(precode_92) {
			// [short]
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "INC [%02x]\n", indirect_address);
			inc_sim_time(7);
			precode_92 = 0;

		} else {
			// short
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "INC %02x\n", short_address);
			inc_sim_time(5);

		}
		put_data_memory_byte(short_address, (get_data_memory_byte(short_address) + 1));
		set_flags(prog_memory[short_address]);
		// increment pc
		register_pc += 2;
		
		break;

	case INC_REG_IND_OFF_SHORT:
		if(precode_90) {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "INC (%02x,Y)\n", short_address);
			put_data_memory_byte(short_address+register_y, (get_data_memory_byte(short_address+register_y) + 1));
			set_flags(prog_memory[short_address+register_y]);
			inc_sim_time(7);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "INC ([%02x],Y)\n", indirect_address);
			put_data_memory_byte(short_address+register_y, (get_data_memory_byte(short_address+register_y) + 1));
			set_flags(prog_memory[short_address+register_y]);
			inc_sim_time(8);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "INC ([%02x],X)\n", indirect_address);
			put_data_memory_byte(short_address+register_x, (get_data_memory_byte(short_address+register_x) + 1));
			set_flags(prog_memory[short_address+register_x]);
			inc_sim_time(8);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "INC (%02x,X)\n", short_address);
			put_data_memory_byte(short_address+register_x, (get_data_memory_byte(short_address+register_x) + 1));
			set_flags(prog_memory[short_address+register_x]);
			inc_sim_time(6);

		}
		// increment pc
		register_pc += 2;
		
		break;

	// DEC
	case DEC_A:
		sprintf((char *)print_buffer, "DEC A\n");
		register_a--;
		set_flags(register_a);
		// increment pc
		register_pc++;
		inc_sim_time(2);

		break;
	case DEC_X:
		if(precode_72) {
			long_address = get_data_memory_byte(register_pc+1) << 8;
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "DEC %08x\n", long_address);
			put_data_memory_byte(long_address, (get_data_memory_byte(long_address) - 1));
			set_flags(prog_memory[long_address]);
			inc_sim_time(6);
			precode_72 = 0;
			// increment pc
			register_pc += 3;

		} else if(precode_90) {
			sprintf((char *)print_buffer, "DEC Y\n");
			register_y--;
			set_flags(register_y);
			inc_sim_time(4);
			precode_90 = 0;
			// increment pc
			register_pc++;

		} else {
			sprintf((char *)print_buffer, "DEC X\n");
			register_x--;
			set_flags(register_x);
			inc_sim_time(3);
			// increment pc
			register_pc++;
		}
		
		break;

	case DEC_REG_IND:
		if(precode_90) {
			sprintf((char *)print_buffer, "DEC (Y)\n");
			put_data_memory_byte(register_y, (get_data_memory_byte(register_y) - 1));
			inc_sim_time(7);
			precode_90 = 0;

		} else {
			sprintf((char *)print_buffer, "DEC (X)\n");
			put_data_memory_byte(register_x, (get_data_memory_byte(register_x) - 1));
			inc_sim_time(5);

		}
		// increment pc
		register_pc++;
		
		break;

	case DEC_SHORT:		// 0x3a
		if(precode_72) {
			//	added 09/06/2017; st8
			short_indirect_address = get_data_memory_byte(register_pc+1) << 8;
			short_indirect_address |= get_data_memory_byte(register_pc+2);
			short_address = get_data_memory_byte(short_indirect_address);
			sprintf((char *)print_buffer, "DEC [%04x]\n", short_indirect_address);
			inc_sim_time(9);
			precode_72 = 0;
			register_pc++;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "DEC [%02x]\n", indirect_address);
			inc_sim_time(7);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "DEC %02x\n", short_address);
			inc_sim_time(5);

		}
		put_data_memory_byte(short_address, (get_data_memory_byte(short_address) - 1));
		set_flags(prog_memory[short_address]);
		// increment pc
		register_pc += 2;
		break;

	case DEC_REG_IND_OFF_SHORT:
		if(precode_90) {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "DEC (%02x,Y)\n", short_address);
			put_data_memory_byte(short_address+register_y, (get_data_memory_byte(short_address+register_y) - 1));
			set_flags(prog_memory[short_address+register_y]);
			inc_sim_time(7);
			precode_90 = 0;
		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "DEC ([%02x],Y)\n", indirect_address);
			put_data_memory_byte(short_address+register_y, (get_data_memory_byte(short_address+register_y) - 1));
			set_flags(prog_memory[short_address+register_y]);
			inc_sim_time(8);
			precode_91 = 0;
		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "DEC ([%02x],X)\n", indirect_address);
			put_data_memory_byte(short_address+register_x, (get_data_memory_byte(short_address+register_x) - 1));
			set_flags(prog_memory[short_address+register_x]);
			inc_sim_time(8);
			precode_92 = 0;
		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "DEC (%02x,X)\n", short_address);
			put_data_memory_byte(short_address+register_x, (get_data_memory_byte(short_address+register_x) - 1));
			set_flags(prog_memory[short_address+register_x]);
			inc_sim_time(6);
		}
		// increment pc
		register_pc += 2;
		break;

	// NEG
	case NEG_A:
		sprintf((char *)print_buffer, "NEG A\n");
		register_a = 0 - register_a;
		set_flags(register_a);
		if(!register_a) {
			register_cc &= ~CARRY_BIT;
		}
		// increment pc
		register_pc++;
		inc_sim_time(3);
		break;

	case NEG_X:
		if(precode_90) {
			sprintf((char *)print_buffer, "NEG Y\n");
			register_y = 0 - register_y;
			set_flags(register_y);
			if(!register_y) {
				register_cc &= ~CARRY_BIT;
			}
			inc_sim_time(4);
			precode_90 = 0;
		} else {
			sprintf((char *)print_buffer, "NEG X\n");
			register_x = 0 - register_x;
			set_flags(register_x);
			if(!register_x) {
				register_cc &= ~CARRY_BIT;
			}
			inc_sim_time(3);
		}
		// increment pc
		register_pc++;
		break;

	case NEG_REG_IND:
		if(precode_90) {
			sprintf((char *)print_buffer, "NEG (Y)\n");
			bcp_temp = get_data_memory_byte(register_y);
			bcp_temp = 0 - bcp_temp;
			put_data_memory_byte(register_y, bcp_temp);
			set_flags(bcp_temp);
			if(!bcp_temp) {
				register_cc &= ~CARRY_BIT;
			}
			inc_sim_time(7);
			precode_90 = 0;
		} else {
			sprintf((char *)print_buffer, "NEG (X)\n");
			bcp_temp = get_data_memory_byte(register_x);
			bcp_temp = 0 - bcp_temp;
			put_data_memory_byte(register_x, bcp_temp);
			set_flags(bcp_temp);
			if(!bcp_temp) {
				register_cc &= ~CARRY_BIT;
			}
			inc_sim_time(5);
		}
		// increment pc
		register_pc++;
		break;

	case NEG_SHORT:
		if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "NEG [%02x]\n", indirect_address);
			inc_sim_time(7);
			precode_92 = 0;
		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "NEG %02x\n", short_address);
			inc_sim_time(5);
		}
		bcp_temp = get_data_memory_byte(short_address);
		bcp_temp = 0 - bcp_temp;
		put_data_memory_byte(short_address, bcp_temp);
		set_flags(bcp_temp);
		if(!bcp_temp) {
			register_cc &= ~CARRY_BIT;
		}
		// increment pc
		register_pc += 2;
		break;

	case NEG_REG_IND_OFF_SHORT:
		if(precode_90) {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "NEG (%02x,Y)\n", short_address);
			bcp_temp = get_data_memory_byte(short_address+register_y);
			bcp_temp = 0 - bcp_temp;
			put_data_memory_byte(short_address+register_y, bcp_temp);
			set_flags(bcp_temp);
			if(!bcp_temp) {
				register_cc &= ~CARRY_BIT;
			}
			inc_sim_time(7);
			precode_90 = 0;
		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "NEG ([%02x],Y)\n", indirect_address);
			bcp_temp = get_data_memory_byte(short_address+register_y);
			bcp_temp = 0 - bcp_temp;

			put_data_memory_byte(short_address+register_y, bcp_temp);
			set_flags(bcp_temp);
			if(!bcp_temp) {
				register_cc &= ~CARRY_BIT;
			}
			inc_sim_time(8);
			precode_91 = 0;
		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "NEG ([%02x],X)\n", indirect_address);
			bcp_temp = get_data_memory_byte(short_address+register_x);
			bcp_temp = 0 - bcp_temp;

			put_data_memory_byte(short_address+register_x, bcp_temp);
			set_flags(bcp_temp);
			if(!bcp_temp) {
				register_cc &= ~CARRY_BIT;
			}
			inc_sim_time(8);
			precode_92 = 0;
		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "NEG (%02x,X)\n", short_address);
			bcp_temp = get_data_memory_byte(short_address+register_x);
			bcp_temp = 0 - bcp_temp;
			put_data_memory_byte(short_address+register_x, bcp_temp);
			set_flags(bcp_temp);
			if(!bcp_temp) {
				register_cc &= ~CARRY_BIT;
			}
			inc_sim_time(6);
		}
		// increment pc
		register_pc += 2;
		break;

	// CPL
	case CPL_A:
		sprintf((char *)print_buffer, "CPL A\n");
		register_a = (0xff - register_a);
		set_flags(register_a);
		register_cc |= CARRY_BIT;
		// increment pc
		register_pc++;
		inc_sim_time(3);
		break;

	case CPL_X:
		if(precode_90) {
			sprintf((char *)print_buffer, "CPL Y\n");
			register_y = (0xff - register_y);
			set_flags(register_y);	
			inc_sim_time(4);
			precode_90 = 0;
		} else {
			sprintf((char *)print_buffer, "CPL X\n");
			register_x = (0xff - register_x);
			set_flags(register_x);
			inc_sim_time(3);
		}
		register_cc |= CARRY_BIT;
		// increment pc
		register_pc++;
		break;

	case CPL_REG_IND:
		if(precode_90) {
			sprintf((char *)print_buffer, "CPL (Y)\n");
			bcp_temp = get_data_memory_byte(register_y);
			bcp_temp = (0xff - bcp_temp);
			put_data_memory_byte(register_y, bcp_temp);
			set_flags(bcp_temp);
			inc_sim_time(7);
			precode_90 = 0;
		} else {
			sprintf((char *)print_buffer, "CPL (X)\n");
			bcp_temp = get_data_memory_byte(register_x);
			bcp_temp = (0xff - bcp_temp);
			put_data_memory_byte(register_x, bcp_temp);
			set_flags(bcp_temp);
			inc_sim_time(5);
		}
		register_cc |= CARRY_BIT;
		// increment pc
		register_pc++;
		break;

	case CPL_SHORT:
		if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "CPL [%02x]\n", indirect_address);
			inc_sim_time(7);
			precode_92 = 0;
		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "CPL %02x\n", short_address);
			inc_sim_time(5);
		}
		bcp_temp = get_data_memory_byte(short_address);
		bcp_temp = (0xff - bcp_temp);
		put_data_memory_byte(short_address, bcp_temp);
		set_flags(bcp_temp);
		register_cc |= CARRY_BIT;
		// increment pc
		register_pc += 2;
		break;

	case CPL_REG_IND_OFF_SHORT:
		if(precode_90) {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "CPL (%02x,Y)\n", short_address);
			bcp_temp = get_data_memory_byte(short_address+register_y);
			bcp_temp = (0xff - bcp_temp);
			put_data_memory_byte(short_address+register_y, bcp_temp);
			set_flags(bcp_temp);			
			inc_sim_time(7);
			precode_90 = 0;
		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "CPL ([%02x],Y)\n", indirect_address);
			bcp_temp = get_data_memory_byte(short_address+register_y);
			bcp_temp = (0xff - bcp_temp);
			put_data_memory_byte(short_address+register_y, bcp_temp);
			inc_sim_time(8);
			precode_91 = 0;
		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "CPL ([%02x],X)\n", indirect_address);
			bcp_temp = get_data_memory_byte(short_address+register_x);
			bcp_temp = (0xff - bcp_temp);
			put_data_memory_byte(short_address+register_x, bcp_temp);
			precode_92 = 0;
			inc_sim_time(8);
		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "CPL (%02x,X)\n", short_address);
			bcp_temp = get_data_memory_byte(short_address+register_x);
			bcp_temp = (0xff - bcp_temp);
			put_data_memory_byte(short_address+register_x, bcp_temp);
			inc_sim_time(6);
		}
		register_cc |= CARRY_BIT;
		// increment pc
		register_pc += 2;
		break;

	// TNZ
	case TNZ_A:	// 0x4d
		// standard st8 has precode 72 - tnz #xxxx
		// standard st8 has precode 90 - (tnz #xxxx,Y)
		sprintf((char *)print_buffer, "TNZ A\n");
		set_flags(register_a);
		// increment pc
		register_pc++;
		inc_sim_time(3);
		break;

	case TNZ_X:	// 0x5d
		if(precode_90) {
			sprintf((char *)print_buffer, "TNZ Y\n");
			set_flags(register_y);
			inc_sim_time(4);
			precode_90 = 0;
			// increment pc
			register_pc++;
		} else if(precode_72) {
			long_address = get_data_memory_byte(register_pc+1) << 8;
			long_address |= get_data_memory_byte(register_pc+2);
			sprintf((char *)print_buffer, "TNZ %04x\n", long_address);
			inc_sim_time(5);
			set_flags(get_data_memory_byte(long_address));
			precode_72 = 0;
			// increment pc
			register_pc += 3;
		} else {
			sprintf((char *)print_buffer, "TNZ X\n");
			set_flags(register_x);
			inc_sim_time(3);
			// increment pc
			register_pc++;
		}
		break;

	case TNZ_REG_IND:	// 0x7d
		if(precode_90) {
			sprintf((char *)print_buffer, "TNZ (Y)\n");
			set_flags(get_data_memory_byte(register_y));
			inc_sim_time(7);
			precode_90 = 0;
		} else {
			sprintf((char *)print_buffer, "TNZ (X)\n");
			set_flags(get_data_memory_byte(register_x));
			inc_sim_time(5);
		}
		// increment pc
		register_pc++;
		break;

	case TNZ_SHORT: // 0x3d
		// standard st8 has precode 0x72 TNZ [xxxx].w
		if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "TNZ [%02x]\n", indirect_address);
			inc_sim_time(7);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "TNZ %02x\n", short_address);
			inc_sim_time(5);

		}
		set_flags(get_data_memory_byte(short_address));
		// increment pc
		register_pc += 2;
		break;

	case TNZ_REG_IND_OFF_SHORT:	// 0x6d
		// standard st8 has precode 0x72 - TNZ([#xxxx.w],X)
		if(precode_90) {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "TNZ (%02x,Y)\n", short_address);
			temp = get_data_memory_byte(short_address+register_y);
			inc_sim_time(7);
			precode_90 = 0;

		} else if(precode_91) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "TNZ ([%02x],Y)\n", indirect_address);
			temp = get_data_memory_byte(short_address+register_y);
			inc_sim_time(8);
			precode_91 = 0;

		} else if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			sprintf((char *)print_buffer, "TNZ ([%02x],X)\n", indirect_address);
			temp = get_data_memory_byte(short_address+register_x);
			inc_sim_time(8);
			precode_92 = 0;

		} else {
			short_address = get_data_memory_byte(register_pc+1);
			sprintf((char *)print_buffer, "TNZ (%02x,X)\n", short_address);
			temp = get_data_memory_byte(short_address+register_x);
			inc_sim_time(6);

		}
		set_flags(temp);
		// increment pc
		register_pc += 2;
		break;

	// PUSH
	case PUSH_A:
		sprintf((char *)print_buffer, "PUSH A\n");
		prog_memory[register_sp--] = register_a;	// push a onto the stack
		// increment pc
		register_pc++;
		inc_sim_time(3);
		break;

	case PUSH_X:
		if(precode_90) {
			sprintf((char *)print_buffer, "PUSH Y\n");
			prog_memory[register_sp--] = register_y;	// push y onto the stack
			inc_sim_time(4);
			precode_90 = 0;

		} else {
			sprintf((char *)print_buffer, "PUSH X\n");
			prog_memory[register_sp--] = register_x;	// push x onto the stack
			inc_sim_time(3);
		}
		// increment pc
		register_pc++;
		break;

	case PUSH_CC:
		sprintf((char *)print_buffer, "PUSH CC\n");
		prog_memory[register_sp--] = register_cc;	// push cc onto the stack
		// increment pc
		register_pc++;
		inc_sim_time(3);
		break;

	case PUSH_LONG:	// 3b
		long_address = get_data_memory_byte(register_pc+1) << 8;
		long_address |= get_data_memory_byte(register_pc+2);
		sprintf((char *)print_buffer, "PUSH %08x\n", long_address);
		temp = get_data_memory_byte(long_address);
		prog_memory[register_sp--] = temp;	// push onto the stack
		// increment pc
		register_pc += 3;
		inc_sim_time(3);
		break;

	case PUSH_IMMED:	//				0x4b
		temp = get_data_memory_byte(register_pc+1);
		sprintf((char *)print_buffer, "PUSH #%02x\n", temp);
		prog_memory[register_sp--] = temp;	// push onto the stack
		// increment pc
		register_pc += 2;
		inc_sim_time(3);
		break;

	// POP
	case POP_A:
		sprintf((char *)print_buffer, "POP A\n");
		register_a = get_data_memory_byte(++register_sp);	// push from the stack into a
//		set_flags(register_a);
		// increment pc
		register_pc++;
		inc_sim_time(3);
		break;

	case POP_X:
		if(precode_90) {
			sprintf((char *)print_buffer, "POP Y\n");
			register_y = get_data_memory_byte(++register_sp);	// push from the stack into y
//			set_flags(register_y);
			inc_sim_time(4);
			precode_90 = 0;

		} else {
			sprintf((char *)print_buffer, "POP X\n");
			register_x = get_data_memory_byte(++register_sp);	// push from the stack into x
//			set_flags(register_x);
			inc_sim_time(3);
		}
		// increment pc
		register_pc++;
		break;

	case POP_CC:
		sprintf((char *)print_buffer, "POP CC\n");
		register_cc = get_data_memory_byte(++register_sp);	// push from the stack into cc
		// increment pc
		register_pc++;
		inc_sim_time(3);
		break;

	case POP_LONG:	//				0x32
		long_address = get_data_memory_byte(register_pc+1) << 8;
		long_address |= get_data_memory_byte(register_pc+2);
		sprintf((char *)print_buffer, "POP %08x\n", long_address);
		temp = get_data_memory_byte(++register_sp);	// push from the stack
		put_data_memory_byte(long_address, temp);
		// increment pc
		register_pc += 3;
		inc_sim_time(3);
		break;

// CALL
	case CALL_LONG:
		if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			dest = get_data_memory_byte(indirect_address) << 8;
			dest |= get_data_memory_byte(indirect_address+1);
			
			register_pc += 2;	// adjust so return address is correct
	
			prog_memory[register_sp--] = (unsigned char)(register_pc & 0xff);	// push return address on stack
			prog_memory[register_sp--] = (unsigned char)((register_pc >> 8) & 0xff);
			register_pc &= 0xffff0000;
			register_pc |= dest;			

			sprintf((char *)print_buffer, "CALL [%02x.w]=%04x EA=%04x : pc=%08x\n", indirect_address, dest, dest, register_pc);
			
			inc_sim_time(8);
			precode_92 = 0;
		} else {
			dest = get_data_memory_byte(register_pc+1) << 8;
			dest |= get_data_memory_byte(register_pc+2);
			
			register_pc += 3;	// adjust so return address is correct

			prog_memory[register_sp--] = (unsigned char)(register_pc & 0xff);// push return address on stack
			prog_memory[register_sp--] = (unsigned char)((register_pc >> 8) & 0xff);
			register_pc &= 0xffff0000;
			register_pc |= dest;

			sprintf((char *)print_buffer, "CALL %04x : pc=%08x\n", dest, register_pc);

			inc_sim_time(6);
		}
		executed_call_instruction = 1;
		break;

	case CALL_REG_IND:
		if(precode_90) {
			dest = register_y;

			register_pc++;	// adjust so return address is correct

			prog_memory[register_sp--] = (unsigned char)(register_pc & 0xff);
			prog_memory[register_sp--] = (unsigned char)((register_pc >> 8) & 0xff);
			register_pc = dest;

			sprintf((char *)print_buffer, "CALL (Y) : pc=%08x\n", dest);

			inc_sim_time(6);
			precode_90 = 0;
		} else {
			dest = register_x;

			register_pc++;	// adjust so return address is correct

			prog_memory[register_sp--] = (unsigned char)(register_pc & 0xff);
			prog_memory[register_sp--] = (unsigned char)((register_pc >> 8) & 0xff);
			register_pc = dest;

			sprintf((char *)print_buffer, "CALL (X) : pc=%08x\n", dest);

			inc_sim_time(5);
		}
		executed_call_instruction = 1;
		break;

	case CALL_REG_IND_OFF_SHORT:
		register_pc += 2;
		if(precode_90) {
			// (short,Y)
			short_address = get_data_memory_byte(register_pc+1);
			
			temp = register_y;

			prog_memory[register_sp--] = (unsigned char)(register_pc & 0xff); // save return address on stack
			prog_memory[register_sp--] = (unsigned char)((register_pc >> 8) & 0xff);

			dest = short_address;
			dest += temp;
			register_pc = dest;

			sprintf((char *)print_buffer, "CALL (%02x,Y) : pc=%08x\n", short_address, register_pc);

			inc_sim_time(7);
			precode_90 = 0;

		} else if(precode_92) {
			// ([short],X)
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			
			temp = register_x;

			prog_memory[register_sp--] = (unsigned char)(register_pc & 0xff); // save return address on stack
			prog_memory[register_sp--] = (unsigned char)((register_pc >> 8) & 0xff);

			dest = short_address;
			dest += temp;
			register_pc = dest;

			sprintf((char *)print_buffer, "CALL ([%02x],X) : pc=%08x\n", indirect_address, register_pc);

			inc_sim_time(8);
			precode_92 = 0;

		} else if(precode_91) {
			// ([short],Y)
			indirect_address = get_data_memory_byte(register_pc+1);
			short_address = get_data_memory_byte(indirect_address);
			
			temp = register_y;

			prog_memory[register_sp--] = (unsigned char)(register_pc & 0xff); // save return address on stack
			prog_memory[register_sp--] = (unsigned char)((register_pc >> 8) & 0xff);

			dest = short_address;
			dest += temp;
			register_pc = dest;

			sprintf((char *)print_buffer, "CALL ([%02x],Y) : pc=%08x\n", indirect_address, register_pc);

			inc_sim_time(8);
			precode_91 = 0;

		} else {
			// (short,X)
			short_address = get_data_memory_byte(register_pc+1);
			
			temp = register_x;

			prog_memory[register_sp--] = (unsigned char)(register_pc & 0xff); // save return address on stack
			prog_memory[register_sp--] = (unsigned char)((register_pc >> 8) & 0xff);

			dest = short_address;
			dest += temp;
			register_pc = dest;

			sprintf((char *)print_buffer, "CALL (%02x,X) : pc=%08x\n", short_address, register_pc);

			inc_sim_time(6);
		}
		executed_call_instruction = 1;
		break;

	case CALL_REG_IND_OFF_LONG:
		if(precode_90) {
			// (long,Y)
			long_address = get_data_memory_byte(register_pc+1) << 8;
			long_address |= get_data_memory_byte(register_pc+2);
			
			temp = register_y;

			register_pc += 3;	// adjust so return address is correct

			prog_memory[register_sp--] = (unsigned char)(register_pc & 0xff);	// save returnj address on stack
			prog_memory[register_sp--] = (unsigned char)((register_pc >> 8) & 0xff);

			dest = long_address;
			dest += temp;
			register_pc = dest;

			sprintf((char *)print_buffer, "CALL (%04x,Y) : pc=%08x\n", long_address, register_pc);

			inc_sim_time(8);
			precode_90 = 0;

		} else if(precode_92) {
			// ([long],X)
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = get_data_memory_byte(indirect_address) << 8;
			long_address |= get_data_memory_byte(indirect_address);
			
			temp = register_x;

			register_pc += 2;// adjust so return address is correct

			prog_memory[register_sp--] = (unsigned char)(register_pc & 0xff);	// save returnj address on stack
			prog_memory[register_sp--] = (unsigned char)((register_pc >> 8) & 0xff);

			dest = long_address;
			dest += temp;
			register_pc = dest;

			sprintf((char *)print_buffer, "CALL ([%02x.w],X) : pc=%08x\n", indirect_address, register_pc);

			inc_sim_time(9);
			precode_92 = 0;

		} else if(precode_91) {
			// ([long],Y)
			indirect_address = get_data_memory_byte(register_pc+1);
			long_address = get_data_memory_byte(indirect_address) << 8;
			long_address = get_data_memory_byte(indirect_address) << 8;
			
			temp = register_y;

			register_pc += 2;	// adjust so return address is correct

			prog_memory[register_sp--] = (unsigned char)(register_pc & 0xff);	// save returnj address on stack
			prog_memory[register_sp--] = (unsigned char)((register_pc >> 8) & 0xff);

			dest = long_address;
			dest += temp;
			register_pc = dest;

			sprintf((char *)print_buffer, "CALL ([%02x.w],Y) : pc=%08x\n", indirect_address, register_pc);

			inc_sim_time(9);
			precode_91 = 0;

		} else {
			// (long,X)
			long_address = get_data_memory_byte(register_pc+1) << 8;
			long_address |= get_data_memory_byte(register_pc+2);
			
			temp = register_x;

			register_pc += 3;	// adjust so return address is correct

			prog_memory[register_sp--] = (unsigned char)(register_pc & 0xff);	// save returnj address on stack
			prog_memory[register_sp--] = (unsigned char)((register_pc >> 8) & 0xff);

			dest = long_address;
			dest += temp;
			register_pc = dest;

			sprintf((char *)print_buffer, "CALL (%04x,X) : pc=%08x\n", long_address, register_pc);

			inc_sim_time(7);
		}
		executed_call_instruction = 1;
		break;

	case CALLR_SHORT:
		if(precode_92) {
			indirect_address = get_data_memory_byte(register_pc+1);
			displacement = get_data_memory_byte(indirect_address);

			register_pc += 2;	// adjust so return address is correct

			prog_memory[register_sp--] = (unsigned char)(register_pc & 0xff);	// save returnj address on stack
			prog_memory[register_sp--] = (unsigned char)((register_pc >> 8) & 0xff);

			if(displacement & 0x0080) {
				displacement |= 0xff00;
			}
			register_pc += displacement;
			
			sprintf((char *)print_buffer, "CALLR [%02x]=%d : pc=%08x\n", indirect_address, displacement, register_pc);

			inc_sim_time(8);
			precode_92 = 0;

		} else {
			displacement = get_data_memory_byte(register_pc+1);

			register_pc += 2;	// adjust so return address is correct

			prog_memory[register_sp--] = (unsigned char)(register_pc & 0xff);	// save returnj address on stack
			prog_memory[register_sp--] = (unsigned char)((register_pc >> 8) & 0xff);

			if(displacement & 0x0080) {
					displacement |= 0xff00;
			}
			register_pc += displacement;	
	
			sprintf((char *)print_buffer, "CALLR %d : pc=%08x\n", displacement, register_pc);

			inc_sim_time(6);
		}
		executed_call_instruction = 1;
		break;

	case CALL_FAR:
		dword_address = get_data_memory_byte(register_pc+1) << 16;
		dword_address |= get_data_memory_byte(register_pc+2) << 8;
		dword_address |= get_data_memory_byte(register_pc+3);

		register_pc += 4;	// adjust so return address is correct

		prog_memory[register_sp--] = (unsigned char)(register_pc & 0xff);	// save returnj address on stack (long format)
		prog_memory[register_sp--] = (unsigned char)((register_pc >> 8) & 0xff);
		prog_memory[register_sp--] = (unsigned char)((register_pc >> 16) & 0xff);

		if(dword_address & 0xffff0000) {
//			sprintf((char *)print_buffer, "*INTER-SEGMENT CALL to %08x", dword_address);
		}
		register_pc = dword_address;

		sprintf((char *)print_buffer, "CALLF %08x : pc=%08x\n", dword_address, register_pc);

		inc_sim_time(10);
		executed_call_instruction = 1;
		break;

	case RET:
		// protection for stack underflow
/*		if(register_sp >= 0x1000) {
			sprintf((char *)print_buffer, "*** STACK UNDERFLOW @ %08x\n", register_pc);
			running = 0;
		} else {
*/
			dest = get_data_memory_byte(++register_sp) << 8;	// get return address from stack
			dest |= get_data_memory_byte(++register_sp);

			sprintf((char *)print_buffer, "RET (%04x)\n", dest);

		if(((dest & 0x0000ffff) >= XIO_START) && ((dest & 0x0000ffff) <= XIO_END)) {
			sprintf((char *)print_buffer, "\n*** FETCHING FROM IO REGION: pc=%08x, address=%08x previous_pc=%08x\n", register_pc, dest, previous_register_pc);
			simulator_output();
			running = 0;
			aabnormal_termination = 1;
			return(0);
		}

		if(((dest & 0x0000ffff) >= IO_START) && ((dest & 0x0000ffff) <= IO_END)) {
			sprintf((char *)print_buffer, "\n*** FETCHING FROM IO REGION: pc=%08x, address=%08x previous_pc=%08x\n", register_pc, dest, previous_register_pc);
			simulator_output();
			aabnormal_termination = 1;
			running = 0;
			return(0);
		}

		if(((dest & 0x0000ffff) >= RAM_START) && ((dest & 0x0000ffff) <= RAM_END)) {
			sprintf((char *)print_buffer, "\n*** FETCHING FROM RAM REGION: pc=%08x, address=%08x previous_pc=%08x\n", register_pc, dest, previous_register_pc);
			simulator_output();
			aabnormal_termination = 1;
			running = 0;
			return(0);
		}
		register_pc &= 0xffff0000;
		register_pc |= dest;
//		}
		executed_return_instruction = 1;
		inc_sim_time(6);
		break;

	case RETF:
		// protection for stack underflow
/*		if(register_sp >= 0x1000) {
			sprintf((char *)print_buffer, "*** STACK UNDERFLOW @ %08x\n", register_pc);
			running = 0;
		} else {
*/
			dword_address = get_data_memory_byte(++register_sp) << 16;	// save returnj address on stack (long format)
			dword_address |= get_data_memory_byte(++register_sp) << 8;
			dword_address |= get_data_memory_byte(++register_sp);

			sprintf((char *)print_buffer, "RETF (%08x)\n", dword_address);

		if(((dword_address & 0x0000ffff) >= XIO_START) && ((dword_address & 0x0000ffff) <= XIO_END)) {
			sprintf((char *)print_buffer, "\n*** FETCHING FROM IO REGION: pc=%08x, address=%08x previous_pc=%08x\n", register_pc, dword_address, previous_register_pc);
			simulator_output();
			running = 0;
			aabnormal_termination = 1;
			return(0);
		}

		if(((dword_address & 0x0000ffff) >= IO_START) && ((dword_address & 0x0000ffff) <= IO_END)) {
			sprintf((char *)print_buffer, "\n*** FETCHING FROM IO REGION: pc=%08x, address=%08x previous_pc=%08x\n", register_pc, dword_address, previous_register_pc);
			simulator_output();
			aabnormal_termination = 1;
			running = 0;
			return(0);
		}

		if(((dword_address & 0x0000ffff) >= RAM_START) && ((dword_address & 0x0000ffff) <= RAM_END)) {
			sprintf((char *)print_buffer, "\n*** FETCHING FROM RAM REGION: pc=%08x, address=%08x previous_pc=%08x\n", register_pc, dword_address, previous_register_pc);
			simulator_output();
			aabnormal_termination = 1;
			running = 0;
			return(0);
		}

		register_pc = dword_address;
//		}
		inc_sim_time(8);
		executed_return_instruction = 1;
		break;

	case TRAP:	// this is st7 style
		register_pc++;

		prog_memory[register_sp--] = (unsigned char)(register_pc & 0xff);	// save registers to stack
		prog_memory[register_sp--] = (unsigned char)((register_pc >> 8) & 0xff);
		prog_memory[register_sp--] = register_x;
		prog_memory[register_sp--] = register_a;
		prog_memory[register_sp--] = register_cc;

		register_cc |= (INTERRUPT_MASK_L0_BIT|INTERRUPT_MASK_L1_BIT);

		dest = get_data_memory_byte(SWI) << 8;
		dest |= get_data_memory_byte(SWI+1);
		register_pc = dest;
		sprintf((char *)print_buffer, "TRAP %04x\n", dest);
		
		inc_sim_time(10);
		break;

	case IRET:
		register_cc = get_data_memory_byte(++register_sp);	// restore registers from stack
		register_a = get_data_memory_byte(++register_sp);
		register_x = get_data_memory_byte(++register_sp);

		dest = get_data_memory_byte(++register_sp) << 8;
		dest |= get_data_memory_byte(++register_sp);

		sprintf((char *)print_buffer, "IRET (%04x)\n", dest);
		

		register_pc = dest;
		inc_sim_time(9);
		break;

	case WFI:	// we do what we can here
		sprintf((char *)print_buffer, "WFI\n");
		
		register_cc &= ~(INTERRUPT_MASK_L0_BIT|INTERRUPT_MASK_L1_BIT);
		inc_sim_time(2);
		running = 0;
//		while(1)
//			;
		break;

	// Unknown instruction
	default:
		sprintf((char *)print_buffer, "*** Unknown instruction (%02x) @ pc=%08x\n", instruction, register_pc);
		simulator_output();
		aabnormal_termination = 1;
		running = 0;
		break;
	}

	// rely on the individual handlers to clear so we can detect unhandled precodes
	if(precode_72) {
		sprintf((char *)print_buffer, "*** Unhandled precode_72 @ pc=%08x\n", register_pc);
		simulator_output();
		aabnormal_termination = 1;
		running = 0;
	}
	if(precode_90) {
		sprintf((char *)print_buffer, "*** Unhandled precode_90 @ pc=%08x\n", register_pc);
		simulator_output();
		aabnormal_termination = 1;
		running = 0;
	}
	if(precode_91) {
		sprintf((char *)print_buffer, "*** Unhandled precode_91 @ pc=%08x\n", register_pc);
		simulator_output();
		aabnormal_termination = 1;
		running = 0;
	}
	if(precode_92) {
		sprintf((char *)print_buffer, "*** Unhandled precode_92 @ pc=%08x\n", register_pc);
		simulator_output();
		aabnormal_termination = 1;
		running = 0;
	}

	// produce output
	simulator_output();

	// tell caller to stop
	return(0);
}
