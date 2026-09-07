/* win_calc  entry 0x40b904  size 12 bytes */

void win_calc(int param_1,int param_2,int param_3)

{
  double dVar1;
  double dVar2;
  double dVar3;
  float fVar4;
  undefined4 *apuStack_cc [4];
  undefined4 *puStack_bc;
  undefined4 *puStack_b8;
  undefined4 *puStack_b4;
  undefined4 *puStack_b0;
  undefined4 ****ppppuStack_ac;
  undefined4 uStack_a8;
  undefined4 uStack_a4;
  undefined4 uStack_a0;
  undefined4 uStack_9c;
  undefined4 uStack_98;
  undefined4 uStack_94;
  undefined4 uStack_90;
  undefined4 uStack_8c;
  undefined4 uStack_88;
  undefined4 uStack_84;
  undefined4 uStack_80;
  undefined4 uStack_7c;
  undefined4 uStack_78;
  undefined4 uStack_74;
  undefined4 uStack_70;
  undefined4 uStack_6c;
  undefined4 uStack_68;
  undefined4 uStack_64;
  undefined4 uStack_60;
  undefined4 uStack_5c;
  undefined4 uStack_58;
  undefined4 uStack_54;
  undefined4 uStack_50;
  undefined4 uStack_4c;
  undefined4 uStack_48;
  undefined4 uStack_44;
  undefined4 uStack_40;
  undefined4 uStack_3c;
  undefined4 uStack_38;
  undefined4 uStack_34;
  undefined4 uStack_30;
  undefined4 uStack_2c;
  undefined4 uStack_28;
  undefined4 uStack_24;
  undefined4 uStack_20;
  int iStack_1c;
  float fStack_14;
  float fStack_10;
  float fStack_c;
  float fStack_8;
  undefined4 uStack_4;
  
  uStack_4 = 0;
  uStack_2c = DAT_100000d4;
  uStack_28 = DAT_100000d8;
  uStack_20 = DAT_100000e0;
  uStack_24 = DAT_100000dc;
  uStack_3c = DAT_100000e4;
  uStack_38 = DAT_100000e8;
  uStack_30 = DAT_100000f0;
  uStack_34 = DAT_100000ec;
  uStack_4c = DAT_100000f4;
  uStack_48 = DAT_100000f8;
  uStack_40 = DAT_10000100;
  uStack_44 = DAT_100000fc;
  uStack_5c = DAT_10000104;
  uStack_58 = DAT_10000108;
  uStack_50 = DAT_10000110;
  uStack_54 = DAT_1000010c;
  uStack_6c = DAT_10000114;
  uStack_68 = DAT_10000118;
  uStack_60 = DAT_10000120;
  uStack_64 = DAT_1000011c;
  uStack_7c = DAT_10000124;
  uStack_78 = DAT_10000128;
  uStack_70 = DAT_10000130;
  uStack_74 = DAT_1000012c;
  uStack_8c = DAT_10000134;
  uStack_88 = DAT_10000138;
  uStack_80 = DAT_10000140;
  uStack_84 = DAT_1000013c;
  uStack_9c = DAT_10000144;
  uStack_98 = DAT_10000148;
  uStack_90 = DAT_10000150;
  uStack_94 = DAT_1000014c;
  uStack_a8 = DAT_10000158;
  uStack_a0 = DAT_10000160;
  uStack_a4 = DAT_1000015c;
  apuStack_cc[0] = &uStack_2c;
  apuStack_cc[1] = &uStack_3c;
  apuStack_cc[2] = &uStack_4c;
  apuStack_cc[3] = &uStack_5c;
  puStack_bc = &uStack_6c;
  puStack_b8 = &uStack_7c;
  puStack_b4 = &uStack_8c;
  puStack_b0 = &uStack_9c;
  ppppuStack_ac = &ppppuStack_ac;
  if (param_2 < 9) {
    fStack_8 = (float)*apuStack_cc[param_2];
    fStack_c = (float)apuStack_cc[param_2][1];
    fStack_10 = (float)apuStack_cc[param_2][2];
    fStack_14 = (float)apuStack_cc[param_2][3];
  }
  else {
    perror("bad window type! Try again...\n");
  }
  if (DAT_1000b98c == 0) {
    iStack_1c = 0;
    if (0 < param_3) {
      do {
        dVar1 = cos(((double)iStack_1c * 12.566370614359172) / (double)param_3);
        dVar2 = cos(((double)iStack_1c * 6.283185307179586) / (double)param_3);
        dVar3 = cos(((double)iStack_1c * 18.84955592153876) / (double)param_3);
        *(float *)(param_1 + iStack_1c * 4) =
             (float)(dVar3 * (double)fStack_14 +
                    (double)fStack_8 + (double)fStack_c * dVar2 + (double)fStack_10 * dVar1);
        iStack_1c = iStack_1c + 1;
      } while (iStack_1c < param_3);
    }
  }
  else {
    iStack_1c = 0;
    if (0 < param_3) {
      do {
        dVar1 = cos(((double)iStack_1c * 12.566370614359172) / (double)param_3);
        dVar2 = cos(((double)iStack_1c * 6.283185307179586) / (double)param_3);
        dVar3 = cos(((double)iStack_1c * 18.84955592153876) / (double)param_3);
        fVar4 = sqrtf((float)(dVar3 * (double)fStack_14 +
                             (double)fStack_8 + (double)fStack_c * dVar2 + (double)fStack_10 * dVar1
                             ));
        *(float *)(param_1 + iStack_1c * 4) = fVar4;
        iStack_1c = iStack_1c + 1;
      } while (iStack_1c < param_3);
    }
  }
  return;
}

