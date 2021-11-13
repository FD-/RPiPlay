void modified_md5(unsigned char* originalblockIn, unsigned char* keyIn, unsigned char* keyOut);
void sap_hash(unsigned char* blockIn, unsigned char* keyOut);

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "omg_hax.h"

#define printf(...) (void)0;

void xor_blocks(unsigned char* a, unsigned char* b, unsigned char* out)
{
   for (int i = 0; i < 16; i++)
      out[i] = a[i] ^ b[i];
}


void z_xor(unsigned char* in, unsigned char* out, int blocks)
{
   for (int j = 0; j < blocks; j++)
      for (int i = 0; i < 16; i++)
         out[j*16+i] = in[j*16+i] ^ z_key[i];   
}

void x_xor(unsigned char* in, unsigned char* out, int blocks)
{
   for (int j = 0; j < blocks; j++)
      for (int i = 0; i < 16; i++)
         out[j*16+i] = in[j*16+i] ^ x_key[i];   
}


void t_xor(unsigned char* in, unsigned char* out)
{
   for (int i = 0; i < 16; i++)
      out[i] = in[i] ^ t_key[i];   
}

unsigned char sap_iv[] = {0x2B,0x84,0xFB,0x79,0xDA,0x75,0xB9,0x04,0x6C,0x24,0x73,0xF7,0xD1,0xC4,0xAB,0x0E,0x2B,0x84,0xFB,0x79,0x75,0xB9,0x04,0x6C,0x24,0x73};

unsigned char sap_key_material[] = {0xA1, 0x1A, 0x4A, 0x83,
                                    0xF2, 0x7A, 0x75, 0xEE,
                                    0xA2, 0x1A, 0x7D, 0xB8,
                                    0x8D, 0x77, 0x92, 0xAB};

unsigned char index_mangle[] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1B, 0x36, 0x6C};

unsigned char* table_index(int i)
{
   return &table_s1[((31*i) % 0x28) << 8];
}

unsigned char* message_table_index(int i)
{   
   return &table_s2[(97*i % 144) << 8];   
}

void print_block(char* msg, unsigned char* dword)
{
   printf("%s", msg);
   for (int i = 0; i < 16; i++)
      printf("%02X ", dword[i]);
   printf("\n");
}

void permute_block_1(unsigned char* block)
{
   block[0] = table_s3[block[0]];
   block[4] = table_s3[0x400+block[4]];
   block[8] = table_s3[0x800+block[8]];
   block[12] = table_s3[0xc00+block[12]];
   
   unsigned char tmp = block[13];
   block[13] = table_s3[0x100+block[9]];
   block[9] = table_s3[0xd00+block[5]];
   block[5] = table_s3[0x900+block[1]];
   block[1] = table_s3[0x500+tmp];

   tmp = block[2];
   block[2] = table_s3[0xa00+block[10]];
   block[10] = table_s3[0x200+tmp];
   tmp = block[6];
   block[6] = table_s3[0xe00+block[14]];
   block[14] = table_s3[0x600+tmp];

   tmp = block[3];
   block[3] = table_s3[0xf00+block[7]];
   block[7] = table_s3[0x300+block[11]];
   block[11] = table_s3[0x700+block[15]];
   block[15] = table_s3[0xb00+tmp];
   print_block("Permutation complete. Final value of block: ", block); // This looks right to me, at least for decrypt_kernel
}

unsigned char* permute_table_2(unsigned int i)
{
   return &table_s4[((71 * i) % 144) << 8];
}

void permute_block_2(unsigned char* block, int round)
{
   // round is 0..8?
   printf("Permuting via table2, round %d... (block[0] = %02X)\n", round, block[0]);
   block[0] = permute_table_2(round*16+0)[block[0]];
   block[4] = permute_table_2(round*16+4)[block[4]];
   block[8] = permute_table_2(round*16+8)[block[8]];
   block[12] = permute_table_2(round*16+12)[block[12]];
   
   unsigned char tmp = block[13];
   block[13] = permute_table_2(round*16+13)[block[9]];
   block[9] = permute_table_2(round*16+9)[block[5]];
   block[5] = permute_table_2(round*16+5)[block[1]];
   block[1] = permute_table_2(round*16+1)[tmp];

   tmp = block[2];
   block[2] = permute_table_2(round*16+2)[block[10]];
   block[10] = permute_table_2(round*16+10)[tmp];
   tmp = block[6];
   block[6] = permute_table_2(round*16+6)[block[14]];
   block[14] = permute_table_2(round*16+14)[tmp];

   tmp = block[3];
   block[3] = permute_table_2(round*16+3)[block[7]];
   block[7] = permute_table_2(round*16+7)[block[11]];
   block[11] = permute_table_2(round*16+11)[block[15]];
   block[15] = permute_table_2(round*16+15)[tmp];
   print_block("Permutation (2) complete. Final value of block: ", block); // This looks right to me, at least for decrypt_kernel
}

