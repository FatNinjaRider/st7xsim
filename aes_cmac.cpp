//-----------------------------------------------------------------------------
// 
//   aes_ian.c - AES implementation
//
//   Authors:
//      Mike Shelby - 08/12/2015
//      Rick Stievenart - 08/12/2015
//      Ian Rooze - 08/12/2015
//
//   Updates:
//
//-----------------------------------------------------------------------------

#include "types.h"
#include "utils.h"
#include "aes_ian.h"
#include "aes_cmac.h"
#include "debug.h"

#define DEBUG_MACS 1

// xor sixteen bytes
void xor_128(uint8 *a, uint8 *b, uint8 *out)
{
	int i;
	
	for (i = 0; i < 16; i++) {
		out[i] = a[i] ^ b[i];
	}
}

// AES-CMAC Generation Function
void leftshift_onebit(uint8 *input, uint8 *output)
{
	int32 i, overflow;

	overflow = 0;

	for ( i = 15; i >= 0; i-- ) {
		output[i] = (uint8)(input[i] << 1);
		output[i] |= overflow;
		overflow = (input[i] & 0x80) ? 1:0;
	}
}

void aes_cmac_padding(uint8 *lastb, uint8 *pad, uint16 length)
{
	int32 j;

	// original last block
	for ( j=0; j < 16; j++ ) {
		if ( j < length ) {
			pad[j] = lastb[j];
		} else if (j == length) {
			pad[j] = 0x80;
		} else {
			pad[j] = 0x00;
		}   
	}
}

void aes_cmac_generate_subkey(uint8 *K1, uint8 *K2)
{
	/* L = AES(zeros) */
	mymemset(K1, 0, 16);
	aes_encrypt_ecb(K1, K2, 128);

	/* If MSB(L) = 0, then K1 = L << 1 */
	/* Else K1 = ( L << 1 ) (+) Rb */
	leftshift_onebit(K2, K1);
	if ((K2[0] & 0x80) != 0) {
		K1[15] ^= 0x87;
	}
	/* If MSB(K1) = 0, then K2 = K1 << 1 */
	/* Else K2 = ( K1 << 1 ) (+) Rb */
	leftshift_onebit(K1, K2);
	if ((K1[0] & 0x80) != 0) {
		K2[15] ^= 0x87;
	}
}

//
// this is a modified version of the aes_cmac_sign
//
void aes_cmac_sign_new(int param, uint8 *input, uint16 length, uint8 *prev, uint8 *mac)
{
    uint32 ln, li, flag, mults;
	uint8 M_last[16], X[16], Y[16];
	uint8 K1[16], K2[16];
	int special_case;

#ifdef DEBUG_MACS
	print_txt("acsn(");
	uint32tohex((uint32)param);
	print_txt(",");
	uint32tohex((uint32)input);
	print_txt(",");
	uint16tohex(length);
	print_txt(",");
	uint32tohex((uint32)prev);
	print_txt(",");
	uint32tohex((uint32)mac);		
	print_txt(")\r\n");
#endif
 
	ln = (length + 15) >> 4; // speed up of ln = (length + 15) / 16
    if ( ln == 0 ) {
        ln = 1;
        flag = 0;
    } else {
	    if ((length & 0x0f) == 0 ) {
			// last block is a complete block
	        flag = 1;
		} else {
			// last block is not complete block
            flag = 0;
		}
	}

	if ((param == 3) || (param == 5)) {
		// 3 or 5
		mymemcpy((uint8 *)&X[0], prev, 16);
	} else {
		// all others
		mymemset((uint8 *)&X[0], 0, 16);

		if((param == 2) || (param == 4)) {
			// rjs mod
			if(length > 16) {
				mults = length >> 4; // speed up of length / 16
				mults <<= 4; // speed up of mults *= 16;
				if(((length - mults) & 0xff) == 0) {
					special_case = 1;
					ln++;
				} else {
					special_case = 0;
				}
			} else {
				if(length == 16) {
					special_case = 1;
						ln++;
				} else {
					special_case = 0;
				}
			}
		}
		// end RJS mod
	}

	for ( li=0; li < ln-1; li++ ) {
		xor_128((uint8 *)&X[0], (uint8 *)&input[(li << 4)], (uint8 *)&Y[0]);	// Y := Mi (+) X
		aes_encrypt_ecb((uint8 *)&Y[0], (uint8 *)&X[0], 128);			// X := AES-128(KEY, Y);
	}

	if ((param == 2) || (param == 4)) {
		// 2 or 4

		if ( flag ) { // last block is complete block
			// RJS mod
			if(special_case) {
				M_last[0] = 0x80;
				mymemset((uint8 *)&M_last[1], 0, 15);
			} else {
				mymemcpy((uint8 *)&M_last[0], &input[((ln - 1) << 4)], 16);
			}
			// end RJS mod
		} else {
			aes_cmac_padding((uint8 *)&input[((ln - 1) << 4)], (uint8 *)&M_last[0], (uint16)(length & 0x0f));
		}

#ifdef DEBUG_MACS
		print_txt("param==2|4: M_last=");
		print_hex((uint8 *)&M_last[0], 16);
		newline();	
#endif
		
	} else {
		// all others

		aes_cmac_generate_subkey((uint8 *)&K1[0], (uint8 *)&K2[0]);

#ifdef DEBUG_MACS
		print_txt("acme: K1=");
		print_hex((uint8 *)&K1[0], 16);
		newline();

		print_txt("acme: K2=");
		print_hex((uint8 *)&K2[0], 16);
		newline();
#endif

		if (flag) {
			// last block is complete block
			xor_128((uint8 *)&input[((ln - 1) << 4)], (uint8 *)&K1[0], (uint8 *)&M_last[0]);
		} else {
			// last block is not complete block
			aes_cmac_padding((uint8 *)&input[((ln - 1) << 4)], (uint8 *)&Y[0], (uint16)(length & 0x0f));

#ifdef DEBUG_MACS
			print_txt("acme: after cmac_padding Y=");
			print_hex((uint8 *)&Y[0], 16);
			newline();
#endif
			xor_128((uint8 *)&Y[0], (uint8 *)&K2[0], (uint8 *)&M_last[0]);
		}
	}

#ifdef DEBUG_MACS
	print_txt("acme: X=");
	print_hex((uint8 *)&X[0], 16);
	newline();
	
	print_txt("acme: M_last=");
	print_hex((uint8 *)&M_last[0], 16);
	newline();	
#endif

	xor_128((uint8 *)&X[0], (uint8 *)&M_last[0], (uint8 *)&Y[0]);
	
#ifdef DEBUG_MACS
	print_txt("acme: Y=");
	print_hex((uint8 *)&Y[0], 16);
	newline();
#endif
	
	aes_encrypt_ecb((uint8 *)&Y[0], (uint8 *)&X[0], 128);


#ifdef DEBUG_MACS
	print_txt("acme: Output X=");
	print_hex((uint8 *)&X[0], 16);
	newline();
#endif

	mymemcpy((uint8 *)&mac[0], (uint8 *)&X[0], 16);
}


