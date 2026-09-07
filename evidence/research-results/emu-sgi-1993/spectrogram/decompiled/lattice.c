/* lattice  entry 0x4171c0  size 12 bytes */

float lattice(float param_1)

{
  float fStack_8;
  int iStack_4;
  
  iStack_4 = 0xb;
  fStack_8 = param_1;
  do {
    fStack_8 = fStack_8 + (float)(&DAT_120527f4)[iStack_4] * (&DAT_120528d0)[iStack_4];
    *(float *)(iStack_4 * 4 + 0x120528d4) =
         (&DAT_120528d0)[iStack_4] - (float)(&DAT_120527f4)[iStack_4] * fStack_8;
    iStack_4 = iStack_4 + -1;
  } while (-1 < iStack_4);
  DAT_120528d0 = fStack_8;
  return fStack_8;
}

