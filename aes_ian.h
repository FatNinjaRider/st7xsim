//-----------------------------------------------------------------------------
// 
//   aes_ian.h - AES definitions
//
//   Updates:
//
//-----------------------------------------------------------------------------

void aes_set_key(uint8 *key, int keysize);
void aes_encrypt_ecb(uint8 *inbuf, uint8 *outbuf, int keysize);
void aes_decrypt_ecb(uint8 *inbuf, uint8 *outbuf, int keysize);

