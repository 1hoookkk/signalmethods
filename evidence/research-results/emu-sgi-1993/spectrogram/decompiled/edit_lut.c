/* edit_lut  entry 0x41056c  size 12 bytes */

void edit_lut(void)

{
  float fVar1;
  float fVar2;
  float fVar3;
  undefined4 uStack_10;
  undefined4 uStack_c;
  undefined4 uStack_8;
  undefined4 uStack_4;
  
  uStack_8 = DAT_1000acc8;
  uStack_c = DAT_1000accc;
  uStack_10 = DAT_1000acd0;
  fVar3 = DAT_1000acd4 - DAT_1000acc8;
  fVar2 = DAT_1000acd8 - DAT_1000accc;
  fVar1 = DAT_1000acdc - DAT_1000acd0;
  uStack_4 = 1;
  do {
    mapcolor(uStack_4,(int)(uStack_8 * 255.0),(int)(uStack_c * 255.0),(int)(uStack_10 * 255.0));
    uStack_8 = uStack_8 + fVar3 / 254.0;
    uStack_c = uStack_c + fVar2 / 254.0;
    uStack_10 = uStack_10 + fVar1 / 254.0;
    uStack_4 = uStack_4 + 1;
  } while (uStack_4 < 0xff);
  return;
}

