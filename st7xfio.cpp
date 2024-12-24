//
//---------------------------------------------------------------------------
//
// ST7x Simulator - file i/o
//
// Author: Rick Stievenart
//
// Genesis: 03/04/2011
//
//----------------------------------------------------------------------------
//

#include "stdafx.h"
#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <conio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>

#include "st7xcpu.h"
#include "st7xfio.h"

#include "processor_externs.h"

#include "breakpoints.h"
#include "simulator.h"

//
//--------------------------------------------------------
// simulator internals - file i/o - savers and loaders
//--------------------------------------------------------
//

// get a line from a file
int fgetline(FILE *fp, char *buff)
{
	int c;

	while((c = fgetc(fp)) != EOF) {
		*buff++ = c;
		if(c == '\n') {
			*buff = '\0';
			break;
		}
	}
	if(c == EOF) {
		return(0);
	}
	return(1);
}


// load a Motorola S-record file
void load_srec(void)
{
	char filename[128], line[128], bcstring[3], addressstring[5], bytestring[3];
	FILE *fp;
	unsigned int address;
	unsigned char bytecount, byte;
	int x;

	printf("Load Motorola S-Record File...\n");

	printf("Filename? ");
	scanf("%s", &filename[0]);
	getchar();

	if((fp = fopen(filename, "r")) == (FILE *)NULL) {
		printf("Can't open %s!\n", filename);
		return;
	}

	// process lines
	while(fgetline(fp, &line[0])) {
		if(!strncmp("S0", &line[0], 2)) {
			// S0 Header
			printf("Header found\n");
		}
		if(!strncmp("S1", &line[0], 2)) {
			// S1 Data: S11310009CA602B780A600B7814F5F90AE06270E5C
			strncpy(&bcstring[0], &line[2], 2);
			strncpy(&addressstring[0], &line[4], 4);
			sscanf(&bcstring[0], "%02x", &bytecount);
			sscanf(&addressstring[0], "%04x", &address);
			printf("Data (S1): %d bytes @ %04x\n", bytecount, address);

			x = 8;
			while(bytecount--) {
				strncpy(&bytestring[0], &line[x], 2);
				sscanf(&bytestring[0], "%02x", &byte);
				prog_memory[address++] = byte;
				x += 2;
			}

		}
		if(!strncmp("S2", &line[0], 2)) {
			// S2 Data
			printf("Data (S2):\n");
		}
		if(!strncmp("S3", &line[0], 2)) {
			// S3 Data
			printf("Data (S3):\n");
		}
		if(!strncmp("S9", &line[0], 2)) {
			// S9 Trailer
			printf("Trailer found\n");
		}
	}
	fclose(fp);
}

// load a binary file
void load_bin(char *filenamebase)
{
	char filename[128];
	FILE *fp;
	int c, bytecount, segment;
	unsigned short address;

	// Construct filename
	strcpy(filename, filenamebase);
	strcat(filename, ".bin");

	printf("Loading Binary File: %s...\n", filename);

	if((fp = fopen(filename, "rb")) == (FILE *)NULL) {
		printf("Can't open %s!\n", filename);
		return;
	}

	printf("Segment (0/1=10)? ");
	scanf("%d", &segment);
	getchar();

	bytecount = 0;
	if(segment == 0) {
		address = 0x4000;
	} else if(segment == 1) {
		address = 0x8000;
	} else {
		address = 0x9000;
	}
	while((c = fgetc(fp)) != EOF) {
		if(segment == 0) {
			prog_memory[address++] = c;
		} else {
			prog2_memory[address++] = c;
		}
		bytecount++;
	}

	printf("Loaded %d bytes.\n", bytecount);
	fclose(fp);
}