// This COULD just be Rijndael key expansion, but with a different set of S-boxes
void generate_key_schedule(unsigned char* key_material, uint32_t key_schedule[11][4])
{   
   uint32_t key_data[4];
   int i;
   for (i = 0; i < 11; i++)
   {
      key_schedule[i][0] = 0xdeadbeef;
      key_schedule[i][1] = 0xdeadbeef;
      key_schedule[i][2] = 0xdeadbeef;
      key_schedule[i][3] = 0xdeadbeef;
   }
   unsigned char* buffer = (unsigned char*)key_data;
   int ti = 0;
   printf("Generating key schedule\n");
   // G
   print_block("Raw key material: ", key_material);
   t_xor(key_material, buffer);   
   print_block("G has produced: ", buffer);
   for (int round = 0; round < 11; round++)
   {
      printf("Starting round %d\n", round);
      // H
      key_schedule[round][0] = key_data[0];
      printf("H has set chunk 1 of round %d %08X\n", round, key_schedule[round][0]);
      printf("H complete\n");
      // I
      unsigned char* table1 = table_index(ti);
      unsigned char* table2 = table_index(ti+1);
      unsigned char* table3 = table_index(ti+2);
      unsigned char* table4 = table_index(ti+3);
      ti += 4;
      //buffer[0] = (buffer[0] - (4 & (buffer[0] << 1)) + 2) ^ 2 ^ index_mangle[round] ^ table1[buffer[0x0d]];
      printf("S-box: 0x%02x -> 0x%02x\n", buffer[0x0d], table1[buffer[0x0d]]);
      printf("S-box: 0x%02x -> 0x%02x\n", buffer[0x0e], table2[buffer[0x0e]]);
      printf("S-box: 0x%02x -> 0x%02x\n", buffer[0x0f], table3[buffer[0x0f]]);
      printf("S-box: 0x%02x -> 0x%02x\n", buffer[0x0c], table4[buffer[0x0c]]);
      buffer[0] ^= table1[buffer[0x0d]] ^ index_mangle[round];
      buffer[1] ^= table2[buffer[0x0e]];
      buffer[2] ^= table3[buffer[0x0f]];
      buffer[3] ^= table4[buffer[0x0c]];
      print_block("After I, buffer is now: ", buffer);
      printf("I complete\n");
      // H
      key_schedule[round][1] = key_data[1];
      printf("H has set chunk 2 to %08X\n", key_schedule[round][1]);

      printf("H complete\n");
      // J
      key_data[1] ^= key_data[0];
      printf("J complete\n");
      print_block("Buffer is now ", buffer);
      // H
      key_schedule[round][2] = key_data[2];
      printf("H has set chunk3 to %08X\n", key_schedule[round][2]);
      printf("H complete\n");      

      // J
      key_data[2] ^= key_data[1];
      printf("J complete\n");
      // K and L
      // Implement K and L to fill in other bits of the key schedule
      key_schedule[round][3] = key_data[3];
      // J again
      key_data[3] ^= key_data[2];
      printf("J complete\n");
   }
   for (i = 0; i < 11; i++)
      print_block("Schedule: ", (unsigned char*)key_schedule[i]);
}

