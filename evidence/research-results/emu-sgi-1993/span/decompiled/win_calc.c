/* win_calc  entry 0x404298  size 12 bytes */

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void win_calc(float *param_1,int param_2,int param_3)

{
  float *pfVar1;
  int iVar2;
  double in_f0_1;
  float fVar3;
  double dVar4;
  double dVar5;
  double dVar6;
  double dVar7;
  double dVar8;
  double dVar9;
  double dVar10;
  double dVar11;
  undefined4 *apuStack_d0 [4];
  undefined4 *puStack_c0;
  undefined4 *puStack_bc;
  undefined4 *puStack_b8;
  undefined4 *puStack_b4;
  undefined4 *puStack_b0;
  undefined4 uStack_ac;
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
  float fStack_14;
  float fStack_10;
  float fStack_c;
  float fStack_8;
  
  uStack_2c = DAT_1000004c;
  uStack_28 = DAT_10000050;
  uStack_24 = DAT_10000054;
  uStack_20 = DAT_10000058;
  uStack_3c = DAT_1000005c;
  uStack_38 = DAT_10000060;
  uStack_34 = DAT_10000064;
  uStack_30 = DAT_10000068;
  uStack_4c = DAT_1000006c;
  uStack_48 = DAT_10000070;
  uStack_44 = DAT_10000074;
  uStack_40 = DAT_10000078;
  uStack_5c = DAT_1000007c;
  uStack_58 = DAT_10000080;
  uStack_54 = DAT_10000084;
  uStack_50 = DAT_10000088;
  uStack_6c = DAT_1000008c;
  uStack_68 = DAT_10000090;
  uStack_64 = DAT_10000094;
  uStack_60 = DAT_10000098;
  uStack_7c = DAT_1000009c;
  uStack_78 = DAT_100000a0;
  uStack_74 = DAT_100000a4;
  uStack_70 = DAT_100000a8;
  uStack_8c = DAT_100000ac;
  uStack_88 = DAT_100000b0;
  uStack_84 = DAT_100000b4;
  uStack_80 = DAT_100000b8;
  uStack_9c = DAT_100000bc;
  uStack_98 = DAT_100000c0;
  uStack_94 = DAT_100000c4;
  uStack_90 = DAT_100000c8;
  uStack_ac = DAT_100000cc;
  uStack_a8 = DAT_100000d0;
  uStack_a4 = DAT_100000d4;
  uStack_a0 = DAT_100000d8;
  apuStack_d0[0] = &uStack_2c;
  apuStack_d0[1] = &uStack_3c;
  apuStack_d0[2] = &uStack_4c;
  apuStack_d0[3] = &uStack_5c;
  puStack_c0 = &uStack_6c;
  puStack_bc = &uStack_7c;
  puStack_b8 = &uStack_8c;
  puStack_b4 = &uStack_9c;
  puStack_b0 = &uStack_ac;
  if (param_2 < 9) {
    pfVar1 = (float *)apuStack_d0[param_2];
    fStack_8 = *pfVar1;
    fStack_c = pfVar1[1];
    fStack_10 = pfVar1[2];
    fStack_14 = pfVar1[3];
  }
  else {
    perror("bad window type! Try again...\n");
  }
  iVar2 = 0;
  if (0 < param_3) {
    dVar7 = (double)param_3;
    dVar4 = (double)fStack_c;
    dVar5 = (double)fStack_10;
    dVar10 = (double)fStack_14;
    dVar11 = (double)fStack_8;
    do {
      dVar6 = (double)iVar2;
      cos((dVar6 * 12.566370614359172) / dVar7);
      dVar8 = in_f0_1;
      cos((dVar6 * 6.283185307179586) / dVar7);
      dVar9 = in_f0_1;
      cos((dVar6 * 18.84955592153876) / dVar7);
      iVar2 = iVar2 + 1;
      fVar3 = (float)(in_f0_1 * dVar10 + dVar11 + dVar4 * dVar9 + dVar5 * dVar8);
      *param_1 = fVar3;
      _squark = _squark + (double)(fVar3 * fVar3);
      param_1 = param_1 + 1;
    } while (iVar2 != param_3);
  }
  return;
}

