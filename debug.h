//-----------------------------------------------------------------------------
// 
//
//    debug.h - debug interface definitions
//
//   Author:
//      Rick Stievenart - 08/12/2015
//
//   Updates:
//
//-----------------------------------------------------------------------------

void outchar(uint32 ch);
void newline(void);
void print_num(int32 n);
void uint32tohex(uint32 value);   
void uint16tohex(uint16 value);   
void uint8tohex(uint8 value);   
void print_hex(uint8 *txt, uint32 len);
void print_txt(char *txt);
void print_line(char *txt);