// This MIGHT just be AES, or some variant thereof.
void cycle(unsigned char* block, uint32_t key_schedule[11][4])
{
   uint32_t ptr1 = 0;
   uint32_t ptr2 = 0;
   uint32_t ptr3 = 0;
   uint32_t ptr4 = 0;
   uint32_t ab;
   unsigned char* buffer = (unsigned char*)&ab;
   uint32_t* bWords = (uint32_t*)block;
   bWords[0] ^= key_schedule[10][0];
   bWords[1] ^= key_schedule[10][1];
   bWords[2] ^= key_schedule[10][2];
   bWords[3] ^= key_schedule[10][3];
   // First, these are permuted
   permute_block_1(block);

   for (int round = 0; round < 9; round++)
   {
      // E
      // Note that table_s5 is a table of 4-byte words. Therefore we do not need to <<2 these indices
      // TODO: Are these just T-tables?
      unsigned char* key0 = (unsigned char*)&key_schedule[9-round][0];
      ptr1 = table_s5[block[3] ^ key0[3]]; 
      ptr2 = table_s6[block[2] ^ key0[2]];
      ptr3 = table_s8[block[0] ^ key0[0]];
      ptr4 = table_s7[block[1] ^ key0[1]];

      // A B
      ab = ptr1 ^ ptr2 ^ ptr3 ^ ptr4;
      printf("ab: %08X %08X %08X %08X -> %08X\n", ptr1, ptr2, ptr3, ptr4, ab);
      // C
      ((uint32_t*)block)[0] = ab;
      printf("f7 = %02X\n", block[7]);
      unsigned char* key1 = (unsigned char*)&key_schedule[9-round][1];
      ptr2 = table_s5[block[7] ^ key1[3]];
      ptr1 = table_s6[block[6] ^ key1[2]];
      ptr4 = table_s7[block[5] ^ key1[1]];
      ptr3 = table_s8[block[4] ^ key1[0]];
      // A B again
      ab = ptr1 ^ ptr2 ^ ptr3 ^ ptr4;
      printf("ab: %08X %08X %08X %08X -> %08X\n", ptr1, ptr2, ptr3, ptr4, ab);
      // D is a bit of a nightmare, but it is really not as complicated as you might think
      unsigned char* key2 = (unsigned char*)&key_schedule[9-round][2];
      unsigned char* key3 = (unsigned char*)&key_schedule[9-round][3];
      ((uint32_t*)block)[1] = ab;
      ((uint32_t*)block)[2] = table_s5[block[11] ^ key2[3]] ^ 
                              table_s6[block[10] ^ key2[2]] ^ 
                              table_s7[block[9] ^ key2[1]] ^         
                              table_s8[block[8] ^ key2[0]];

      ((uint32_t*)block)[3] = table_s5[block[15] ^ key3[3]] ^ 
                              table_s6[block[14] ^ key3[2]] ^ 
                              table_s7[block[13] ^ key3[1]] ^ 
                              table_s8[block[12] ^ key3[0]];      
      printf("Set block2 = %08X, block3 = %08X\n", ((uint32_t*)block)[2], ((uint32_t*)block)[3]);
      // In the last round, instead of the permute, we do F
         permute_block_2(block, 8-round);         
   }      
   printf("Using last bit of key up: %08X xor %08X -> %08X\n", ((uint32_t*)block)[0], key_schedule[0][0], ((uint32_t*)block)[0] ^ key_schedule[0][0]);
   ((uint32_t*)block)[0] ^= key_schedule[0][0];
   ((uint32_t*)block)[1] ^= key_schedule[0][1];
   ((uint32_t*)block)[2] ^= key_schedule[0][2];
   ((uint32_t*)block)[3] ^= key_schedule[0][3];
}


void decrypt_sap(unsigned char* sapIn, unsigned char* sapOut)
{
   uint32_t key_schedule[11][4];
   unsigned char* iv;
   print_block("Base sap: ", &sapIn[0xf0]);
   z_xor(sapIn, sapOut, 16);
   generate_key_schedule(sap_key_material, key_schedule);
   print_block("lastSap before cycle: ", &sapOut[0xf0]);
   for (int i = 0xf0; i >= 0x00; i-=0x10)
   {
      printf("Ready to cycle %02X\n", i);
      cycle(&sapOut[i], key_schedule);
      print_block("After cycling, block is: ", &sapOut[i]);
      if (i > 0)
      { // xor with previous block
         iv = &sapOut[i-0x10];
      }
      else
      { // xor with sap IV
         iv = sap_iv;
      }
      for (int j = 0; j < 16; j++)
      {
         printf("%02X ^ %02X -> %02X\n", sapOut[i+j],  iv[j], sapOut[i+j] ^ iv[j]);
         sapOut[i+j] = sapOut[i+j] ^ iv[j];
      }
      printf("Decrypted SAP %02X-%02X:\n", i, i+0xf);
      print_block("", &sapOut[i]);
   }
   // Lastly grind the whole thing through x_key. This is the last time we modify sap
   x_xor(sapOut, sapOut, 16);
   printf("Sap is decrypted to\n");
   for (int i = 0xf0; i >= 0x00; i-=0x10)
   {
      printf("Final SAP %02X-%02X: ", i, i+0xf);
      print_block("", &sapOut[i]);
   }
}

