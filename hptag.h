/****************************************************************************************
* 
*   hptag.h - Main definitions
*
*****************************************************************************************/

// HP I2C command response protocol definitions

#define CMD_ECHO					0x00	// echo
#define CMD_RESET					0x01	// reset
#define CMD_GET_RANDOM_BYTES		0x02	// get random bytes
#define CMD_AUTH					0x03	// auth
#define CMD_DECREMENT_COUNTER		0x04	// decrement counter	
#define CMD_READ					0x05	// read memory
#define CMD_WRITE					0x06	// write memory
#define CMD_0x07					0x07	// 07 - not implemented in oem - SCC uses to change POR low thresold to alternate level										// 08 - not implemented in oem
											// 09 - not implemented in oem											
											// 0a - not implemented in oem
											// 0b - not implemented in oem
											// 0c - not implemented in oem
											// 0d - not implemented in oem
											// 0e - not implemented in oem
											// 0f - not implemented in oem																																																							
#define CMD_SET_INFO				0x10	// set info
											// 11 - not implemented in oem
#define CMD_WRITE_KEYS				0x12	// write keys
#define CMD_WRITE_DATA				0x13	// write data
#define CMD_GET_INFO				0x14	// get info command
											// 15 - not implemented in oem
											// 16 - not implemented in oem
											// 17 - not implemented in oem
											// 18 - not implemented in oem
#define CMD_HALT					0x19	// halt command - used to initialize crc save area
#define CMD_EEPROM_CHECKSUM			0x1a	// eeprom checksum command
#define CMD_ROM_CHECKSUM			0x1b	// rom checksum command
#define CMD_SET_I2C_CONTROL			0x1c	// set i2c control command
#define CMD_0x1D					0x1d	// used by SCC
#define CMD_0x1E					0x1e	// used by SCC
#define CMD_0x1F					0x1f	// backdoor write relative (read relative in engineering code)


// error status conditions
#define STATUS_NO_ERROR				0x00
#define STATUS_INVALID_CRC			0x01	// command incurred a CRC error
#define STATUS_INVALID_LENGTH		0x02	// invalid length
#define STATUS_0x03					0x03	// NOT USED ANYWHERE
#define STATUS_INVALID_CMD			0x04	// invalid command received
#define STATUS_0x05					0x05	// NOT USED ANYWHERE
#define STATUS_0x06					0x06	// invalid output packet length  (too big)
#define STATUS_BAD_AUTH_TYPE		0x07	// used by VerifyMAC
#define STATUS_0x08					0x08	// used by command 4/5/6, and VerifyMAC (bad length)
#define STATUS_0x09					0x09	// used by VerifyMAC
#define STATUS_NO_KEY				0x0a	// no key, key not programmed  
#define STATUS_0x0B					0x0b	// used by auth
#define STATUS_BAD_KEY_VALUE		0x0c	// used by cmd 0x12
#define STATUS_0x0D					0x0d	// NOT USED ANYWHERE
#define STATUS_0x0E					0x0e	// NOT USED ANYWHERE
#define STATUS_INVALID_LOCK_STATE	0x0f	// bad parameter
#define STATUS_INVALID_REGION		0x10	// invalid region
#define STATUS_NO_AUTH				0x11	// auth has not been performed
#define STATUS_0x12					0x12	// used by write command
#define STATUS_0x13					0x13	// used by decrement counter (dec val > counter)
#define STATUS_INVALID_ADDRESS		0x14	// used by command 4/5/6
#define STATUS_0x15					0x15	// NOT USED ANYWHERE
#define STATUS_INVALID_MAC			0x16	// used by VerifyMAC
#define STATUS_0x17					0x17	// NOT USED ANYWHERE
#define STATUS_0x18					0x18	// NOT USED ANYWHERE
#define STATUS_0x19					0x19	// NOT USED ANYWHERE
#define STATUS_0x1A					0x1a	// NOT USED ANYWHERE
#define STATUS_0x1B					0x1b	// NOT USED ANYWHERE
#define STATUS_0x1C					0x1c	// NOT USED ANYWHERE
#define STATUS_0x1D					0x1d	// NOT USED ANYWHERE
#define STATUS_0x1E					0x1e	// NOT USED ANYWHERE
#define STATUS_0x1F					0x1f	// NOT USED ANYWHERE

// misc definitions
#define MAC_SIZE 4					// the size of the MACs

#define STATUS_MASK					0x1f

// jet command modifier bits
#define JET_CMD_MASK			0x1f	// 5 bits of command

#define JET_CMD_BIT5			0x20	// bit 5 - illegal
#define JET_CMD_BIT6			0x40	// bit 6 - t2p mac
#define JET_CMD_BIT7			0x80	// bit 7 - p2t mac