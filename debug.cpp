//-----------------------------------------------------------------------------
// 
//   debug.c - debug implementation
//
//   Author:
//      Rick Stievenart - 08/12/2015
//
//   Updates:
//
//-----------------------------------------------------------------------------

#include <stdio.h>

#include "types.h"
#include "debug.h"

//
//------------------------------------------------------
// Output a character to the UART
//------------------------------------------------------
//	
void outchar(uint32 ch)
{
	putchar(ch);
}

//
//------------------------------------------------------
// print an asciiz string
//------------------------------------------------------
//
void print_txt(char *txt)
{
	while (*txt != 0) {
		outchar(*txt++);
	}
}

//
//------------------------------------------------------
// output a newline sequence
//------------------------------------------------------
//
void newline(void)
{
	print_txt("\r\n");
}

//
//------------------------------------------------------
// print an asciiz string followed by a newline
//------------------------------------------------------
//
void print_line(char *txt)
{
	while (*txt != 0) {
		outchar(*txt++);
	}
	outchar('\r');
	outchar('\n');
}

//
//------------------------------------------------------
// print an decimal number
//------------------------------------------------------
//
void print_num(int n)
{
    const int x[5]={10000,1000,100,10,1};
    char ch;
    int i, zp;
    
    zp=0;
    for (i=0;i<5;i++) {
        ch='0';
        while (n>=x[i]) {
            n-=x[i];
            ch++;
        }
        if (zp==0 && ch=='0' && i!=4) ch=' '; else zp=1;
        outchar(ch);
    }
}

//
//------------------------------------------------------
// print a 32-bit hexadecimal number
//------------------------------------------------------
//
void uint32tohex(uint32 value)   
{   
    int     i, d,shift;   
    uint32 cval,mask;   
 
    i=0;   
    shift=28;   
    cval=value;   
    mask = 0xffffffff;   

    for(i=0;i<8;i++) {   
        d=cval>>shift;   
        cval &= (mask>>(32-shift));   
 
        d = d & 0x0f;
                  
        if(d > 9 ) {   
            d += ('A'-10);   
        } else {   
            d += '0';   
        }
        outchar((unsigned char)(d & 0xff));   
        shift = shift - 4;   
	}   
}

//
//------------------------------------------------------
// print a 16-bit hexadecimal number
//------------------------------------------------------
//
void uint16tohex(uint16 value)   
{   
    int32  i, d,shift;   
    uint16 cval,mask;   
 
    i=0;   
    shift=12;   
    cval=value;   
    mask = 0xffff;   

    for(i=0;i<4;i++) {   
        d=cval>>shift;   
        cval &= (mask>>(16-shift));   
           
        d = d & 0x0f;
                  
        if(d > 9 ) {  
            d += ('A'-10);   
        } else {   
            d += '0';
        }    
        outchar((unsigned char)(d & 0xff));   
        shift -=4;   
	}   
}   


//
//------------------------------------------------------
// print an 8-bit hexadecimal number
//------------------------------------------------------
//
void uint8tohex(uint8 value)   
{   
    int32 i, d, shift;   
    uint8 cval, mask;   
 
    i = 0;   
    shift = 4;   
    cval = value;   
    mask = 0xff;   

    for(i=0;i<2;i++) {   
        d = cval >> shift;   
        cval &= (mask >> (8 - shift));   
           
        if(d > 9 ) {   
            d += ('A' - 10);
        } else {   
            d += '0';
        }
        outchar((uint8)(d & 0xff));   
        shift -= 4;   
	}   
} 

//
//------------------------------------------------------
// print memory as hexbytes for length
//------------------------------------------------------
//
void print_hex(uint8 *txt, uint32 len)
{
	uint32 bytes;
	
	bytes = 0;
	
	print_txt("\r\nMemory at ");
	uint32tohex((uint32)txt);
	print_txt(":\r\n");
	
	while (len--) {
		if(bytes == 0) {
			uint32tohex((uint32)txt);
			print_txt(": ");
		}
		uint8tohex(*txt++);
		outchar(' ');
		bytes++;
		if(bytes == 16) {
			print_txt("\r\n");
			bytes = 0;
		}
	}
	print_txt("\r\n");
}