// load a binary file
void load_bin1(char *filenamebase)
{
	char filename[128];
	FILE *fp;
	int c, bytecount;
	unsigned short address;

	// Construct filename
	strcpy(filename, filenamebase);
	strcat(filename, ".bin");

	printf("Loading Binary File: %s...\n", filename);

	if((fp = fopen(filename, "rb")) == (FILE *)NULL) {
		printf("Can't open %s!\n", filename);
		return;
	}

	bytecount = 0;

	address = 0x8000;

	while((c = fgetc(fp)) != EOF) {
		prog2_memory[address++] = c;
		bytecount++;
	}
	printf("Loaded %d bytes.\n", bytecount);
	fclose(fp);
}


// load a flash text file
void load_flash_text(void)
{
	char filename[128];
	FILE *fp;
	unsigned short address;
	unsigned char bytecount, byte;

	printf("Load Flash Text File...\n");

	printf("Filename? ");
	scanf("%s", &filename[0]);
	getchar();

	if((fp = fopen(filename, "r")) == (FILE *)NULL) {
		printf("Can't open %s!\n", filename);
		return;
	}

	// process file
	bytecount = 0;
	address = FLASH_START;	// start of printer readable flash
	while(1) {
		if(fscanf(fp, "%02x", &byte) != 1) {
			break;
		}
		flash_memory[address++] = byte;
		bytecount++;
	}
	printf("Read %d bytes.\n", bytecount);
	fclose(fp);
}

// load rom text into segment 0 memory
void load_rom_text(void)
{
	char filename[128];
	FILE *fp;
	unsigned int address;
	unsigned int bytecount;
	unsigned char byte;

	printf("Load Rom Text File...\n");

	printf("Filename? ");
	scanf("%s", &filename[0]);
	getchar();

	if((fp = fopen(filename, "r")) == (FILE *)NULL) {
		printf("Can't open %s!\n", filename);
		return;
	}

	// process file
	bytecount = 0;
	address = ROM_START;	// start of rom
	while(1) {
		if(fscanf(fp, "%02x", &byte) != 1) {
			break;
		}
		prog_memory[address++] = byte;
		bytecount++;
	}
	printf("Read %d bytes.\n", bytecount);
	fclose(fp);
}


// load rom0 binary file
void load_rom0(char *filenamebase)
{
	char filename[128];
	FILE *fp;
	int c, bytecount;
	unsigned short address;

	// Construct filename
	strcpy(filename, filenamebase);
	strcat(filename, ".rom0");

	printf("Loading Memory (Segment 0) from Binary File: %s...", filename);
	if((fp = fopen(filename, "rb")) == (FILE *)NULL) {
		printf("Can't open %s!\n", filename);
		return;
	}

	bytecount = 0;
	address = ROM_START;
	while((c = fgetc(fp)) != EOF) {
		prog_memory[address++] = c;
		bytecount++;
	}

	printf("Loaded %d bytes.\n", bytecount);
	fclose(fp);
}

// load rom1 binary file
void load_rom1(char *filenamebase)
{
	char filename[128];
	FILE *fp;
	int c, bytecount;
	unsigned short address;

	// Construct filename
	strcpy(filename, filenamebase);
	strcat(filename, ".rom1");

	printf("Loading Memory (Segment 1) from Binary File: %s...", filename);
	if((fp = fopen(filename, "rb")) == (FILE *)NULL) {
		printf("Can't open %s!\n", filename);
		return;
	}

	bytecount = 0;
	address = ROM1_START;
	while((c = fgetc(fp)) != EOF) {
		prog2_memory[address++] = c;
		bytecount++;
	}
	printf("Loaded %d bytes.\n", bytecount);
	fclose(fp);
}

// load ramio binary file
void load_ramio(char *filenamebase)
{
	char filename[128];
	FILE *fp;
	int c, bytecount;
	unsigned short address;

	// Construct filename
	strcpy(filename, filenamebase);
	strcat(filename, ".ramio");

	printf("Loading (Ram/IO) from Binary File: %s...", filename);
	if((fp = fopen(filename, "rb")) == (FILE *)NULL) {
		printf("Can't open %s!\n", filename);
		return;
	}

	bytecount = 0;
	address = 0x0;
	while((c = fgetc(fp)) != EOF) {

		// the ram and io are kept in both page 00 and page 80 images
		prog_memory[address] = c;
		prog2_memory[address] = c;

		address++;
		bytecount++;
	}
	printf("Loaded %d bytes\n", bytecount);
	fclose(fp);
}

