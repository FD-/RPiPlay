#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define printf(...) (void)0;

void garble(unsigned char*, unsigned char*, unsigned char*, unsigned char*, unsigned char*);

unsigned char rol8(unsigned char input, int count)
{
   return ((input << count) & 0xff) | (input & 0xff) >> (8-count);
}

uint32_t rol8x(unsigned char input, int count)
{
   return ((input << count)) | (input) >> (8-count);
}


void sap_hash(unsigned char* blockIn, unsigned char* keyOut)
{
   uint32_t* block_words = (uint32_t*)blockIn;
   uint32_t* out_words = (uint32_t*)keyOut;   
   unsigned char buffer0[20] = {0x96, 0x5F, 0xC6, 0x53, 0xF8, 0x46, 0xCC, 0x18, 0xDF, 0xBE, 0xB2, 0xF8, 0x38, 0xD7, 0xEC, 0x22, 0x03, 0xD1, 0x20, 0x8F};
   unsigned char buffer1[210];
   unsigned char buffer2[35] = {0x43, 0x54, 0x62, 0x7A, 0x18, 0xC3, 0xD6, 0xB3, 0x9A, 0x56, 0xF6, 0x1C, 0x14, 0x3F, 0x0C, 0x1D, 0x3B, 0x36, 0x83, 0xB1, 0x39, 0x51, 0x4A, 0xAA, 0x09, 0x3E, 0xFE, 0x44, 0xAF, 0xDE, 0xC3, 0x20, 0x9D, 0x42, 0x3A}; 
   unsigned char buffer3[132];
   unsigned char buffer4[21] = {0xED, 0x25, 0xD1, 0xBB, 0xBC, 0x27, 0x9F, 0x02, 0xA2, 0xA9, 0x11, 0x00, 0x0C, 0xB3, 0x52, 0xC0, 0xBD, 0xE3, 0x1B, 0x49, 0xC7};
   int i0_index[11] = {18, 22, 23, 0, 5, 19, 32, 31, 10, 21, 30};
   uint8_t w,x,y,z;
   int i, j;
   
   // Load the input into the buffer
   for (i = 0; i < 210; i++)
   {
      // We need to swap the byte order around so it is the right endianness      
      uint32_t in_word = block_words[((i % 64)>>2)];
      uint32_t in_byte = (in_word >> ((3-(i % 4)) << 3)) & 0xff;
      buffer1[i] = in_byte;
   }
   // Next a scrambling
   for (i = 0; i < 840; i++)
   {
      // We have to do unsigned, 32-bit modulo, or we get the wrong indices
      x = buffer1[((i-155) & 0xffffffff) % 210];
      y = buffer1[((i-57) & 0xffffffff) % 210];
      z = buffer1[((i-13) & 0xffffffff) % 210];
      w = buffer1[(i & 0xffffffff) % 210];
      buffer1[i % 210] = (rol8(y, 5) + (rol8(z, 3) ^ w) - rol8(x,7)) & 0xff;
   }
   printf("Garbling...\n");
   // I have no idea what this is doing (yet), but it gives the right output
   garble(buffer0, buffer1, buffer2, buffer3, buffer4);

   // Fill the output with 0xE1
   for (i = 0; i < 16; i++)
     keyOut[i] = 0xE1;
   
   // Now we use all the buffers we have calculated to grind out the output. First buffer3
   for (i = 0; i < 11; i++)
   {
      // Note that this is addition (mod 255) and not XOR
      // Also note that we only use certain indices
      // And that index 3 is hard-coded to be 0x3d (Maybe we can hack this up by changing buffer3[0] to be 0xdc?
      if (i == 3)
         keyOut[i] = 0x3d;
      else
         keyOut[i] = ((keyOut[i] + buffer3[i0_index[i] * 4]) & 0xff);
   }
   
   // Then buffer0
   for (i = 0; i < 20; i++)
      keyOut[i % 16] ^= buffer0[i];
   
   // Then buffer2
   for (i = 0; i < 35; i++)
      keyOut[i % 16] ^= buffer2[i];

   // Do buffer1
   for (i = 0; i < 210; i++)
      keyOut[(i % 16)] ^= buffer1[i];


   // Now we do a kind of reverse-scramble
   for (j = 0; j < 16; j++)
   {
      for (i = 0; i < 16; i++)
      {
         x = keyOut[((i-7) & 0xffffffff) % 16];
         y = keyOut[i % 16];
         z = keyOut[((i-37) & 0xffffffff) % 16];
         w = keyOut[((i-177) & 0xffffffff) % 16];
         keyOut[i] = rol8(x, 1) ^ y ^ rol8(z, 6) ^ rol8(w, 5);
      }
   }
}
