/* init_logftbl  entry 0x411698  size 12 bytes */

void init_logftbl(void)

{
  float fVar1;
  float fVar2;
  float fVar3;
  int iStack_8;
  
  fVar1 = logf((float)DAT_1000b97c);
  fVar3 = (float)DAT_1000b97c;
  iStack_8 = 0;
  if (-1 < DAT_1000b97c) {
    do {
      fVar2 = logf((float)(iStack_8 + 1));
      (&zlogpos)[iStack_8] = fVar2 * (fVar3 / fVar1);
      iStack_8 = iStack_8 + 1;
    } while (iStack_8 <= DAT_1000b97c);
  }
  return;
}

