//
//---------------------------------------------------------------------------
//
// ST7x Simulator - cpu defines
//
// Author: Rick Stievenart
//
// Genesis: 03/04/2011
//
//----------------------------------------------------------------------------
//

//
//---------------------------------
// Processor and environment
//---------------------------------
//

#define CLOCK_FREQUENCY					4000000		// 4mhz
#define CLOCKS_PER_INSTRUCTION_CYCLE	1			// one clock per cycle
	
#define SWI								0x2ffc		// Interrupt Vector			

//#define SP_INITIAL_VALUE				0x07ff		// Stack pointer innitial value			
#define SP_INITIAL_VALUE				0x03ff		// Stack pointer innitial value			

#define PC_INITIAL_VALUE				0x00004000	// pc initial value

// Memory regions
#define MEMSIZE							65535

#define IO_START						0x00000000
#define IO_END							0x0000001f

#define RAM_START						0x00000020
#define RAM_END							0x00000fff

#define XIO_START						0x00003c00	// extended IO
#define XIO_END							0x00003dff

#define FLASH_START						0x0000C000
#define FLASH_END						0x0000c7ff

#define ROM_START						0x00004000
#define ROM1_START						0x00008000 // page 10, abs=0x00108000


//-----------------------------------
// INSTRUCTIONS
//-----------------------------------
//

// EXGW
#define EXGW					0x51

// EXG A,x
#define EXG_A_X					0x41
#define EXG_A_Y					0x61
#define EXG_A_LONG				0x31

// LDF	A,x
#define LDF_A_FAR				0xbc
#define LDF_A_REG_IND			0xaf

// LDF x,A
#define LDF_FAR_A				0xbd
#define LDF_REG_IND_A			0xa7

// ADC A,x
#define ADC_IMMED				0xa9
#define ADC_SHORT				0xb9
#define ADC_LONG				0xc9
#define ADC_REG_IND				0xf9
#define ADC_REG_IND_OFF_SHORT	0xe9
#define ADC_REG_IND_OFF_LONG	0xd9

// ADD A,x
#define ADD_IMMED				0xab
#define ADD_SHORT				0xbb
#define ADD_LONG				0xcb
#define ADD_REG_IND				0xfb
#define ADD_REG_IND_OFF_SHORT	0xeb
#define ADD_REG_IND_OFF_LONG	0xdb

// AND A,x
#define AND_IMMED				0xa4
#define AND_SHORT				0xb4
#define AND_LONG				0xc4
#define AND_REG_IND				0xf4
#define AND_REG_IND_OFF_SHORT	0xe4
#define AND_REG_IND_OFF_LONG	0xd4

// BCP A,x
#define BCP_IMMED				0xa5
#define BCP_SHORT				0xb5
#define BCP_LONG				0xc5
#define BCP_REG_IND				0xf5
#define BCP_REG_IND_OFF_SHORT	0xe5
#define BCP_REG_IND_OFF_LONG	0xd5

// CP A,x
#define CP_IMMED				0xa1
#define CP_SHORT				0xb1
#define CP_LONG					0xc1
#define CP_REG_IND				0xf1
#define CP_REG_IND_OFF_SHORT	0xe1
#define CP_REG_IND_OFF_LONG		0xd1

// CP X,x
#define CP_X_IMMED				0xa3
#define CP_X_SHORT				0xb3
#define CP_X_LONG				0xc3
#define CP_X_REG_IND			0xf3
#define CP_X_REG_IND_OFF_SHORT	0xe3
#define CP_X_REG_IND_OFF_LONG	0xd3

// OR A,x
#define OR_IMMED				0xaa
#define OR_SHORT				0xba
#define OR_LONG					0xca
#define OR_REG_IND				0xfa
#define OR_REG_IND_OFF_SHORT	0xea
#define OR_REG_IND_OFF_LONG		0xda
	
// SBC A,x
#define SBC_IMMED				0xa2
#define SBC_SHORT				0xb2
#define SBC_LONG				0xc2
#define SBC_REG_IND				0xf2
#define SBC_REG_IND_OFF_SHORT	0xe2
#define SBC_REG_IND_OFF_LONG	0xd2

// SUB A,x
#define SUB_IMMED				0xa0
#define SUB_SHORT				0xb0
#define SUB_LONG				0xc0
#define SUB_REG_IND				0xf0
#define SUB_REG_IND_OFF_SHORT	0xe0
#define SUB_REG_IND_OFF_LONG	0xd0

