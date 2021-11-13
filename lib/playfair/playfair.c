#include <stdint.h>

#include "playfair.h"

void generate_key_schedule(unsigned char* key_material, uint32_t key_schedule[11][4]);
void generate_session_key(unsigned char* oldSap, unsigned char* messageIn, unsigned char* sessionKey);
void cycle(unsigned char* block, uint32_t key_schedule[11][4]);
void z_xor(unsigned char* in, unsigned char* out, int blocks);
void x_xor(unsigned char* in, unsigned char* out, int blocks);

extern unsigned char default_sap[];

void playfair_decrypt(unsigned char* message3, unsigned char* cipherText, unsigned char* keyOut)
{
	unsigned char* chunk1 = &cipherText[16];
	unsigned char* chunk2 = &cipherText[56];
	int i;
	unsigned char blockIn[16];
	unsigned char sapKey[16];
	uint32_t key_schedule[11][4];
	generate_session_key(default_sap, message3, sapKey);	
	generate_key_schedule(sapKey, key_schedule);
	z_xor(chunk2, blockIn, 1);
	cycle(blockIn, key_schedule);
	for (i = 0; i < 16; i++) {
		keyOut[i] = blockIn[i] ^ chunk1[i];
	}
	x_xor(keyOut, keyOut, 1);
	z_xor(keyOut, keyOut, 1);
}