unsigned char initial_session_key[] = {0xDC, 0xDC, 0xF3, 0xB9, 0x0B, 0x74, 0xDC, 0xFB, 0x86, 0x7F, 0xF7, 0x60, 0x16, 0x72, 0x90, 0x51};


void decrypt_key(unsigned char* decryptedSap, unsigned char* keyIn, unsigned char* iv, unsigned char* keyOut)
{
   unsigned char blockIn[16];
   uint32_t key_schedule[11][4];
   uint32_t mode_key_schedule[11][4];
   generate_key_schedule(&decryptedSap[8], key_schedule);
   printf("Generating mode key:\n");
   generate_key_schedule(initial_session_key, mode_key_schedule);
   z_xor(keyIn, blockIn, 1);
   print_block("Input to cycle is: ", blockIn);
   cycle(blockIn, key_schedule);
   for (int j = 0; j < 16; j++)
      keyOut[j] = blockIn[j] ^ iv[j];
   print_block("Output from cycle is: ", keyOut);
   x_xor(keyOut, keyOut, 1);
}


void decryptMessage(unsigned char* messageIn, unsigned char* decryptedMessage)
{
   unsigned char buffer[16];
   int i, j;
   unsigned char tmp;
   uint32_t key_schedule[11][4];
   int mode = messageIn[12];  // 0,1,2,3
   printf("mode = %02x\n", mode);
   generate_key_schedule(initial_session_key, key_schedule);
      
   // For M0-M6 we follow the same pattern
   for (i = 0; i < 8; i++)
   {      
      // First, copy in the nth block (we must start with the last one)
      for (j = 0; j < 16; j++)
      {
         if (mode == 3)
            buffer[j] = messageIn[(0x80-0x10*i)+j];
         else if (mode == 2 || mode == 1 || mode == 0)
            buffer[j] = messageIn[(0x10*(i+1))+j];   
      }
      // do this permutation and update 9 times. Could this be cycle(), or the reverse of cycle()?
      for (j = 0; j < 9; j++)
      {
         int base = 0x80 - 0x10*j;
         //print_block("About to cycle. Buffer is currently: ", buffer);
         buffer[0x0] = message_table_index(base+0x0)[buffer[0x0]] ^ message_key[mode][base+0x0];
         buffer[0x4] = message_table_index(base+0x4)[buffer[0x4]] ^ message_key[mode][base+0x4];
         buffer[0x8] = message_table_index(base+0x8)[buffer[0x8]] ^ message_key[mode][base+0x8];
         buffer[0xc] = message_table_index(base+0xc)[buffer[0xc]] ^ message_key[mode][base+0xc];

         tmp = buffer[0x0d];
         buffer[0xd] = message_table_index(base+0xd)[buffer[0x9]] ^ message_key[mode][base+0xd];
         buffer[0x9] = message_table_index(base+0x9)[buffer[0x5]] ^ message_key[mode][base+0x9];
         buffer[0x5] = message_table_index(base+0x5)[buffer[0x1]] ^ message_key[mode][base+0x5];
         buffer[0x1] = message_table_index(base+0x1)[tmp]         ^ message_key[mode][base+0x1];

         tmp = buffer[0x02];
         buffer[0x2] = message_table_index(base+0x2)[buffer[0xa]] ^ message_key[mode][base+0x2];
         buffer[0xa] = message_table_index(base+0xa)[tmp]         ^ message_key[mode][base+0xa];
         tmp = buffer[0x06];
         buffer[0x6] = message_table_index(base+0x6)[buffer[0xe]] ^ message_key[mode][base+0x6];
         buffer[0xe] = message_table_index(base+0xe)[tmp]         ^ message_key[mode][base+0xe];

         tmp = buffer[0x3];
         buffer[0x3] = message_table_index(base+0x3)[buffer[0x7]] ^ message_key[mode][base+0x3];
         buffer[0x7] = message_table_index(base+0x7)[buffer[0xb]] ^ message_key[mode][base+0x7];
         buffer[0xb] = message_table_index(base+0xb)[buffer[0xf]] ^ message_key[mode][base+0xb];
         buffer[0xf] = message_table_index(base+0xf)[tmp]         ^ message_key[mode][base+0xf];

         // Now we must replace the entire buffer with 4 words that we read and xor together
         uint32_t word;
         uint32_t* block = (uint32_t*)buffer;
         
         block[0] = table_s9[0x000 + buffer[0x0]] ^ 
                    table_s9[0x100 + buffer[0x1]] ^ 
                    table_s9[0x200 + buffer[0x2]] ^ 
                    table_s9[0x300 + buffer[0x3]];
         block[1] = table_s9[0x000 + buffer[0x4]] ^ 
                    table_s9[0x100 + buffer[0x5]] ^ 
                    table_s9[0x200 + buffer[0x6]] ^ 
                    table_s9[0x300 + buffer[0x7]];
         block[2] = table_s9[0x000 + buffer[0x8]] ^
                    table_s9[0x100 + buffer[0x9]] ^
                    table_s9[0x200 + buffer[0xa]] ^
                    table_s9[0x300 + buffer[0xb]];
         block[3] = table_s9[0x000 + buffer[0xc]] ^
                    table_s9[0x100 + buffer[0xd]] ^
                    table_s9[0x200 + buffer[0xe]] ^
                    table_s9[0x300 + buffer[0xf]];
      }
      // Next, another permute with a different table
      buffer[0x0] = table_s10[(0x0 << 8) + buffer[0x0]];
      buffer[0x4] = table_s10[(0x4 << 8) + buffer[0x4]];
      buffer[0x8] = table_s10[(0x8 << 8) + buffer[0x8]];
      buffer[0xc] = table_s10[(0xc << 8) + buffer[0xc]];

      tmp = buffer[0x0d];
      buffer[0xd] = table_s10[(0xd << 8) + buffer[0x9]];
      buffer[0x9] = table_s10[(0x9 << 8) + buffer[0x5]];
      buffer[0x5] = table_s10[(0x5 << 8) + buffer[0x1]];
      buffer[0x1] = table_s10[(0x1 << 8) + tmp];

      tmp = buffer[0x02];
      buffer[0x2] = table_s10[(0x2 << 8) + buffer[0xa]];
      buffer[0xa] = table_s10[(0xa << 8) + tmp];
      tmp = buffer[0x06];
      buffer[0x6] = table_s10[(0x6 << 8) + buffer[0xe]];
      buffer[0xe] = table_s10[(0xe << 8) + tmp];

      tmp = buffer[0x3];
      buffer[0x3] = table_s10[(0x3 << 8) + buffer[0x7]];
      buffer[0x7] = table_s10[(0x7 << 8) + buffer[0xb]];
      buffer[0xb] = table_s10[(0xb << 8) + buffer[0xf]];
      buffer[0xf] = table_s10[(0xf << 8) + tmp];

      // And finally xor with the previous block of the message, except in mode-2 where we do this in reverse
      if (mode == 2 || mode == 1 || mode == 0)
      {
         if (i > 0)
         {
            xor_blocks(buffer, &messageIn[0x10*i], &decryptedMessage[0x10*i]); // remember that the first 0x10 bytes are the header
         }
         else
            xor_blocks(buffer, message_iv[mode], &decryptedMessage[0x10*i]);
         print_block(" ", &decryptedMessage[0x10*i]);
      }
      else
      {
         if (i < 7)
            xor_blocks(buffer, &messageIn[0x70 - 0x10*i], &decryptedMessage[0x70 - 0x10*i]);
         else
            xor_blocks(buffer, message_iv[mode], &decryptedMessage[0x70 - 0x10*i]);
         printf("Decrypted message block %02X-%02X:", 0x70 - 0x10*i, 0x70 - 0x10*i+0xf);
         print_block(" ", &decryptedMessage[0x70 - 0x10*i]);
      }
   }
}

