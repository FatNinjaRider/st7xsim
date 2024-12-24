//
//---------------------------------------------------------------------------
//
// ST7x Simulator - file i/o header
//
// Author: Rick Stievenart
//
// Genesis: 03/04/2011
//
//----------------------------------------------------------------------------
//

int fgetline(FILE *fp, char *buff);
void load_srec(void);
void load_bin(char *filenamebase);
void load_flash_text(void);
void load_rom_text(void);
void load_rom0(char *filenamebase);
void load_rom1(char *filenamebase);
void load_ramio(char *filenamebase);
void load_flash(char *filenamebase);
void save_rom0(char *filenamebase);
void save_rom1(char *filenamebase);
void save_ramio(char *filenamebase);
void save_flash(char *filenamebase);
void save_bin(char *filenamebase);
void save_state(char *filenamebase);
void load_state(char *filenamebase);
void snapshot(void);