// XOR A,x
#define XOR_IMMED				0xa8
#define XOR_SHORT				0xb8
#define XOR_LONG				0xc8
#define XOR_REG_IND				0xf8
#define XOR_REG_IND_OFF_SHORT	0xe8
#define XOR_REG_IND_OFF_LONG	0xd8

// LD reg,reg
#define LD_X_A					0x97
#define LD_A_X					0x9f
#define LD_X_Y					0x93
#define LD_A_S					0x9e
#define LD_S_A					0x95
#define LD_X_S					0x96
#define LD_S_X					0x94

// LD A,x
#define LD_A_IMMED				0xa6
#define LD_A_SHORT				0xb6
#define LD_A_LONG				0xc6
#define LD_A_REG_IND			0xf6
#define LD_A_REG_IND_OFF_SHORT	0xe6
#define LD_A_REG_IND_OFF_LONG	0xd6
#define LD_A_SP_IND				0x7b

// LD x,A
#define LD_SHORT_A				0xb7
#define LD_LONG_A				0xc7
#define LD_REG_IND_A			0xf7
#define LD_REG_IND_OFF_SHORT_A	0xe7
#define LD_REG_IND_OFF_LONG_A	0xd7
#define LD_SP_IND_A				0x6b

// LD X,x
#define LD_X_IMMED				0xae
#define LD_X_SHORT				0xbe
#define LD_X_LONG				0xce
#define LD_X_REG_IND			0xfe
#define LD_X_REG_IND_OFF_SHORT	0xee
#define LD_X_REG_IND_OFF_LONG	0xde

// LD x,X
#define LD_SHORT_X				0xbf
#define LD_LONG_X				0xcf
#define LD_REG_IND_X			0xff
#define LD_REG_IND_OFF_SHORT_X	0xef
#define LD_REG_IND_OFF_LONG_X	0xdf

#define LDW_SP_X				0x8b	

// INC
#define INC_A					0x4c
#define INC_X					0x5c
#define INC_REG_IND				0x7c
#define INC_SHORT				0x3c
#define INC_REG_IND_OFF_SHORT	0x6c

// DEC	
#define DEC_A					0x4a
#define DEC_X					0x5a
#define DEC_REG_IND				0x7a
#define DEC_SHORT				0x3a
#define DEC_REG_IND_OFF_SHORT	0x6a

// NEG
#define NEG_A					0x40
#define NEG_X					0x50
#define NEG_REG_IND				0x70
#define NEG_SHORT				0x30
#define NEG_REG_IND_OFF_SHORT	0x60

// CLR
#define CLR_A					0x4F
#define CLR_X					0x5F
#define CLR_REG_IND				0x7f
#define CLR_SHORT				0x3f
#define CLR_REG_IND_OFF_SHORT	0x6f

// CPL
#define CPL_A					0x43
#define CPL_X					0x53
#define CPL_REG_IND				0x73
#define CPL_SHORT				0x33
#define CPL_REG_IND_OFF_SHORT	0x63

// SLA
#define SLA_A					0x48
#define SLA_X					0x58
#define SLA_REG_IND				0x78
#define SLA_SHORT				0x38
#define SLA_REG_IND_OFF_SHORT	0x68

#define SRA_A					0x47
#define SRA_X					0x57
#define SRA_REG_IND				0x77
#define SRA_SHORT				0x37
#define SRA_REG_IND_OFF_SHORT	0x67

#define SRL_A					0x44
#define SRL_X					0x54
#define SRL_REG_IND				0x74
#define SRL_SHORT				0x34
#define SRL_REG_IND_OFF_SHORT	0x64

#define RLC_A					0x49
#define RLC_X					0x59
#define RLC_REG_IND				0x79
#define RLC_SHORT				0x39
#define RLC_REG_IND_OFF_SHORT	0x69

#define RRC_A					0x46
#define RRC_X					0x56
#define RRC_REG_IND				0x76
#define RRC_SHORT				0x36
#define RRC_REG_IND_OFF_SHORT	0x66

#define SWAP_A					0x4e
#define SWAP_X					0x5e
#define SWAP_REG_IND			0x7e
#define SWAP_SHORT				0x3e
#define SWAP_REG_IND_OFF_SHORT	0x6e

// TNZ
#define TNZ_A					0x4d
#define TNZ_X					0x5d
#define TNZ_REG_IND				0x7d
#define TNZ_SHORT				0x3d
#define TNZ_REG_IND_OFF_SHORT	0x6d

