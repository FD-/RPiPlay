#include <stdint.h>
#include <stdio.h>

#define printf(...) (void)0;

uint8_t rol8(uint8_t x, int y);
uint32_t rol8x(uint8_t x, int y);

uint32_t weird_ror8(uint8_t input, int count)
{
   if (count == 0)
      return 0;
   return ((input >> count) & 0xff) | (input & 0xff) << (8-count);
      
}

uint32_t weird_rol8(uint8_t input, int count)
{
   if (count == 0)
      return 0;
   return ((input << count) & 0xff) | (input & 0xff) >> (8-count);      
}

uint32_t weird_rol32(uint8_t input, int count)
{
   if (count == 0)
      return 0;
   return (input << count) ^ (input >> (8 - count));
}

// I do not know why it is doing all of this, and there is still a possibility for a gremlin or two to be lurking in the background
// I DO know it is not trivial. It could be purely random garbage, of course.
void garble(unsigned char* buffer0, unsigned char* buffer1, unsigned char* buffer2, unsigned char* buffer3, unsigned char* buffer4)
{
   unsigned int tmp, tmp2, tmp3;
   unsigned int A, B, C, D, E, M, J, G, F, H, K, R, S, T, U, V, W, X, Y, Z;
   // buffer1[64] = A
   // (buffer1[99] / 3) = B
   // 0ABAAABB
   // Then we AND with a complex expression, and add 20 just for good measure
   buffer2[12] = 0x14 + (((buffer1[64] & 92) | ((buffer1[99] / 3) & 35)) & buffer4[rol8x(buffer4[(buffer1[206] % 21)],4) % 21]);
   printf("buffer2[12] = %02x\n", buffer2[12]);

   // This is a bit simpler: 2*B*B/25
   buffer1[4] = (buffer1[99] / 5) * (buffer1[99] / 5) * 2;
   printf("buffer1[4] = %02x\n", buffer1[4]);
   
   // Simpler still! 
   buffer2[34] = 0xb8;
   printf("buffer2[34] = %02x\n", buffer2[34]);

   // ...
   buffer1[153] ^= (buffer2[buffer1[203] % 35] * buffer2[buffer1[203] % 35] * buffer1[190]);
   printf("buffer1[153] = %02x\n", buffer1[153]);

   // This one looks simple, but wow was it not :(
   buffer0[3] -= (((buffer4[buffer1[205] % 21]>>1) & 80) | 0xe6440);
   printf("buffer0[3] = %02x\n", buffer0[3]);

   // This is always 0x93
   buffer0[16] = 0x93;
   printf("buffer0[16] = %02x\n", buffer0[16]);

   // This is always 0x62
   buffer0[13] = 0x62;
   printf("buffer0[13] = %02x\n", buffer0[13]);

   buffer1[33] -= (buffer4[buffer1[36] % 21] & 0xf6);
   printf("buffer1[33] = %02x\n", buffer1[33]);

   // This is always 7
   tmp2 = buffer2[buffer1[67] % 35];
   buffer2[12] = 0x07;
   printf("buffer2[12] = %02x\n", buffer2[12]);

   // This is pretty easy!
   tmp = buffer0[buffer1[181] % 20];
   buffer1[2] -= 3136;
   printf("buffer1[2] = %02x\n", buffer1[2]);
   
   buffer0[19] = buffer4[buffer1[58] % 21];
   printf("buffer0[19] = %02x\n", buffer0[19]);
   
   buffer3[0] = 92 - buffer2[buffer1[32] % 35];
   printf("buffer3[0] = %02x\n", buffer3[0]);

   buffer3[4] = buffer2[buffer1[15] % 35] + 0x9e;
   printf("buffer3[4] = %02x\n", buffer3[4]);
   
   buffer1[34] += (buffer4[((buffer2[buffer1[15] % 35] + 0x9e) & 0xff) % 21] / 5);
   printf("buffer1[34] = %02x\n", buffer1[34]);

   buffer0[19] += 0xfffffee6 - ((buffer0[buffer3[4] % 20]>>1) & 102);
   printf("buffer0[19] = %02x\n", buffer0[19]);

   // This LOOKS like it should be a rol8x, but it just doesnt work out because if the shift amount is 0, then the output is 0 too :(
   // FIXME: Switch to weird_ror8
   buffer1[15] = (3*(((buffer1[72] >> (buffer4[buffer1[190] % 21] & 7)) ^ (buffer1[72] << ((7 - (buffer4[buffer1[190] % 21]-1)&7)))) - (3*buffer4[buffer1[126] % 21]))) ^ buffer1[15];
   printf("buffer1[15] = %02x\n", buffer1[15]);

   buffer0[15] ^= buffer2[buffer1[181] % 35] * buffer2[buffer1[181] % 35] * buffer2[buffer1[181] % 35];
   printf("buffer0[15] = %02x\n", buffer0[15]);

   buffer2[4] ^= buffer1[202]/3;
   printf("buffer2[4] = %02x\n", buffer2[4]);

   // This could probably be quite a bit simpler.
   A = 92 - buffer0[buffer3[0] % 20];
   E = (A & 0xc6) | (~buffer1[105] & 0xc6) | (A & (~buffer1[105]));
   buffer2[1] += (E*E*E);
   printf("buffer2[1] = %02x\n", buffer2[1]);

   buffer0[19] ^= ((224 | (buffer4[buffer1[92] % 21] & 27)) * buffer2[buffer1[41] % 35]) / 3;
   printf("buffer0[19] = %02x\n", buffer0[19]);

   buffer1[140] += weird_ror8(92, buffer1[5] & 7);
   printf("buffer1[140] = %02x\n", buffer1[140]);

   // Is this as simple as it could be?
   buffer2[12] += ((((~buffer1[4]) ^ buffer2[buffer1[12] % 35]) | buffer1[182]) & 192) | (((~buffer1[4]) ^ buffer2[buffer1[12] % 35]) & buffer1[182]);
   printf("buffer2[12] = %02x\n", buffer2[12]);
  
   buffer1[36] += 125;
   printf("buffer1[36] = %02x\n", buffer1[36]);

   buffer1[124] = rol8x((((74 & buffer1[138]) | ((74 | buffer1[138]) & buffer0[15])) & buffer0[buffer1[43] % 20]) | (((74 & buffer1[138]) | ((74 | buffer1[138]) & buffer0[15]) | buffer0[buffer1[43] % 20]) & 95), 4);
   printf("buffer1[124] = %02x\n", buffer1[124]);

   buffer3[8] = ((((buffer0[buffer3[4] % 20] & 95)) & ((buffer4[buffer1[68] % 21] & 46) << 1)) | 16) ^ 92;
   printf("buffer3[8] = %02x\n", buffer3[8]);   

   A = buffer1[177] + buffer4[buffer1[79] % 21];
   D = (((A >> 1) | ((3 * buffer1[148]) / 5)) & buffer2[1]) | ((A >> 1) & ((3 * buffer1[148])/5));
   buffer3[12] =  ((-34 - D));
   printf("buffer3[12] = %02x\n", buffer3[12]);   

   A = 8 - ((buffer2[22] & 7));     // FIXME: buffer2[22] = 74, so A is always 6 and B^C is just ror8(buffer1[33], 6)
   B = (buffer1[33] >> (A & 7));
   C = buffer1[33] << (buffer2[22] & 7);   
   buffer2[16] += ((buffer2[buffer3[0] % 35] & 159) | buffer0[buffer3[4] % 20] | 8) - ((B^C) | 128);
   printf("buffer2[16] = %02x\n", buffer2[16]);   

   // This one was very easy so I just skipped ahead and did it
   buffer0[14] ^= buffer2[buffer3[12] % 35];
   printf("buffer0[14] = %02x\n", buffer0[14]);

   // Monster goes here
   A = weird_rol8(buffer4[buffer0[buffer1[201] % 20] % 21], ((buffer2[buffer1[112] % 35] << 1) & 7));
   D = (buffer0[buffer1[208] % 20] & 131) | (buffer0[buffer1[164] % 20] & 124);
   buffer1[19] += (A & (D/5)) | ((A | (D/5)) & 37);
   printf("buffer1[19] = %02x\n", buffer1[19]);

   buffer2[8] = weird_ror8(140, ((buffer4[buffer1[45] % 21] + 92) * (buffer4[buffer1[45] % 21] + 92)) & 7);
   printf("buffer2[8] = %02x\n", buffer2[8]);

   buffer1[190] = 56;
   printf("buffer1[190] = %02x\n", buffer1[190]);

   buffer2[8] ^= buffer3[0];
   printf("buffer2[8] = %02x\n", buffer2[8]);

   buffer1[53] = ~((buffer0[buffer1[83] % 20] | 204)/5);
   printf("buffer1[53] = %02x\n", buffer1[53]);

   buffer0[13] += buffer0[buffer1[41] % 20];
   printf("buffer0[13] = %02x\n", buffer0[13]);

   buffer0[10] = ((buffer2[buffer3[0] % 35] & buffer1[2]) | ((buffer2[buffer3[0] % 35] | buffer1[2]) & buffer3[12])) / 15;
   printf("buffer0[10] = %02x\n", buffer0[10]);

   A = (((56 | (buffer4[buffer1[2] % 21] & 68)) | buffer2[buffer3[8] % 35]) & 42) | (((buffer4[buffer1[2] % 21] & 68) | 56) & buffer2[buffer3[8] % 35]);
   buffer3[16] = (A*A) + 110;
   printf("buffer3[16] = %02x\n", buffer3[16]);

   buffer3[20] = 202 - buffer3[16];
   printf("buffer3[20] = %02x\n", buffer3[20]);

   buffer3[24] = buffer1[151];
   printf("buffer3[24] = %02x\n", buffer3[24]);

   buffer2[13] ^= buffer4[buffer3[0] % 21];
   printf("buffer2[13] = %02x\n", buffer2[13]);

   B = ((buffer2[buffer1[179] % 35] - 38) & 177) | (buffer3[12] & 177);
   C = ((buffer2[buffer1[179] % 35] - 38)) & buffer3[12];
   buffer3[28] = 30 + ((B | C) * (B | C));
   printf("buffer3[28] = %02x\n", buffer3[28]);

   buffer3[32] = buffer3[28] + 62;
   printf("buffer3[32] = %02x\n", buffer3[32]);

   // eek
   A = ((buffer3[20] + (buffer3[0] & 74)) | ~buffer4[buffer3[0] % 21]) & 121;
   B = ((buffer3[20] + (buffer3[0] & 74)) & ~buffer4[buffer3[0] % 21]);
   tmp3 = (A|B);
   C = ((((A|B) ^ 0xffffffa6) | buffer3[0]) & 4) | (((A|B) ^ 0xffffffa6) & buffer3[0]);
   buffer1[47] = (buffer2[buffer1[89] % 35] + C) ^ buffer1[47];   
   printf("buffer1[47] = %02x\n", buffer1[47]);

   buffer3[36] = ((rol8((tmp & 179) + 68, 2) & buffer0[3]) | (tmp2 & ~buffer0[3])) - 15;
   printf("buffer3[36] = %02x\n", buffer3[36]);
  
   buffer1[123] ^= 221;
   printf("buffer1[123] = %02x\n", buffer1[123]);

   A = ((buffer4[buffer3[0] % 21]) / 3) - buffer2[buffer3[4] % 35];
   C = (((buffer3[0] & 163) + 92) & 246) | (buffer3[0] & 92);
   E = ((C | buffer3[24]) & 54) | (C & buffer3[24]);
   buffer3[40] = A - E;
   printf("buffer3[40] = %02x\n", buffer3[40]);

   buffer3[44] = tmp3 ^ 81 ^ (((buffer3[0] >> 1) & 101) + 26);
   printf("buffer3[44] = %02x\n", buffer3[44]);

   buffer3[48] = buffer2[buffer3[4] % 35] & 27;
   printf("buffer3[48] = %02x\n", buffer3[48]);
   buffer3[52] = 27;
   printf("buffer3[52] = %02x\n", buffer3[52]);
   buffer3[56] = 199;
   printf("buffer3[56] = %02x\n", buffer3[56]);

   // caffeine 
   buffer3[64] = buffer3[4] + (((((((buffer3[40] | buffer3[24]) & 177) | (buffer3[40] & buffer3[24])) & ((((buffer4[buffer3[0] % 20] & 177) | 176)) | ((buffer4[buffer3[0] % 21]) & ~3))) | ((((buffer3[40] & buffer3[24]) | ((buffer3[40] | buffer3[24]) & 177)) & 199) | ((((buffer4[buffer3[0] % 21] & 1) + 176) | (buffer4[buffer3[0] % 21] & ~3)) & buffer3[56]))) & (~buffer3[52])) | buffer3[48]);
   printf("buffer3[64] = %02x (want E7)\n", buffer3[64]);

   buffer2[33] ^= buffer1[26];
   printf("buffer2[33] = %02x\n", buffer2[33]);

   buffer1[106] ^= buffer3[20] ^ 133;
   printf("buffer1[106] = %02x\n", buffer1[106]);
   
   buffer2[30] = ((buffer3[64] / 3) - (275 | (buffer3[0] & 247))) ^ buffer0[buffer1[122] % 20];
   printf("buffer2[130] = %02x\n", buffer2[30]);

   buffer1[22] = (buffer2[buffer1[90] % 35] & 95) | 68;
   printf("buffer1[22] = %02x\n", buffer1[22]);

   A = (buffer4[buffer3[36] % 21] & 184) | (buffer2[buffer3[44] % 35] & ~184);
   buffer2[18] += ((A*A*A) >> 1);
   printf("buffer2[18] = %02x\n", buffer2[18]);

   buffer2[5] -= buffer4[buffer1[92] % 21];
   printf("buffer2[5] = %02x\n", buffer2[5]);

   A = (((buffer1[41] & ~24)|(buffer2[buffer1[183] % 35] & 24)) & (buffer3[16] + 53)) | (buffer3[20] & buffer2[buffer3[20] % 35]);
   B = (buffer1[17] & (~buffer3[44])) | (buffer0[buffer1[59] % 20] & buffer3[44]);
   buffer2[18] ^= (A*B);
   printf("buffer2[18] = %02x\n", buffer2[18]);


   A = weird_ror8(buffer1[11], buffer2[buffer1[28] % 35] & 7) & 7;
   B = (((buffer0[buffer1[93] % 20] & ~buffer0[14]) | (buffer0[14] & 150)) & ~28) | (buffer1[7] & 28);
   buffer2[22] = (((((B | weird_rol8(buffer2[buffer3[0] % 35], A)) & buffer2[33]) | (B & weird_rol8(buffer2[buffer3[0] % 35], A))) + 74) & 0xff);
   printf("buffer2[22] = %02x\n", buffer2[22]);

   A = buffer4[(buffer0[buffer1[39] % 20] ^ 217) % 21]; // X5
   buffer0[15] -= ((((buffer3[20] | buffer3[0]) & 214) | (buffer3[20] & buffer3[0])) & A) | ((((buffer3[20] | buffer3[0]) & 214) | (buffer3[20] & buffer3[0]) | A) & buffer3[32]);
   printf("buffer0[15] = %02x\n", buffer0[15]);

   // We need to save T here, and boy is it complicated to calculate!
   B = (((buffer2[buffer1[57] % 35] & buffer0[buffer3[64] % 20]) | ((buffer0[buffer3[64] % 20] | buffer2[buffer1[57] % 35]) & 95) | (buffer3[64] & 45) | 82) & 32);
   C = ((buffer2[buffer1[57] % 35] & buffer0[buffer3[64] % 20]) | ((buffer2[buffer1[57] % 35] | buffer0[buffer3[64] % 20]) & 95)) & ((buffer3[64] & 45) | 82);
   D = ((((buffer3[0]/3) - (buffer3[64]|buffer1[22]))) ^ (buffer3[28] + 62) ^ ((B|C)));
   T = buffer0[(D & 0xff) % 20];

   buffer3[68] = (buffer0[buffer1[99] % 20] * buffer0[buffer1[99] % 20] * buffer0[buffer1[99] % 20] * buffer0[buffer1[99] % 20]) | buffer2[buffer3[64] % 35];
   printf("buffer3[68] = %02x\n", buffer3[68]);

   U = buffer0[buffer1[50] % 20]; // this is also v100
   W = buffer2[buffer1[138] % 35]; 
   X = buffer4[buffer1[39] % 21];
   Y = buffer0[buffer1[4] % 20]; // this is also v120
   Z = buffer4[buffer1[202] % 21]; // also v124
   V = buffer0[buffer1[151] % 20];
   S = buffer2[buffer1[14] % 35];
   R = buffer0[buffer1[145] % 20];
   
   A = (buffer2[buffer3[68] % 35] & buffer0[buffer1[209] % 20]) | ((buffer2[buffer3[68] % 35] | buffer0[buffer1[209] % 20]) & 24);
   B = weird_rol8(buffer4[buffer1[127] % 21], buffer2[buffer3[68] % 35] & 7);
   C = (A & buffer0[10]) | (B & ~buffer0[10]);
   D = 7 ^ (buffer4[buffer2[buffer3[36] % 35] % 21] << 1);
   buffer3[72] = (C & 71) | (D & ~71);
   printf("buffer3[72] = %02x\n", buffer3[72]);

   buffer2[2] += (((buffer0[buffer3[20] % 20] << 1) & 159) | (buffer4[buffer1[190] % 21] & ~159)) & ((((buffer4[buffer3[64] % 21] & 110) | (buffer0[buffer1[25] % 20] & ~110)) & ~150) | (buffer1[25] & 150));
   printf("buffer2[2] = %02x\n", buffer2[2]);
   
   buffer2[14] -= ((buffer2[buffer3[20] % 35] & (buffer3[72] ^ buffer2[buffer1[100] % 35])) & ~34) | (buffer1[97] & 34);
   printf("buffer2[14] = %02x\n", buffer2[14]);

   buffer0[17] = 115;
   printf("buffer0[17] = %02x\n", buffer0[17]);

   buffer1[23] ^= ((((((buffer4[buffer1[17] % 21] | buffer0[buffer3[20] % 20]) & buffer3[72]) | (buffer4[buffer1[17] % 21] & buffer0[buffer3[20] % 20])) & (buffer1[50]/3)) |
                    ((((buffer4[buffer1[17] % 21] | buffer0[buffer3[20] % 20]) & buffer3[72]) | (buffer4[buffer1[17] % 21] & buffer0[buffer3[20] % 20]) | (buffer1[50] / 3)) & 246)) << 1);
   printf("buffer1[23] = %02x\n", buffer1[23]);

   buffer0[13] = ((((((buffer0[buffer3[40] % 20] | buffer1[10]) & 82) | (buffer0[buffer3[40] % 20] & buffer1[10])) & 209) |
                   ((buffer0[buffer1[39] % 20] << 1) & 46)) >> 1);
   printf("buffer0[13] = %02x\n", buffer0[13]);

   buffer2[33] -= buffer1[113] & 9;
   printf("buffer2[33] = %02x\n", buffer2[33]);

   buffer2[28] -= ((((2 | (buffer1[110] & 222)) >> 1) & ~223) | (buffer3[20] & 223));
   printf("buffer2[28] = %02x\n", buffer2[28]);

   J = weird_rol8((V | Z), (U & 7));                   // OK
   A = (buffer2[16] & T) | (W & (~buffer2[16]));
   B = (buffer1[33] & 17) | (X & ~17);
   E = ((Y | ((A+B) / 5)) & 147) |
      (Y & ((A+B) / 5));                                   // OK
   M = (buffer3[40] & buffer4[((buffer3[8] + J + E) & 0xff) % 21]) |
      ((buffer3[40] | buffer4[((buffer3[8] + J + E) & 0xff) % 21]) & buffer2[23]);
   
   buffer0[15] = (((buffer4[buffer3[20] % 21] - 48) & (~buffer1[184])) | ((buffer4[buffer3[20] % 21] - 48) & 189) | (189 & ~buffer1[184])) & (M*M*M);
   printf("buffer0[15] = %02x\n", buffer0[15]);

   buffer2[22] += buffer1[183];
   printf("buffer2[22] = %02x\n", buffer2[22]);

   buffer3[76] = (3 * buffer4[buffer1[1] % 21]) ^ buffer3[0];
   printf("buffer3[76] = %02x\n", buffer3[76]);

   A = buffer2[((buffer3[8] + (J + E)) & 0xff) % 35];
   F = (((buffer4[buffer1[178] % 21] & A) | ((buffer4[buffer1[178] % 21] | A) & 209)) * buffer0[buffer1[13] % 20]) * (buffer4[buffer1[26] % 21] >> 1);
   G = (F + 0x733ffff9) * 198 - (((F + 0x733ffff9) * 396 + 212) & 212) + 85;
   buffer3[80] = buffer3[36] + (G ^ 148) + ((G ^ 107) << 1) - 127;
   printf("buffer3[80] = %02x\n", buffer3[80]);

   buffer3[84] = ((buffer2[buffer3[64] % 35]) & 245) | (buffer2[buffer3[20] % 35] & 10);
   printf("buffer3[84] = %02x\n", buffer3[84]);

   A = buffer0[buffer3[68] % 20] | 81;
   buffer2[18] -= ((A*A*A) & ~buffer0[15]) | ((buffer3[80] / 15) & buffer0[15]);
   printf("buffer2[18] = %02x\n", buffer2[18]);
   
   buffer3[88] = buffer3[8] + J + E - buffer0[buffer1[160] % 20] + (buffer4[buffer0[((buffer3[8] + J + E) & 255) % 20] % 21] / 3);
   printf("buffer3[88] = %02x\n", buffer3[88]);

   B = ((R ^ buffer3[72]) & ~198) | ((S * S) & 198);
   F = (buffer4[buffer1[69] % 21] & buffer1[172]) | ((buffer4[buffer1[69] % 21] | buffer1[172] ) & ((buffer3[12] - B) + 77));
   buffer0[16] = 147 - ((buffer3[72] & ((F & 251) | 1)) | (((F & 250) | buffer3[72]) & 198));
   printf("buffer0[16] = %02x\n", buffer0[16]);

   C = (buffer4[buffer1[168] % 21] & buffer0[buffer1[29] % 20] & 7) | ((buffer4[buffer1[168] % 21] | buffer0[buffer1[29] % 20]) & 6);
   F = (buffer4[buffer1[155] % 21] & buffer1[105]) | ((buffer4[buffer1[155] % 21] | buffer1[105]) & 141);
   buffer0[3] -= buffer4[weird_rol32(F, C) % 21];
   printf("buffer0[3] = %02x\n", buffer0[3]);

   buffer1[5] = weird_ror8(buffer0[12], ((buffer0[buffer1[61] % 20] / 5) & 7)) ^ (((~buffer2[buffer3[84] % 35]) & 0xffffffff) / 5);
   printf("buffer1[5] = %02x\n", buffer1[5]);

   buffer1[198] += buffer1[3];
   printf("buffer1[198] = %02x\n", buffer1[198]);

   A = (162 | buffer2[buffer3[64] % 35]);
   buffer1[164] += ((A*A)/5);
   printf("buffer1[164] = %02x\n", buffer1[164]);

   G = weird_ror8(139, (buffer3[80] & 7));
   C = ((buffer4[buffer3[64] % 21] * buffer4[buffer3[64] % 21] * buffer4[buffer3[64] % 21]) & 95) | (buffer0[buffer3[40] % 20] & ~95);
   buffer3[92] = (G & 12) | (buffer0[buffer3[20] % 20] & 12) | (G & buffer0[buffer3[20] % 20]) | C;
   printf("buffer3[92] = %02x\n", buffer3[92]);

   buffer2[12] += ((buffer1[103] & 32) | (buffer3[92] & ((buffer1[103] | 60))) | 16)/3;
   printf("buffer2[12] = %02x\n", buffer2[12]);

   buffer3[96] = buffer1[143];
   printf("buffer3[96] = %02x\n", buffer3[96]);

   buffer3[100] = 27;
   printf("buffer3[100] = %02x\n", buffer3[100]);

   buffer3[104] = (((buffer3[40] & ~buffer2[8]) | (buffer1[35] & buffer2[8])) & buffer3[64]) ^ 119;
   printf("buffer3[104] = %02x\n", buffer3[104]);

   buffer3[108] = 238 & ((((buffer3[40] & ~buffer2[8]) | (buffer1[35] & buffer2[8])) & buffer3[64]) << 1);
   printf("buffer3[108] = %02x\n", buffer3[108]);

   buffer3[112] = (~buffer3[64] & (buffer3[84] / 3)) ^ 49;
   printf("buffer3[112] = %02x\n", buffer3[112]);

   buffer3[116] = 98 & ((~buffer3[64] & (buffer3[84] / 3)) << 1);
   printf("buffer3[116] = %02x\n", buffer3[116]);

   // finale
   A = (buffer1[35] & buffer2[8]) | (buffer3[40] & ~buffer2[8]);
   B = (A & buffer3[64]) | (((buffer3[84] / 3) & ~buffer3[64]));
   buffer1[143] = buffer3[96] - ((B & (86 + ((buffer1[172] & 64) >> 1))) | (((((buffer1[172] & 65) >> 1) ^ 86) | ((~buffer3[64] & (buffer3[84] / 3)) | (((buffer3[40] & ~buffer2[8]) | (buffer1[35] & buffer2[8])) & buffer3[64]))) & buffer3[100]));
   printf("buffer1[143] = %02x\n", buffer1[143]);

   buffer2[29] = 162;
   printf("buffer2[29] = %02x\n", buffer2[29]);

   A = ((((buffer4[buffer3[88] % 21]) & 160) | (buffer0[buffer1[125] % 20] & 95)) >> 1);
   B = buffer2[buffer1[149] % 35] ^ (buffer1[43] * buffer1[43]);
   buffer0[15] += (B&A) | ((A|B) & 115);
   printf("buffer0[15] = %02x\n", buffer0[15]);

   buffer3[120] = buffer3[64] - buffer0[buffer3[40] % 20];
   printf("buffer3[120] = %02x\n", buffer3[120]);

   buffer1[95] = buffer4[buffer3[20] % 21];
   printf("buffer1[95] = %02x\n", buffer1[95]);

   A = weird_ror8(buffer2[buffer3[80] % 35], (buffer2[buffer1[17] % 35] * buffer2[buffer1[17] % 35] * buffer2[buffer1[17] % 35]) & 7);
   buffer0[7] -= (A*A);
   printf("buffer0[7] = %02x\n", buffer0[7]);

   buffer2[8] = buffer2[8] - buffer1[184] + (buffer4[buffer1[202] % 21] * buffer4[buffer1[202] % 21] * buffer4[buffer1[202] % 21]);
   printf("buffer2[8] = %02x\n", buffer2[8]);

   buffer0[16] = (buffer2[buffer1[102] % 35] << 1) & 132;
   printf("buffer0[16] = %02x\n", buffer0[16]);

   buffer3[124] = (buffer4[buffer3[40] % 21] >> 1) ^ buffer3[68];
   printf("buffer3[124] = %02x\n", buffer3[124]);
   
   buffer0[7] -= (buffer0[buffer1[191] % 20] - (((buffer4[buffer1[80] % 21] << 1) & ~177) | (buffer4[buffer4[buffer3[88] % 21] % 21] & 177)));   
   printf("buffer0[7] = %02x\n", buffer0[7]);

   buffer0[6] = buffer0[buffer1[119] % 20];
   printf("buffer0[6] = %02x\n", buffer0[6]);

   A = (buffer4[buffer1[190] % 21] & ~209) | (buffer1[118] & 209);
   B = buffer0[buffer3[120] % 20] * buffer0[buffer3[120] % 20];
   buffer0[12] = (buffer0[buffer3[84] % 20] ^ (buffer2[buffer1[71] % 35] + buffer2[buffer1[15] % 35])) & ((A & B) | ((A | B) & 27));
   printf("buffer0[12] = %02x\n", buffer0[12]);

   B = (buffer1[32] & buffer2[buffer3[88] % 35]) | ((buffer1[32] | buffer2[buffer3[88] % 35]) & 23);
   D = (((buffer4[buffer1[57] % 21] * 231) & 169) | (B & 86));
   F = (((buffer0[buffer1[82] % 20] & ~29) | (buffer4[buffer3[124] % 21] & 29)) & 190) | (buffer4[(D/5) % 21] & ~190);
   H = buffer0[buffer3[40] % 20] * buffer0[buffer3[40] % 20] * buffer0[buffer3[40] % 20];
   K = (H & buffer1[82]) | (H & 92) | (buffer1[82] & 92);
   buffer3[128] = ((F & K) | ((F | K) & 192)) ^ (D/5);
   printf("buffer3[128] = %02x\n", buffer3[128]);

   buffer2[25] ^= ((buffer0[buffer3[120] % 20] << 1) * buffer1[5]) - (weird_rol8(buffer3[76], (buffer4[buffer3[124] % 21] & 7)) & (buffer3[20] + 110));
   printf("buffer2[25] = %02x\n", buffer2[25]);
   //exit(0);
   
}