unsigned char static_source_1[] = {0xFA, 0x9C, 0xAD, 0x4D, 0x4B, 0x68, 0x26, 0x8C, 0x7F, 0xF3, 0x88, 0x99, 0xDE, 0x92, 0x2E, 0x95, 
                                   0x1E};
unsigned char static_source_2[] = {0xEC, 0x4E, 0x27, 0x5E, 0xFD, 0xF2, 0xE8, 0x30, 0x97, 0xAE, 0x70, 0xFB, 0xE0, 0x00, 0x3F, 0x1C, 
                                   0x39, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x09, 0x00, 0x0, 0x00, 0x00, 0x00, 0x00};

void swap_bytes(unsigned char* a, unsigned char *b)
{
   unsigned char c = *a;
   *a = *b;
   *b = c;
}

void generate_session_key(unsigned char* oldSap, unsigned char* messageIn, unsigned char* sessionKey)
{
   unsigned char decryptedMessage[128];
   unsigned char newSap[320];
   unsigned char Q[210];
   int i;
   int round;
   unsigned char md5[16];
   unsigned char otherHash[16];

   decryptMessage(messageIn, decryptedMessage);
   // Now that we have the decrypted message, we can combine it with our initial sap to form the 5 blocks needed to generate the 5 words which, when added together, give
   // the session key.
   memcpy(&newSap[0x000], static_source_1, 0x11);
   memcpy(&newSap[0x011], decryptedMessage, 0x80);
   memcpy(&newSap[0x091], &oldSap[0x80], 0x80);
   memcpy(&newSap[0x111], static_source_2, 0x2f);
   memcpy(sessionKey, initial_session_key, 16);

   for (round = 0; round < 5; round++)
   {
      unsigned char* base = &newSap[round * 64];
      print_block("Input block: ", &base[0]);
      print_block("Input block: ", &base[0x10]);
      print_block("Input block: ", &base[0x20]);
      print_block("Input block: ", &base[0x30]);
      modified_md5(base, sessionKey, md5);
      printf("MD5 OK\n");
      sap_hash(base, sessionKey);
      printf("OtherHash OK\n");
      
      printf("MD5       = ");
      for (i = 0; i < 4; i++)
         printf("%08x ", ((uint32_t*)md5)[i]);
      printf("\nOtherHash = ");
      for (i = 0; i < 4; i++)
        printf("%08x ", ((uint32_t*)sessionKey)[i]);
      printf("\n");

      uint32_t* sessionKeyWords = (uint32_t*)sessionKey;
      uint32_t* md5Words = (uint32_t*)md5;
      for (i = 0; i < 4; i++)
      {
         sessionKeyWords[i] = (sessionKeyWords[i] + md5Words[i]) & 0xffffffff;
      }      
      printf("Current key: ");
      for (i = 0; i < 16; i++)
         printf("%02x", sessionKey[i]);
      printf("\n");
   }
   for (i = 0; i < 16; i+=4)
   {
      swap_bytes(&sessionKey[i], &sessionKey[i+3]);
      swap_bytes(&sessionKey[i+1], &sessionKey[i+2]);
   }
      
   // Finally the whole thing is XORd with 121:
   for (i = 0; i < 16; i++)
     sessionKey[i] ^= 121;
   print_block("Session key computed as: ", sessionKey);
}