// NOP
#define NOP						0x9d

// JP
#define JP_FAR					0xac
#define JP_LONG					0xcc
#define JP_REG_IND				0xfc
#define JP_REG_IND_OFF_SHORT	0xec
#define JP_REG_IND_OFF_LONG		0xdc

// CALL	
#define CALL_LONG				0xcd
#define CALL_REG_IND			0xfd
#define CALL_REG_IND_OFF_SHORT	0xed
#define CALL_REG_IND_OFF_LONG	0xdd

// CALLR
#define CALLR_SHORT				0xad

// CALLF
#define CALL_FAR				0x8d

#define HALT					0x8e

#define IRET					0x80
#define TRAP					0x83

#define POP_A					0x84
#define POP_X					0x85
#define POP_CC					0x86
#define POP_LONG				0x32

#define PUSH_IMMED				0x4b
#define PUSH_A					0x88
#define PUSH_X					0x89
#define PUSH_CC					0x8A
#define PUSH_LONG				0x3b

#define MUL						0x42
#define MUL1					0x52

#define DIV						0x62

#define WFI						0x8f

#define RCF						0x98
#define SCF						0x99
#define CCF						0x8c

#define RIM						0x9A
#define SIM						0x9b

#define RSP						0x9c

#define ADD_SP					0x5b

#define RET						0x81
#define RETF					0x87	// st7x

// JR'S
#define JRA						0x20
#define JRC						0x25	// carry (c=1)
#define JREQ					0x27	// equal (z=1)
#define JRF						0x21	// false 
#define	JRH						0x29	// half carry (h=1)
#define JRIH					0x2F	// Interrupt line hi
#define JRIL					0x2e	// Interrupt line low
#define JRM						0x2d	// Interrupt mask I=1
#define JRMI					0x2b	// Minus (n=1)
#define JRNC					0x24	// not carry (c=0)
#define JRNE					0x26	// != (z=0)
#define JRNH					0x28	// not-half-carry (h=0)
#define JRNM					0x2c	// not interrupt mask (i=0)
#define JRPL					0x2a	// plus (n=0)

// SAME AS JRA - #define	JRT				0x20	// true
// SAME AS JRNC - #define JRUGE			0x24	// Unsigned greater or equal (c=0)
#define JRUGT					0x22	// Unsigned greater than (C or Z) = 0
#define JRULE					0x23	// Unsigned lower or equal (C or Z) = 1
// SAME AS JRC - #define JRULT			0x25	// Unsigned lower than (c=1)

// BSET
#define BSET_0		0x10
#define BSET_1		0x12
#define BSET_2		0x14
#define BSET_3		0x16
#define BSET_4		0x18
#define BSET_5		0x1a
#define BSET_6		0x1c
#define BSET_7		0x1e

// BRES
#define BRES_0		0x11
#define BRES_1		0x13
#define BRES_2		0x15
#define BRES_3		0x17
#define BRES_4		0x19
#define BRES_5		0x1B
#define BRES_6		0x1D
#define BRES_7		0x1F

// BTJF
#define BTJF_0		0x01
#define BTJF_1		0x03
#define BTJF_2		0x05
#define BTJF_3		0x07
#define BTJF_4		0x09
#define BTJF_5		0x0b
#define BTJF_6		0x0d
#define BTJF_7		0x0f

// BTJT
#define BTJT_0		0x00
#define BTJT_1		0x02
#define BTJT_2		0x04
#define BTJT_3		0x06
#define BTJT_4		0x08
#define BTJT_5		0x0a
#define BTJT_6		0x0c
#define BTJT_7		0x0e

// Others
#define MOV_LONG_IMMED	0x35
#define MOV_SHORT_SHORT	0x45
#define MOV_LONG_LONG	0x55

// unimplemented
#define OPCODE_0x65 0x65
#define OPCODE_0x75	0x75

// Precodes
#define PRECODE_72	0x72 // Prefix unique to superset processor
#define PRECODE_90	0x90 // PDY
#define PRECODE_91	0x91 // PIY
#define PRECODE_92	0x92 // PIX

// Condition Code Register Bits
#define OVERFLOW_BIT			0x80
#define INTERRUPT_MASK_L1_BIT	0x20
#define HALF_CARRY_BIT			0x10
#define INTERRUPT_MASK_L0_BIT	0x08
#define NEGATIVE_BIT			0x04
#define ZERO_BIT				0x02
#define CARRY_BIT				0x01
