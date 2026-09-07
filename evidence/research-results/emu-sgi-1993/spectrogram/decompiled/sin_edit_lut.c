/* sin_edit_lut  entry 0x4106cc  size 12 bytes */

void sin_edit_lut(void)

{
  float fVar1;
  float fVar2;
  float fVar3;
  float fVar4;
  float fVar5;
  float fVar6;
  undefined4 uStack_10;
  undefined4 uStack_c;
  undefined4 uStack_8;
  undefined4 uStack_4;
  
  uStack_8 = DAT_1000acc8 * 6.2831855;
  uStack_c = DAT_1000accc * 6.2831855;
  uStack_10 = DAT_1000acd0 * 6.2831855;
  fVar4 = DAT_1000acd4 * 0.1978956;
  fVar6 = DAT_1000acd8 * 0.1978956;
  fVar5 = DAT_1000acdc * 0.1978956;
  uStack_4 = 1;
  do {
    fVar1 = sinf(uStack_8);
    fVar2 = sinf(uStack_c);
    fVar3 = sinf(uStack_10);
    mapcolor(uStack_4,(int)(((fVar1 + 1.0) / 2.0) * 255.0),(int)(((fVar2 + 1.0) / 2.0) * 255.0),
             (int)(((fVar3 + 1.0) / 2.0) * 255.0));
    uStack_8 = uStack_8 + fVar4;
    uStack_c = uStack_c + fVar6;
    uStack_10 = uStack_10 + fVar5;
    uStack_4 = uStack_4 + 1;
  } while (uStack_4 < 0xff);
  return;
}

