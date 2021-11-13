#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#define printf(...) (void)0;

int shift[] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
               5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
               4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
               6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

uint32_t F(uint32_t B, uint32_t C, uint32_t D)
{
   return (B & C) | (~B & D);
}

uint32_t G(uint32_t B, uint32_t C, uint32_t D)
{
   return (B & D) | (C & ~D);
}

uint32_t H(uint32_t B, uint32_t C, uint32_t D)
{
   return B ^ C ^ D;
}

uint32_t I(uint32_t B, uint32_t C, uint32_t D)
{
   return C ^ (B | ~D);
}


uint32_t rol(uint32_t input, int count)
{
   return ((input << count) & 0xffffffff) | (input & 0xffffffff) >> (32-count);
}

void swap(uint32_t* a, uint32_t* b)
{
   printf("%08x <-> %08x\n", *a, *b);
   uint32_t c = *a;
   *a = *b;
   *b = c;
}

void modified_md5(unsigned char* originalblockIn, unsigned char* keyIn, unsigned char* keyOut)
{
   unsigned char blockIn[64];
   uint32_t* block_words = (uint32_t*)blockIn;
   uint32_t* key_words = (uint32_t*)keyIn;
   uint32_t* out_words = (uint32_t*)keyOut;
   uint32_t A, B, C, D, Z, tmp;
   int i;
   
   memcpy(blockIn, originalblockIn, 64);

   // Each cycle does something like this:
   A = key_words[0];
   B = key_words[1];
   C = key_words[2];
   D = key_words[3];
   for (i = 0; i < 64; i++)
   {
      uint32_t input;
      int j;
      if (i < 16)
         j = i;
      else if (i < 32)
         j = (5*i + 1) % 16;
      else if (i < 48)
         j = (3*i + 5) % 16;
      else if (i < 64)
         j = 7*i % 16;

      input = blockIn[4*j] << 24 | blockIn[4*j+1] << 16 | blockIn[4*j+2] << 8 | blockIn[4*j+3];
      printf("Key = %08x\n", A);
      Z = A + input + (int)(long long)((1LL << 32) * fabs(sin(i + 1)));
      if (i < 16)
         Z = rol(Z + F(B,C,D), shift[i]);
      else if (i < 32)
         Z = rol(Z + G(B,C,D), shift[i]);
      else if (i < 48)
         Z = rol(Z + H(B,C,D), shift[i]);
      else if (i < 64)
         Z = rol(Z + I(B,C,D), shift[i]);
      if (i == 63)
         printf("Ror is %08x\n", Z);
      printf("Output of round %d: %08X + %08X = %08X (shift %d, constant %08X)\n", i, Z, B, Z+B, shift[i], (int)(long long)((1LL << 32) * fabs(sin(i + 1))));
      Z = Z + B;
      tmp = D;
      D = C;
      C = B;
      B = Z;
      A = tmp;
      if (i == 31)
      {
         // swapsies
         swap(&block_words[A & 15], &block_words[B & 15]);
         swap(&block_words[C & 15], &block_words[D & 15]);
         swap(&block_words[(A & (15<<4))>>4], &block_words[(B & (15<<4))>>4]);
         swap(&block_words[(A & (15<<8))>>8], &block_words[(B & (15<<8))>>8]);
         swap(&block_words[(A & (15<<12))>>12], &block_words[(B & (15<<12))>>12]);
      }
   }
   printf("%08X %08X %08X %08X\n", A, B, C, D);
   // Now we can actually compute the output
   printf("Out:\n");
   printf("%08x + %08x = %08x\n", key_words[0], A, key_words[0] + A);
   printf("%08x + %08x = %08x\n", key_words[1], B, key_words[1] + B);
   printf("%08x + %08x = %08x\n", key_words[2], C, key_words[2] + C);
   printf("%08x + %08x = %08x\n", key_words[3], D, key_words[3] + D);
   out_words[0] = key_words[0] + A;
   out_words[1] = key_words[1] + B;
   out_words[2] = key_words[2] + C;
   out_words[3] = key_words[3] + D;
   
}
