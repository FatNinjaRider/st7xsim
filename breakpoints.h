//
//---------------------------------------------------------------------------
//
// ST7x Simulator - breakpoint stuff
//
// Author: Rick Stievenart
//
// Genesis: 03/09/2017
//
// History:
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

//#include "utils.h"
//#include "debug.h"

#include "processor_externs.h"

#include "application.h"

//
//---------------------------------
// Simulator configuration
//---------------------------------
//

#define NUM_INS_BREAKPOINTS 8
#define NUM_DATA_BREAKPOINTS 8

//
//--------------------------------------------------------
// simulator internals - breakpoint mechanism definitions
//--------------------------------------------------------
//

// Stop types
#define STOP_ABNORMAL_TERMINATION	1
#define STOP_USER_BREAK				2
#define STOP_INS_BREAK				3
#define STOP_DATA_BREAK				4
#define STOP_CALL_INS_BREAK			5
#define STOP_APPLICATION_BREAK		6

// Breakpoint type flags
#define DBRK_TYPE_READ		0x01
#define DBRK_TYPE_WRITE		0x02
#define DBRK_TYPE_RW		(DBRK_TYPE_READ|DBRK_TYPE_WRITE)

#define DBRK_TYPE_VAL		0x10
#define DBRK_TYPE_SUB		0x20

#define IBRK_TYPE			0x04
#define BRK_TYPE_COUNT		0x08

// breakpoint struct
struct breakpoint {
	int used;
	int enable;
	int type;
	int terminal_count;
	int count;
	int triggered;
	unsigned int value;
	unsigned int address;
};