// load flash bin
void load_flash(char *filenamebase)
{
	char filename[128];
	FILE *fp;
	int c, bytecount;
	unsigned short address;

	// Construct filename
	strcpy(filename, filenamebase);
	strcat(filename, ".flsh");

	printf("Loading Flash from Binary File: %s...", filename);
	if((fp = fopen(filename, "rb")) == (FILE *)NULL) {
		printf("Can't open %s!\n", filename);
		return;
	}

	bytecount = 0;
	address = FLASH_START;
	while((c = fgetc(fp)) != EOF) {
		flash_memory[address++] = c;
		bytecount++;
	}
	printf("Loaded %d bytes\n", bytecount);
	fclose(fp);
}

// save memory segment 0 to a binary file
void save_rom0(char *filenamebase)
{
	char filename[128];
	FILE *fp;
	int c, bytecount;
	unsigned short address;

	// Construct filename
	strcpy(filename, filenamebase);
	strcat(filename, ".rom0");

	printf("Saving Memory (Segment 0) to Binary File: %s...", filename);
	if((fp = fopen(filename, "wb")) == (FILE *)NULL) {
		printf("Can't open %s!\n", filename);
		return;
	}

	bytecount = 0;
	address = ROM_START;
	while(address != MEMSIZE) {
		c = prog_memory[address++];
		fputc(c, fp);
		bytecount++;
	}
	printf("Wrote %d bytes.\n", bytecount);
	fclose(fp);
}

// save memory segment 1 to a binary file
void save_rom1(char *filenamebase)
{
	char filename[128];
	FILE *fp;
	int c, bytecount;
	unsigned short address;

	// Construct filename
	strcpy(filename, filenamebase);
	strcat(filename, ".rom1");

	printf("Saving Memory (Segment 1) to Binary File: %s...", filename);
	if((fp = fopen(filename, "wb")) == (FILE *)NULL) {
		printf("Can't open %s!\n", filename);
		return;
	}

	bytecount = 0;
	address = ROM1_START;
	while(address != MEMSIZE) {
		c = prog2_memory[address++];
		fputc(c, fp);
		bytecount++;
	}
	printf("Wrote %d bytes\n", bytecount);
	fclose(fp);
}

// save memory ram/io to a binary file
void save_ramio(char *filenamebase)
{
	char filename[128];
	FILE *fp;
	int c, bytecount;
	unsigned short address;

	// Construct filename
	strcpy(filename, filenamebase);
	strcat(filename, ".ramio");

	printf("Saving Memory (Ram/IO) to Binary File: %s...", filename);
	if((fp = fopen(filename, "wb")) == (FILE *)NULL) {
		printf("Can't open %s!\n", filename);
		return;
	}

	bytecount = 0;
	address = 0x0;
	while(address != FLASH_START) {
		// we could have taken this from either page image
		c = prog_memory[address++];
		fputc(c, fp);
		bytecount++;
	}
	printf("Wrote %d bytes\n", bytecount);
	fclose(fp);
}

// save flash to a binary file
void save_flash(char *filenamebase)
{
	char filename[128];
	FILE *fp;
	int c, bytecount;
	unsigned short address;

	// Construct filename
	strcpy(filename, filenamebase);
	strcat(filename, ".flsh");

	printf("Saving Flash to Binary File: %s...", filename);
	if((fp = fopen(filename, "wb")) == (FILE *)NULL) {
		printf("Can't open %s!\n", filename);
		return;
	}

	bytecount = 0;
	address = FLASH_START;
	while(address != ROM_START) {
		c = flash_memory[address++];
		fputc(c, fp);
		bytecount++;
	}
	printf("Wrote %d bytes\n", bytecount);
	fclose(fp);
}