unsigned char default_sap[] =
  { 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79,
    0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79,
    0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79,
    0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79,
    0x79, 0x79, 0x79, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x03, 0x02, 0x53,
    0x00, 0x01, 0xcc, 0x34, 0x2a, 0x5e, 0x5b, 0x1a, 0x67, 0x73, 0xc2, 0x0e, 0x21, 0xb8, 0x22, 0x4d,
    0xf8, 0x62, 0x48, 0x18, 0x64, 0xef, 0x81, 0x0a, 0xae, 0x2e, 0x37, 0x03, 0xc8, 0x81, 0x9c, 0x23,
    0x53, 0x9d, 0xe5, 0xf5, 0xd7, 0x49, 0xbc, 0x5b, 0x7a, 0x26, 0x6c, 0x49, 0x62, 0x83, 0xce, 0x7f,
    0x03, 0x93, 0x7a, 0xe1, 0xf6, 0x16, 0xde, 0x0c, 0x15, 0xff, 0x33, 0x8c, 0xca, 0xff, 0xb0, 0x9e,
    0xaa, 0xbb, 0xe4, 0x0f, 0x5d, 0x5f, 0x55, 0x8f, 0xb9, 0x7f, 0x17, 0x31, 0xf8, 0xf7, 0xda, 0x60,
    0xa0, 0xec, 0x65, 0x79, 0xc3, 0x3e, 0xa9, 0x83, 0x12, 0xc3, 0xb6, 0x71, 0x35, 0xa6, 0x69, 0x4f,
    0xf8, 0x23, 0x05, 0xd9, 0xba, 0x5c, 0x61, 0x5f, 0xa2, 0x54, 0xd2, 0xb1, 0x83, 0x45, 0x83, 0xce,
    0xe4, 0x2d, 0x44, 0x26, 0xc8, 0x35, 0xa7, 0xa5, 0xf6, 0xc8, 0x42, 0x1c, 0x0d, 0xa3, 0xf1, 0xc7,
    0x00, 0x50, 0xf2, 0xe5, 0x17, 0xf8, 0xd0, 0xfa, 0x77, 0x8d, 0xfb, 0x82, 0x8d, 0x40, 0xc7, 0x8e,
    0x94, 0x1e, 0x1e, 0x1e};