// save selected memory to a binary file
void save_bin(char *filenamebase)
{
	char filename[128];
	FILE *fp;
	int c, bytecount, segment;
	unsigned short address;

	// Construct filename
	strcpy(filename, filenamebase);
	strcat(filename, ".bin");

	printf("Saving Memory to Binary File: %s..\n", filename);
	if((fp = fopen(filename, "wb")) == (FILE *)NULL) {
		printf("Can't open %s!\n", filename);
		return;
	}

	printf("Segment (0/1=80)? ");
	scanf("%d", &segment);
	getchar();

	bytecount = 0;
	address = 0;
	while(address != MEMSIZE) {
		if(segment == 0) {
			c = prog_memory[address++];
		} else {
			c = prog2_memory[address++];
		}
		fputc(c, fp);
		bytecount++;
	}
	printf("Wrote %d bytes\n", bytecount);
	fclose(fp);
}

// save processor state to a file
void save_state(char *filenamebase)
{
	char filename[128];
	FILE *fp;

	// Construct filename
	strcpy(filename, filenamebase);
	strcat(filename, ".txt");

	printf("Saving Processor State to File: %s...", filename);
	if((fp = fopen(filename, "w")) == (FILE *)NULL) {
		printf("Can't open %s!\n", filename);
		return;
	}

	fprintf(fp, "REG_A %02x\n", register_a);
	fprintf(fp, "REG_X %02x\n", register_x);
	fprintf(fp, "REG_Y %02x\n", register_y);
	fprintf(fp, "REG_SP %04x\n", register_sp);
	fprintf(fp, "REG_PC %08x\n", register_pc);
	fprintf(fp, "REG_CC %02x\n", register_cc);
	fprintf(fp, "SIMTIME %d\n", get_sim_time());

	fclose(fp);
	printf("Done.\n");
}

// load processor state file
void load_state(char *filenamebase)
{
	char filename[128];
	FILE *fp;
	unsigned int val;

	// Construct filename
	strcpy(filename, filenamebase);
	strcat(filename, ".txt");

	printf("Loading Processor State from File: %s...", filename);
	if((fp = fopen(filename, "r")) == (FILE *)NULL) {
		printf("Can't open %s!\n", filename);
		return;
	}

	// Read state
	fscanf(fp, "REG_A %02x\n", &val);
	register_a = val;

	fscanf(fp, "REG_X %02x\n", &val);
	register_x = val;

	fscanf(fp, "REG_Y %02x\n", &val);
	register_y = val;

	fscanf(fp, "REG_SP %04x\n", &val);
	register_sp = val;

	fscanf(fp, "REG_PC %08x\n", &val);
	register_pc = val;

	fscanf(fp, "REG_CC %02x\n", &val);
	register_cc = val;

	fscanf(fp, "SIMTIME %d\n", &val);
	set_sim_time(val);

	fclose(fp);
	printf("Done.\n");
}

// Load and Save Snapshots
void snapshot(void)
{
	int c;
	char filename[128];

	printf("\nSnapshots\n\n");

	printf("<L>oad\n");
	printf("<S>ave\n");

	printf("> ");
	c = getchar();
	getchar();

	if(c == 'l') {

		printf("Filename? ");
		scanf("%s", &filename[0]);
		getchar();

		load_rom0(filename);
		load_rom1(filename);
		load_flash(filename);
		load_ramio(filename);
		load_state(filename);

		// RJS this is because of a bug
		printf("Reloading Rom1...\n");
		load_bin1("hp_laser_rom_108000");

		printf("Snapshot Components Loaded.\n");

	} else {

		printf("Filename? ");
		scanf("%s", &filename[0]);
		getchar();

		save_rom0(filename);
		save_rom1(filename);
		save_flash(filename);
		save_ramio(filename);
		save_state(filename);

		printf("Snapshot Components Saved.\n");
	}
}

