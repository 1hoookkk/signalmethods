/* check_sr  entry 0x4035a0  size 12 bytes */

void check_sr(void)

{
  undefined4 uStack_8;
  int iStack_4;
  
  uStack_8 = 3;
  FUN_00406ecc(1,&uStack_8,2);
  sampling_rate = iStack_4;
  if (iStack_4 == -1) {
    uStack_8 = 0;
    FUN_00406ecc(1,&uStack_8,2);
    if (iStack_4 != 2) {
      fprintf((FILE *)0xfb52904,"Whoa there! Weird sampling rate.\n");
                    /* WARNING: Subroutine does not return */
      exit(-1);
    }
    uStack_8 = 0x14;
    FUN_00406ecc(1,&uStack_8,2);
    if (iStack_4 < 0) {
      fprintf((FILE *)0xfb52904,"Whoa there! Weird sampling rate.\n");
                    /* WARNING: Subroutine does not return */
      exit(-1);
    }
    sampling_rate = iStack_4;
  }
  return;
}


/* oinit  entry 0x4037e4  size 12 bytes */

void oinit(void)

{
  graph = winopen("Waveform");
  cmode();
  mmode(0);
  deflinestyle(1,0xf0f0);
  gconfig();
  FUN_004036d4();
  return;
}


/* draw_axes  entry 0x40389c  size 12 bytes */

void draw_axes(undefined8 param_1)

{
  float fVar1;
  int iVar2;
  int iVar3;
  float fVar4;
  undefined4 uVar5;
  undefined8 uVar6;
  double dVar7;
  ulonglong uVar8;
  float fVar9;
  float fVar11;
  double dVar10;
  uint in_fcsr;
  undefined4 uStack_24;
  undefined4 uStack_20;
  undefined4 uStack_1c;
  undefined4 uStack_18;
  undefined4 uStack_14;
  int iStack_10;
  int iStack_c;
  
  uVar5 = (undefined4)((ulonglong)param_1 >> 0x20);
  uStack_24 = 0x6b487a20;
  uStack_20 = 0x20202020;
  uStack_1c = 0x20202020;
  uStack_18 = 0x20202020;
  uStack_14 = 0x20202000;
  fVar4 = (float)width;
  fVar9 = (float)sampling_rate;
  mapcolor(3,0xff,0xff,0);
  color(3);
  bgnline();
  iStack_10 = 0;
  iStack_c = height;
  if (height < 0) {
    iStack_c = height + 7;
  }
  iStack_c = iStack_c >> 3;
  v2i(&iStack_10);
  iStack_10 = width;
  v2i(&iStack_10);
  endline();
  uVar6 = CONCAT44(uVar5,(float)(width + -0x21));
  iVar2 = height;
  if (height < 0) {
    iVar2 = height + 7;
  }
  cmov(uVar6,(float)((iVar2 >> 3) + -0xf));
  uVar5 = (undefined4)((ulonglong)uVar6 >> 0x20);
  fmprstr(&uStack_24);
  bgnclosedline();
  iStack_10 = 0;
  iStack_c = 0;
  v2i(&iStack_10);
  iStack_c = height + -1;
  v2i(&iStack_10);
  iStack_10 = width + -1;
  v2i(&iStack_10);
  iStack_c = 0;
  v2i(&iStack_10);
  endclosedline();
  pushmatrix();
  uVar6 = CONCAT44(uVar5,fsc);
  scale(uVar6);
  dVar7 = (double)CONCAT44((int)((ulonglong)uVar6 >> 0x20),foff);
  translate(dVar7,0);
  uVar5 = (undefined4)((ulonglong)dVar7 >> 0x20);
  iVar2 = 0;
  if (-1 < sampling_rate) {
    do {
      uVar5 = (undefined4)((ulonglong)dVar7 >> 0x20);
      bgnline();
      fVar11 = (float)iVar2 * ((fVar4 * 2.0) / fVar9);
      if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
        fVar1 = ROUND(fVar11);
      }
      else {
        fVar1 = FLOOR(fVar11);
      }
      iStack_10 = (int)fVar1;
      iStack_c = height;
      if (height < 0) {
        iStack_c = height + 7;
      }
      iStack_c = iStack_c >> 3;
      v2i(&iStack_10);
      iVar3 = height;
      if (height < 0) {
        iVar3 = height + 7;
      }
      iStack_c = (iVar3 >> 3) + -5;
      v2i(&iStack_10);
      endline();
      iVar3 = height;
      if (height < 0) {
        iVar3 = height + 7;
      }
      cmov(CONCAT44(uVar5,fVar11),(float)((iVar3 >> 3) + -0xf));
      dVar7 = (double)iVar2 / 1000.0;
      gcvt(dVar7,3,(char *)&uStack_24);
      fmprstr(&uStack_24);
      uVar5 = (undefined4)((ulonglong)dVar7 >> 0x20);
      iVar2 = iVar2 + 1000;
    } while (iVar2 <= sampling_rate);
  }
  popmatrix();
  pushmatrix();
  uVar8 = CONCAT44(uVar5,0x3f800000);
  scale(uVar8,asc);
  translate(uVar8 & 0xffffffff00000000,aoff);
  iVar2 = -0x50;
  do {
    bgnline();
    iStack_10 = 0;
    dVar10 = (double)(((float)iVar2 + 100.0) / 20.0 + 1.0);
    dVar7 = (double)height * 0.125 * dVar10;
    if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
      dVar7 = ROUND(dVar7);
    }
    else {
      dVar7 = FLOOR(dVar7);
    }
    iStack_c = (int)dVar7;
    v2i(&iStack_10);
    iStack_10 = 10;
    v2i(&iStack_10);
    endline();
    setlinestyle(1);
    bgnline();
    iStack_10 = 10;
    iStack_c = (int)dVar7;
    v2i(&iStack_10);
    iStack_10 = width;
    v2i(&iStack_10);
    endline();
    setlinestyle(0);
    fVar4 = floorf((float)(iVar2 / 0x14));
    in_fcsr = in_fcsr & 0xff7fffff;
    if (fVar4 == (float)(iVar2 / 0x14)) {
      cmov(0x41200000,(float)((double)height * 0.125 * dVar10));
      gcvt((double)iVar2,3,(char *)&uStack_24);
      fmprstr(&uStack_24);
    }
    iVar2 = iVar2 + 0x14;
  } while (iVar2 != 0x28);
  popmatrix();
  return;
}


/* draw_graph  entry 0x403f90  size 12 bytes */

void draw_graph(float *param_1)

{
  int iVar1;
  int iVar2;
  float fVar3;
  float fVar4;
  float fStack_8;
  float fStack_4;
  
  fVar3 = (float)width;
  fVar4 = (float)order;
  color(0xff);
  pushmatrix();
  scale(fsc,asc);
  translate(foff,aoff);
  bgnline();
  iVar2 = 0;
  iVar1 = order;
  if (order < 0) {
    iVar1 = order + 1;
  }
  if (0 < iVar1 >> 1) {
    do {
      fStack_8 = (float)iVar2 * ((fVar3 * 2.0) / fVar4);
      fStack_4 = (*param_1 / 20.0 + -2.0) * (float)height * 0.125 + (float)height;
      v2f(&fStack_8);
      iVar2 = iVar2 + 1;
      param_1 = param_1 + 1;
      iVar1 = order;
      if (order < 0) {
        iVar1 = order + 1;
      }
    } while (iVar2 < iVar1 >> 1);
  }
  endline();
  popmatrix();
  iVar1 = ALgetfilled(port);
  color(2);
  bgnline();
  fStack_8 = 0.0;
  fStack_4 = 10.0;
  v2f(&fStack_8);
  fStack_8 = (float)((iVar1 * width) / 8000);
  v2f(&fStack_8);
  endline();
  return;
}


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


/* demean  entry 0x404690  size 32 bytes */

void demean(short *param_1,uint param_2)

{
  short *psVar1;
  short *psVar2;
  short sVar3;
  int iVar4;
  uint uVar5;
  short *psVar6;
  short *psVar7;
  uint uVar8;
  float fVar9;
  float fVar10;
  byte in_fcsr;
  
  iVar4 = 0;
  uVar5 = 0;
  if (0 < (int)param_2) {
    psVar6 = param_1;
    if ((param_2 & 3) != 0) {
      do {
        uVar5 = uVar5 + 1;
        iVar4 = iVar4 + *psVar6;
        psVar6 = psVar6 + 1;
      } while ((param_2 & 3) != uVar5);
      if (uVar5 == param_2) goto FUN_00404708;
    }
    psVar6 = param_1 + uVar5;
    do {
      sVar3 = *psVar6;
      psVar7 = psVar6 + 1;
      psVar1 = psVar6 + 2;
      psVar2 = psVar6 + 3;
      psVar6 = psVar6 + 4;
      iVar4 = iVar4 + sVar3 + (int)*psVar7 + (int)*psVar1 + (int)*psVar2;
    } while (psVar6 != param_1 + param_2);
  }
FUN_00404708:
  uVar5 = 0;
  uVar8 = param_2 & 3;
  if (0 < (int)param_2) {
    if (uVar8 != 0) {
      if (param_2 == 0) {
        trap(0x1c00);
      }
      if ((param_2 == 0xffffffff) && (iVar4 == -0x80000000)) {
        trap(0x1800);
      }
      uVar5 = 1;
      fVar9 = (float)(int)*param_1;
      psVar6 = param_1;
      psVar7 = param_1;
      if (uVar8 != 1) {
        do {
          fVar9 = fVar9 - (float)(iVar4 / (int)param_2);
          uVar5 = uVar5 + 1;
          if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
            fVar10 = ROUND(fVar9);
          }
          else {
            fVar10 = FLOOR(fVar9);
          }
          psVar7 = psVar6 + 1;
          fVar9 = (float)(int)psVar6[1];
          *psVar6 = (short)(int)fVar10;
          psVar6 = psVar7;
        } while (uVar8 != uVar5);
      }
      fVar9 = fVar9 - (float)(iVar4 / (int)param_2);
      if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
        fVar9 = ROUND(fVar9);
      }
      else {
        fVar9 = FLOOR(fVar9);
      }
      *psVar7 = (short)(int)fVar9;
      if (uVar5 == param_2) {
        return;
      }
    }
    if (param_2 == 0) {
      trap(0x1c00);
    }
    if ((param_2 == 0xffffffff) && (iVar4 == -0x80000000)) {
      trap(0x1800);
    }
    fVar9 = (float)(iVar4 / (int)param_2);
    sVar3 = param_1[uVar5];
    psVar6 = param_1 + uVar5;
    while( true ) {
      psVar7 = psVar6 + 4;
      if (psVar7 == param_1 + param_2) break;
      fVar10 = (float)(int)sVar3 - fVar9;
      if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
        fVar10 = ROUND(fVar10);
      }
      else {
        fVar10 = FLOOR(fVar10);
      }
      *psVar6 = (short)(int)fVar10;
      sVar3 = *psVar7;
      if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
        fVar10 = ROUND((float)(int)psVar6[1] - fVar9);
      }
      else {
        fVar10 = FLOOR((float)(int)psVar6[1] - fVar9);
      }
      psVar6[1] = (short)(int)fVar10;
      if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
        fVar10 = ROUND((float)(int)psVar6[2] - fVar9);
      }
      else {
        fVar10 = FLOOR((float)(int)psVar6[2] - fVar9);
      }
      psVar6[2] = (short)(int)fVar10;
      if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
        fVar10 = ROUND((float)(int)psVar6[3] - fVar9);
      }
      else {
        fVar10 = FLOOR((float)(int)psVar6[3] - fVar9);
      }
      psVar6[3] = (short)(int)fVar10;
      psVar6 = psVar7;
    }
    fVar10 = (float)(int)sVar3 - fVar9;
    if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
      fVar10 = ROUND(fVar10);
    }
    else {
      fVar10 = FLOOR(fVar10);
    }
    *psVar6 = (short)(int)fVar10;
    if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
      fVar10 = ROUND((float)(int)psVar6[1] - fVar9);
    }
    else {
      fVar10 = FLOOR((float)(int)psVar6[1] - fVar9);
    }
    psVar6[1] = (short)(int)fVar10;
    if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
      fVar10 = ROUND((float)(int)psVar6[2] - fVar9);
    }
    else {
      fVar10 = FLOOR((float)(int)psVar6[2] - fVar9);
    }
    psVar6[2] = (short)(int)fVar10;
    if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
      fVar9 = ROUND((float)(int)psVar6[3] - fVar9);
    }
    else {
      fVar9 = FLOOR((float)(int)psVar6[3] - fVar9);
    }
    psVar6[3] = (short)(int)fVar9;
  }
  return;
}


/* winmult  entry 0x4049dc  size 56 bytes */

void winmult(short *param_1,float *param_2,float *param_3,uint param_4)

{
  uint uVar1;
  float *pfVar2;
  short *psVar3;
  float *pfVar4;
  float *pfVar5;
  
  uVar1 = 0;
  if (0 < (int)param_4) {
    pfVar2 = param_3;
    psVar3 = param_1;
    pfVar4 = param_2;
    if ((param_4 & 3) != 0) {
      do {
        uVar1 = uVar1 + 1;
        *pfVar2 = (float)(int)*psVar3 * *pfVar4;
        pfVar2 = pfVar2 + 1;
        psVar3 = psVar3 + 1;
        pfVar4 = pfVar4 + 1;
      } while ((param_4 & 3) != uVar1);
      if (uVar1 == param_4) {
        return;
      }
    }
    pfVar2 = param_3 + uVar1;
    psVar3 = param_1 + uVar1;
    pfVar4 = param_2 + uVar1;
    do {
      pfVar5 = pfVar4 + 4;
      *pfVar2 = (float)(int)*psVar3 * *pfVar4;
      pfVar2[1] = (float)(int)psVar3[1] * pfVar4[1];
      pfVar2[2] = (float)(int)psVar3[2] * pfVar4[2];
      pfVar2[3] = (float)(int)psVar3[3] * pfVar4[3];
      pfVar2 = pfVar2 + 4;
      psVar3 = psVar3 + 4;
      pfVar4 = pfVar5;
    } while (pfVar5 != param_2 + param_4);
  }
  return;
}


/* log_of  entry 0x404af0  size 12 bytes */

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void log_of(float *param_1,int param_2)

{
  float fVar1;
  float *pfVar2;
  float *pfVar3;
  undefined4 uVar4;
  float fVar5;
  undefined4 uVar6;
  float fVar7;
  double dVar8;
  float fVar9;
  
  dVar8 = (double)winsize / _squark;
  fVar5 = log10f((float)order);
  fVar1 = (float)dVar8 / 1.0737418e+09;
  if (power_mode == 0) {
    uVar4 = 0x3ddb7cdf;
    uVar6 = 0xd9d7bdbb;
  }
  else {
    uVar4 = 0x3f1a36e2;
    uVar6 = 0xeb1c432d;
  }
  if (-1 < param_2) {
    fVar9 = (float)((double)CONCAT44(uVar4,uVar6) * (double)(1.0 / fVar1));
    pfVar2 = param_1;
    do {
      fVar7 = *pfVar2;
      if (fVar7 < fVar9) {
        *pfVar2 = fVar9;
        fVar7 = *pfVar2;
      }
      fVar7 = log10f(fVar7 * fVar1);
      pfVar3 = pfVar2 + 1;
      *pfVar2 = fVar7 * 10.0 - fVar5 * 20.0 * (float)power_mode;
      pfVar2 = pfVar3;
    } while (param_1 + param_2 + 1 != pfVar3);
  }
  return;
}


/* xavg  entry 0x404cb0  size 12 bytes */

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void xavg(float *param_1,int param_2,int param_3)

{
  undefined4 uVar1;
  uint uVar2;
  float *pfVar3;
  float *pfVar4;
  uint uVar5;
  undefined4 *puVar6;
  
  if ((param_3 != 0) && (uVar2 = 0, -1 < param_2)) {
    uVar5 = param_2 + 1U & 3;
    if (uVar5 != 0) {
      puVar6 = &DAT_10007818;
      do {
        uVar2 = uVar2 + 1;
        *puVar6 = 0;
        puVar6 = puVar6 + 1;
      } while (uVar5 != uVar2);
      if (param_2 + 1U == uVar2) goto FUN_00404d40;
    }
    puVar6 = &DAT_10007818 + uVar2;
    do {
      uVar2 = uVar2 + 4;
      puVar6[1] = 0;
      puVar6[2] = 0;
      puVar6[3] = 0;
      *puVar6 = 0;
      puVar6 = puVar6 + 4;
    } while (param_2 + 1U != uVar2);
  }
FUN_00404d40:
  uVar2 = 0;
  if (-1 < param_2) {
    uVar5 = param_2 + 1U & 3;
    if (uVar5 != 0) {
      pfVar3 = (float *)&DAT_10007818;
      pfVar4 = param_1;
      do {
        uVar2 = uVar2 + 1;
        *pfVar4 = (float)((double)*pfVar3 * _expk +
                         (double)(float)((double)*pfVar4 * (double)CONCAT44(expg,DAT_1000001c)));
        *pfVar3 = *pfVar4;
        pfVar3 = pfVar3 + 1;
        pfVar4 = pfVar4 + 1;
      } while (uVar5 != uVar2);
      if (param_2 + 1U == uVar2) {
        return;
      }
    }
    pfVar3 = (float *)(&DAT_10007818 + uVar2);
    pfVar4 = param_1 + uVar2;
    do {
      uVar2 = uVar2 + 4;
      *pfVar4 = (float)((double)*pfVar3 * _expk +
                       (double)(float)((double)*pfVar4 * (double)CONCAT44(expg,DAT_1000001c)));
      uVar1 = DAT_1000001c;
      *pfVar3 = *pfVar4;
      pfVar4[1] = (float)((double)pfVar3[1] * _expk +
                         (double)(float)((double)pfVar4[1] * (double)CONCAT44(expg,uVar1)));
      uVar1 = DAT_1000001c;
      pfVar3[1] = pfVar4[1];
      pfVar4[2] = (float)((double)pfVar3[2] * _expk +
                         (double)(float)((double)pfVar4[2] * (double)CONCAT44(expg,uVar1)));
      uVar1 = DAT_1000001c;
      pfVar3[2] = pfVar4[2];
      pfVar4[3] = (float)((double)pfVar3[3] * _expk +
                         (double)(float)((double)pfVar4[3] * (double)CONCAT44(expg,uVar1)));
      pfVar3[3] = pfVar4[3];
      pfVar3 = pfVar3 + 4;
      pfVar4 = pfVar4 + 4;
    } while (param_2 + 1U != uVar2);
  }
  return;
}


/* clr_buf  entry 0x404f1c  size 12 bytes */

void clr_buf(undefined4 *param_1)

{
  int iVar1;
  int iVar2;
  
  iVar2 = 0;
  iVar1 = order;
  if (order < 0) {
    iVar1 = order + 1;
  }
  if (-1 < iVar1 >> 1) {
    do {
      *param_1 = 0xc2c80000;
      iVar2 = iVar2 + 1;
      param_1 = param_1 + 1;
      iVar1 = order;
      if (order < 0) {
        iVar1 = order + 1;
      }
    } while (iVar2 <= iVar1 >> 1);
  }
  return;
}


/* save_graph  entry 0x404f8c  size 12 bytes */

void save_graph(float *param_1)

{
  int iVar1;
  int iVar2;
  double dVar3;
  
  iVar2 = 0;
  iVar1 = order;
  if (order < 0) {
    iVar1 = order + 1;
  }
  if (-1 < iVar1 >> 1) {
    do {
      if (order == 0) {
        trap(0x1c00);
      }
      if ((order == -1) && (sampling_rate * iVar2 == -0x80000000)) {
        trap(0x1800);
      }
      dVar3 = (double)((sampling_rate * iVar2) / order);
      fprintf(fd,"%f %f\n",(int)((ulonglong)dVar3 >> 0x20),SUB84(dVar3,0),
              (double)((*param_1 + 100.0) * 0.65789473 - 100.0));
      iVar2 = iVar2 + 1;
      param_1 = param_1 + 1;
      iVar1 = order;
      if (order < 0) {
        iVar1 = order + 1;
      }
    } while (iVar2 <= iVar1 >> 1);
  }
  fprintf(fd,"\n");
  return;
}


/* rescale  entry 0x405114  size 12 bytes */

void rescale(void)

{
  float fVar1;
  float fVar2;
  int iVar3;
  double dVar4;
  double dVar5;
  double dVar6;
  double dVar7;
  uint in_fcsr;
  
  fVar1 = (float)width;
  iVar3 = sampling_rate;
  if (sampling_rate < 0) {
    iVar3 = sampling_rate + 1;
  }
  fVar2 = (float)height;
  getsize();
  reshapeviewport();
  color(0);
  clear();
  dVar4 = (double)width * 0.1;
  if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
    dVar4 = ROUND(dVar4);
  }
  else {
    dVar4 = FLOOR(dVar4);
  }
  dVar6 = (double)width * 0.9;
  if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
    dVar6 = ROUND(dVar6);
  }
  else {
    dVar6 = FLOOR(dVar6);
  }
  dVar7 = (double)height * 0.1;
  if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
    dVar7 = ROUND(dVar7);
  }
  else {
    dVar7 = FLOOR(dVar7);
  }
  dVar5 = (double)height * 0.9;
  if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
    dVar5 = ROUND(dVar5);
  }
  else {
    dVar5 = FLOOR(dVar5);
  }
  viewport((int)(short)(int)dVar4,(int)(short)(int)dVar6,(int)(short)(int)dVar7,
           (int)(short)(int)dVar5);
  ortho2(0xbf000000,(float)width - 0.5);
  fsc = (float)sampling_rate / ((fmax - fmin) * 2.0);
  asc = 160.0 / (amax - amin);
  foff = -fmin * ((fVar1 * 0.8) / (float)(iVar3 >> 1));
  aoff = (-120.0 - amin) * ((fVar2 * 0.8 * 0.125) / 20.0);
  drawmode(0x20);
  color(0);
  clear();
  FUN_004038a8();
  drawmode(0x10);
  return;
}


/* redraw_everything_and_resize  entry 0x405484  size 12 bytes */

void redraw_everything_and_resize(int param_1,int param_2,int param_3)

{
  int iVar1;
  undefined2 *puVar2;
  double dVar4;
  double dVar5;
  double dVar6;
  double dVar7;
  uint in_fcsr;
  undefined1 auStack_7e9c [8192];
  undefined2 auStack_5e9c [8000];
  undefined1 auStack_201c [8212];
  int iStack_8;
  undefined2 *puVar3;
  
  getsize(&width,&height);
  dVar5 = (double)width * 0.1;
  if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
    dVar5 = ROUND(dVar5);
  }
  else {
    dVar5 = FLOOR(dVar5);
  }
  dVar7 = (double)width * 0.9;
  if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
    dVar7 = ROUND(dVar7);
  }
  else {
    dVar7 = FLOOR(dVar7);
  }
  dVar4 = (double)height * 0.1;
  if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
    dVar4 = ROUND(dVar4);
  }
  else {
    dVar4 = FLOOR(dVar4);
  }
  dVar6 = (double)height * 0.9;
  if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
    dVar6 = ROUND(dVar6);
  }
  else {
    dVar6 = FLOOR(dVar6);
  }
  viewport((int)(short)(int)dVar5,(int)(short)(int)dVar7,(int)(short)(int)dVar4,
           (int)(short)(int)dVar6);
  dVar5 = 0.5;
  ortho2(0xbf000000,(float)width - 0.5);
  if (param_3 == 0) {
    iStack_8 = sampling_rate;
    FUN_004035ac();
    if (iStack_8 != sampling_rate) {
      iVar1 = sampling_rate;
      if (sampling_rate < 0) {
        iVar1 = sampling_rate + 1;
      }
      fmax = (float)(iVar1 >> 1);
      fmin = 0;
      FUN_00405120();
    }
    FUN_00408cac(port,auStack_5e9c,winsize);
  }
  else {
    AFgetrate(param_3,0x3e9);
    if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
      dVar5 = ROUND(dVar5);
    }
    else {
      dVar5 = FLOOR(dVar5);
    }
    sampling_rate = (int)dVar5;
    iVar1 = AFreadframes(param_3,0x3e9,auStack_5e9c,winsize);
    if ((iVar1 != winsize) && (iVar1 <= winsize)) {
      puVar2 = auStack_5e9c + iVar1;
      do {
        puVar3 = puVar2 + 1;
        *puVar2 = 0;
        puVar2 = puVar3;
      } while (puVar3 <= auStack_5e9c + winsize);
    }
  }
  if (removeDC != 0) {
    demean(auStack_5e9c,winsize);
  }
  winmult(auStack_5e9c,&wintbl,auStack_201c,winsize);
  FUN_004065bc(auStack_201c,winsize,auStack_7e9c,order);
  if (expavg != 0) {
    iVar1 = order;
    if (order < 0) {
      iVar1 = order + 1;
    }
    FUN_00404cbc(auStack_7e9c,iVar1 >> 1,param_2);
  }
  iVar1 = order;
  if (order < 0) {
    iVar1 = order + 1;
  }
  FUN_00404afc(auStack_7e9c,iVar1 >> 1);
  color(0);
  clear();
  if (param_2 != 0) {
    FUN_00404f28(auStack_7e9c);
  }
  FUN_00403f9c(auStack_7e9c);
  if (param_1 != 0) {
    FUN_00404f98(auStack_7e9c);
    fflush(fd);
  }
  return;
}


/* aifopen  entry 0x40591c  size 12 bytes */

void aifopen(undefined4 param_1)

{
  undefined4 uVar1;
  
  uVar1 = AFnewfilesetup();
  AFopenfile(param_1,&DAT_100009b4,uVar1);
  return;
}


/* usage  entry 0x40596c  size 12 bytes */

void usage(void)

{
  printf("Usage:\tspan <-ak> <-ffilename> <-d> <-wW> <-oM> <-nN> <-h> <-t> <-p>\n");
  printf("\t<aifffile>\n");
  printf("\n");
  printf("\tDisplays the power spectrum of the audio input.\n");
  printf("\tk is the feedback constant in the exponential smoother (default\n");
  printf("\tno smoothing). filename is an xgraph file to write snapshots to.\n");
  printf("\t-d will cause the DC component to be removed\n");
  printf("\tW is window type (Blackman), M is FFT size(1024)\n");
  printf("\tN is window length(1024), -h prints this msg,\n");
  printf("\t-t prints the window options.\n");
  printf("\t-p displays power (time-normalized) instead of energy\n");
  printf("\tif aifffile is specified, input will be read from the named file.\n");
  printf("\tNOTE: aifffile must be the LAST argument!\n\n");
  printf("\tWhile active, you can press the following keys:\n\n");
  printf("\tC: \treset the averager\n");
  printf("\tQ: \tquit\n");
  printf("\tR: \trewind to beginning of file\n");
  printf("\tS: \ttake a snapshot\n");
  printf("\tX: \tchange x-axis limits\n");
  printf("\tY: \tchange y-axis limits\n");
  return;
}


/* winhelp  entry 0x405bb8  size 12 bytes */

void winhelp(void)

{
  printf("Valid window args are:\n");
  printf("\n");
  printf("\t0 = exact blackman, 1 = blackman, 2-5 = blackman-harris\n");
  printf("\t6 = hamming, 7 = hanning, 8 = rectangular (no windowing!)\n");
  return;
}


/* main  entry 0x405c44  size 12 bytes */

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void main(int param_1,undefined4 *param_2)

{
  int iVar1;
  undefined4 uVar2;
  char *pcVar3;
  int iVar4;
  undefined4 uVar5;
  undefined4 uVar6;
  double dVar7;
  double dVar8;
  double dVar9;
  double dVar10;
  uint in_fcsr;
  short asStack_22 [3];
  int iStack_1c;
  undefined4 uStack_18;
  
  uVar6 = 0;
  foreground();
  printf("%s\n","Copyright (c) 1995 E-mu Systems, Inc.");
  iVar4 = 1;
  if (1 < param_1) {
    do {
      param_2 = param_2 + 1;
      pcVar3 = (char *)*param_2;
      if (*pcVar3 == '-') {
        dVar7 = _expg;
        switch(pcVar3[1]) {
        case 'a':
          dVar7 = atof(pcVar3 + 2);
          expk = (undefined4)((ulonglong)dVar7 >> 0x20);
          DAT_10000024 = SUB84(dVar7,0);
          expavg = 1;
          dVar7 = 1.0 - dVar7;
          break;
        default:
          FUN_00405978();
                    /* WARNING: Subroutine does not return */
          exit(1);
        case 'd':
          removeDC = 1;
          break;
        case 'f':
          fd = fopen(pcVar3 + 2,"w");
          dVar7 = _expg;
          break;
        case 'h':
          FUN_00405978();
                    /* WARNING: Subroutine does not return */
          exit(0);
        case 'n':
          winsize = atoi(pcVar3 + 2);
          dVar7 = _expg;
          break;
        case 'o':
          order = atoi(pcVar3 + 2);
          dVar7 = _expg;
          break;
        case 'p':
          power_mode = 1;
          break;
        case 't':
          FUN_00405bc4();
                    /* WARNING: Subroutine does not return */
          exit(0);
        case 'w':
          wintype = atoi(pcVar3 + 2);
          dVar7 = _expg;
        }
      }
      else {
        uVar6 = FUN_00405928(pcVar3);
        iVar1 = AFgetframecnt(uVar6,0x3e9);
        iStack_1c = iVar1 / winsize;
        if (winsize == 0) {
          trap(0x1c00);
        }
        if ((winsize == -1) && (iVar1 == -0x80000000)) {
          trap(0x1800);
        }
        in_fcsr = in_fcsr & 0xff7fffff;
        dVar7 = 1.0 / (double)iStack_1c;
        if (_expg != 0.0) {
          dVar7 = _expg;
        }
      }
      _expg = dVar7;
      iVar4 = iVar4 + 1;
    } while (iVar4 != param_1);
  }
  FUN_004037f0();
  fminit();
  iVar4 = fmfindfont("Times-Roman");
  if (iVar4 == 0) {
                    /* WARNING: Subroutine does not return */
    exit(1);
  }
  uVar2 = fmscalefont(iVar4);
  fmsetfont(uVar2);
  FUN_004035ac();
  iVar4 = sampling_rate;
  if (sampling_rate < 0) {
    iVar4 = sampling_rate + 1;
  }
  fmax = (float)(iVar4 >> 1);
  winset(graph);
  getsize(&width,&height);
  reshapeviewport();
  color(0);
  clear();
  dVar7 = (double)width * 0.1 - 1.0;
  if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
    dVar7 = ROUND(dVar7);
  }
  else {
    dVar7 = FLOOR(dVar7);
  }
  dVar9 = (double)width * 0.9;
  if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
    dVar9 = ROUND(dVar9);
  }
  else {
    dVar9 = FLOOR(dVar9);
  }
  dVar10 = (double)height * 0.1;
  if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
    dVar10 = ROUND(dVar10);
  }
  else {
    dVar10 = FLOOR(dVar10);
  }
  dVar8 = (double)height * 0.9;
  if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
    dVar8 = ROUND(dVar8);
  }
  else {
    dVar8 = FLOOR(dVar8);
  }
  viewport((int)(short)(int)dVar7,(int)(short)(int)dVar9,(int)(short)(int)dVar10,
           (int)(short)(int)dVar8);
  drawmode(0x20);
  FUN_004038a8();
  drawmode(0x10);
  FUN_004042a4(&wintbl,wintype,winsize);
  qdevice(0x210);
  qdevice(10);
  qdevice(0x18);
  qdevice(0x1c);
  qdevice(0x15);
  qdevice(0x20);
  if (fd != (FILE *)0x0) {
    qdevice(0xc);
  }
  uVar2 = uStack_18;
  uVar5 = 0;
caseD_b:
  while( true ) {
    while (iVar4 = qtest(), iVar4 == 0) {
      FUN_00405490(uVar5,uVar2,uVar6);
      uVar2 = 0;
      uVar5 = 0;
    }
    iVar4 = qread(asStack_22);
    if (iVar4 < 0x21) break;
    if (iVar4 == 0x210) {
      FUN_00405120();
    }
  }
  switch(iVar4) {
  case 10:
    goto caseD_a;
  default:
    goto caseD_b;
  case 0xc:
    if (asStack_22[0] != 0) {
      uVar5 = 1;
      goto caseD_b;
    }
    break;
  case 0x15:
    break;
  case 0x18:
    if (iStack_1c != 0) {
      AFseekframe(uVar6,0x3e9,0);
    }
    goto caseD_b;
  case 0x1c:
    if (asStack_22[0] != 0) {
      uVar2 = 1;
    }
    goto caseD_b;
  case 0x20:
    goto code_r0x004064f0;
  }
  if (asStack_22[0] == 0) {
code_r0x004064f0:
    if (asStack_22[0] != 0) {
      printf("Min Amp:\n");
      scanf("%f",&amin);
      printf("Max Amp:\n");
      scanf("%f",&amax);
      FUN_00405120();
    }
  }
  else {
    printf("Min Freq:\n");
    scanf("%f",&fmin);
    printf("Max Freq:\n");
    scanf("%f",&fmax);
    FUN_00405120();
  }
  goto caseD_b;
caseD_a:
  fclose(fd);
                    /* WARNING: Subroutine does not return */
  exit(0);
}


/* spectrum  entry 0x4065b0  size 12 bytes */

/* WARNING: Instruction at (ram,0x00406708) overlaps instruction at (ram,0x00406704)
    */

void spectrum(undefined4 *param_1,uint param_2,float *param_3,uint param_4)

{
  float *pfVar1;
  undefined4 *puVar2;
  undefined4 *puVar3;
  float *pfVar4;
  float *pfVar5;
  uint uVar6;
  uint uVar7;
  uint uVar8;
  int iVar10;
  int iVar11;
  undefined4 uVar12;
  uint uVar9;
  
  iVar10 = 1;
  iVar11 = 0;
  if (1 < (int)param_4) {
    do {
      iVar10 = iVar10 << 1;
      iVar11 = iVar11 + 1;
    } while (iVar10 < (int)param_4);
  }
  if (iVar11 != fftinitialized) {
    FUN_004069ec(iVar11);
    fftinitialized = iVar11;
  }
  puVar2 = &tmpc;
  uVar7 = 0;
  puVar3 = puVar2;
  if (0 < (int)param_2) {
    if ((param_2 & 3) != 0) {
      do {
        uVar12 = *param_1;
        uVar7 = uVar7 + 1;
        puVar3[1] = 0;
        param_1 = param_1 + 1;
        puVar2 = puVar3 + 2;
        *puVar3 = uVar12;
        puVar3 = puVar2;
      } while ((param_2 & 3) != uVar7);
      if (uVar7 == param_2) goto FUN_004066d0;
    }
    do {
      uVar12 = *param_1;
      puVar2[1] = 0;
      *puVar2 = uVar12;
      uVar12 = param_1[1];
      puVar2[3] = 0;
      puVar2[2] = uVar12;
      uVar12 = param_1[2];
      puVar2[5] = 0;
      puVar2[4] = uVar12;
      uVar12 = param_1[3];
      uVar7 = uVar7 + 4;
      puVar2[7] = 0;
      param_1 = param_1 + 4;
      puVar3 = puVar2 + 8;
      puVar2[6] = uVar12;
      puVar2 = puVar3;
    } while (uVar7 != param_2);
  }
FUN_004066d0:
  uVar7 = 0;
  uVar6 = iVar10 - param_2;
  if (((int)param_2 < iVar10) && (0 < (int)uVar6)) {
    if ((uVar6 & 3) == 0) goto FUN_00406708;
    do {
      uVar9 = uVar7;
      uVar8 = uVar9 + 1;
      *puVar3 = 0;
      puVar3[1] = 0;
      puVar3 = puVar3 + 2;
      uVar7 = uVar8;
    } while ((uVar6 & 3) != uVar8);
    uVar7 = uVar9 + 5;
    puVar2 = puVar3;
    if (uVar8 != uVar6) {
      while( true ) {
        puVar2[2] = 0;
        puVar2[3] = 0;
        puVar2[4] = 0;
        puVar2[5] = 0;
        puVar2[6] = 0;
        puVar2[7] = 0;
        puVar3 = puVar2 + 8;
        *puVar2 = 0;
        puVar2[1] = 0;
        if (uVar7 == uVar6) break;
FUN_00406708:
        uVar7 = uVar7 + 4;
        puVar2 = puVar3;
      }
    }
  }
  FUN_00406b88(&tmpc,iVar11);
  FUN_00406c3c(&tmpc,iVar11);
  uVar7 = 0;
  if (0 < (int)param_4) {
    pfVar5 = (float *)&tmpc;
    pfVar4 = (float *)&tmpc;
    if ((param_4 & 3) != 0) {
      do {
        uVar7 = uVar7 + 1;
        pfVar1 = param_3 + 1;
        pfVar5 = pfVar4 + 2;
        *param_3 = *pfVar4 * *pfVar4;
        *param_3 = *param_3 + pfVar4[1] * pfVar4[1];
        param_3 = pfVar1;
        pfVar4 = pfVar5;
      } while ((param_4 & 3) != uVar7);
      if (uVar7 == param_4) {
        return;
      }
    }
    do {
      uVar7 = uVar7 + 4;
      *param_3 = *pfVar5 * *pfVar5;
      *param_3 = *param_3 + pfVar5[1] * pfVar5[1];
      param_3[1] = pfVar5[2] * pfVar5[2];
      param_3[1] = param_3[1] + pfVar5[3] * pfVar5[3];
      param_3[2] = pfVar5[4] * pfVar5[4];
      param_3[2] = param_3[2] + pfVar5[5] * pfVar5[5];
      param_3[3] = pfVar5[6] * pfVar5[6];
      param_3[3] = param_3[3] + pfVar5[7] * pfVar5[7];
      param_3 = param_3 + 4;
      pfVar5 = pfVar5 + 8;
    } while (uVar7 != param_4);
  }
  return;
}


/* inspect  entry 0x406878  size 48 bytes */

void inspect(float *param_1,float *param_2,uint param_3)

{
  int iVar1;
  float *pfVar2;
  uint uVar3;
  int iVar4;
  float fVar5;
  float fVar6;
  
  iVar1 = 1;
  iVar4 = 0;
  if (1 < (int)param_3) {
    do {
      iVar1 = iVar1 << 1;
      iVar4 = iVar4 + 1;
    } while (iVar1 < (int)param_3);
  }
  if (iVar4 != fftinitialized) {
    FUN_004069ec(iVar4);
    fftinitialized = iVar4;
  }
  FUN_00406b88(param_1,iVar4);
  FUN_00406d90(param_1,iVar4);
  uVar3 = 0;
  fVar5 = 1.0 / (float)(int)param_3;
  if (0 < (int)param_3) {
    if ((param_3 & 3) != 0) {
      do {
        fVar6 = *param_1;
        uVar3 = uVar3 + 1;
        pfVar2 = param_2 + 1;
        param_1 = param_1 + 2;
        *param_2 = fVar6 * fVar5;
        param_2 = pfVar2;
      } while ((param_3 & 3) != uVar3);
      if (uVar3 == param_3) {
        return;
      }
    }
    do {
      uVar3 = uVar3 + 4;
      *param_2 = *param_1 * fVar5;
      param_2[1] = param_1[2] * fVar5;
      param_2[2] = param_1[4] * fVar5;
      param_2[3] = param_1[6] * fVar5;
      param_2 = param_2 + 4;
      param_1 = param_1 + 8;
    } while (uVar3 != param_3);
  }
  return;
}


/* buildtable  entry 0x4069e0  size 12 bytes */

void buildtable(int param_1)

{
  void *pvVar1;
  void *pvVar2;
  int iVar3;
  double dVar4;
  double __x;
  double dVar5;
  
  iVar3 = *(int *)(&pwr_two + param_1 * 4);
  pvVar2 = (&DAT_100000e0)[param_1];
  if (DAT_100000e0 != (void *)0x0) {
    free(DAT_100000e0);
  }
  DAT_100000e0 = malloc((int)pvVar2 << 3);
  if (DAT_100000e0 == (void *)0x0) {
    printf("malloc failed\n");
                    /* WARNING: Subroutine does not return */
    exit(0);
  }
  pvVar1 = (void *)0x0;
  if (0 < (int)pvVar2) {
    dVar5 = (double)iVar3;
    iVar3 = 0;
    do {
      __x = (double)(float)(((double)(int)pvVar1 * 6.283185307179586) / dVar5);
      dVar4 = cos(__x);
      *(float *)((int)DAT_100000e0 + iVar3) = (float)dVar4;
      dVar4 = sin(__x);
      pvVar1 = (void *)((int)pvVar1 + 1);
      *(float *)((int)DAT_100000e0 + iVar3 + 4) = (float)-dVar4;
      iVar3 = iVar3 + 8;
    } while (pvVar1 != pvVar2);
  }
  return;
}


/* bitreverse  entry 0x406b7c  size 12 bytes */

void bitreverse(int param_1,int param_2)

{
  int iVar1;
  uint uVar2;
  int iVar3;
  undefined4 *puVar4;
  undefined4 *puVar5;
  int iVar6;
  uint *puVar7;
  uint uVar8;
  undefined4 uVar9;
  undefined4 uVar10;
  
  iVar1 = *(int *)(&pwr_two + param_2 * 4);
  uVar2 = 0;
  iVar3 = 0;
  do {
    if (iVar3 < (int)uVar2) {
      puVar5 = (undefined4 *)(param_1 + uVar2 * 8);
      puVar4 = (undefined4 *)(param_1 + iVar3 * 8);
      uVar9 = *puVar4;
      *puVar4 = *puVar5;
      uVar10 = puVar4[1];
      puVar4[1] = puVar5[1];
      *puVar5 = uVar9;
      puVar5[1] = uVar10;
    }
    iVar3 = iVar3 + 1;
    iVar6 = param_2 + -1;
    if (iVar3 == iVar1) {
      return;
    }
    if (-1 < iVar6) {
      puVar7 = (uint *)(&pwr_two + iVar6 * 4);
      do {
        uVar8 = *puVar7;
        puVar7 = puVar7 + -1;
        if ((uVar8 & uVar2) == 0) break;
        iVar6 = iVar6 + -1;
        uVar2 = uVar2 - uVar8;
      } while (-1 < iVar6);
    }
    uVar2 = *(int *)(&pwr_two + iVar6 * 4) + uVar2;
  } while( true );
}


/* fft  entry 0x406c30  size 12 bytes */

void fft(int param_1,int param_2)

{
  bool bVar1;
  uint uVar2;
  uint uVar3;
  uint uVar4;
  int iVar5;
  int iVar6;
  uint uVar7;
  int iVar8;
  int iVar9;
  float *pfVar10;
  float *pfVar11;
  float fVar12;
  float fVar13;
  float fVar14;
  float fVar15;
  float fVar16;
  float fVar17;
  
  iVar5 = *(int *)(&pwr_two + param_2 * 4);
  iVar6 = (&DAT_100000e0)[param_2];
  uVar3 = param_2 - 1;
  uVar2 = 1;
  if (param_2 != 0) {
    uVar4 = uVar3;
    do {
      uVar7 = 0;
      if (iVar6 + -1 != -1) {
        iVar8 = iVar6 + -1;
        do {
          iVar9 = uVar7 + uVar2;
          pfVar10 = (float *)(param_1 + iVar9 * 8);
          pfVar11 = (float *)(DAT_100000e0 + (uVar7 << (uVar3 & 0x1f) & iVar5 - 1U) * 8);
          fVar12 = *pfVar10;
          fVar13 = *pfVar11;
          fVar14 = pfVar11[1];
          pfVar11 = (float *)(param_1 + uVar7 * 8);
          fVar16 = *pfVar11;
          fVar17 = pfVar11[1];
          fVar15 = fVar12 * fVar13 - pfVar10[1] * fVar14;
          *pfVar10 = fVar16 - fVar15;
          fVar12 = fVar12 * fVar14 + pfVar10[1] * fVar13;
          uVar7 = iVar9 + 1U & ~uVar2;
          *(float *)(param_1 + iVar9 * 8 + 4) = fVar17 - fVar12;
          *pfVar11 = fVar16 + fVar15;
          pfVar11[1] = fVar17 + fVar12;
          bVar1 = iVar8 != 0;
          iVar8 = iVar8 + -1;
        } while (bVar1);
      }
      uVar2 = uVar2 << 1;
      uVar3 = uVar3 - 1;
      bVar1 = uVar4 != 0;
      uVar4 = uVar4 - 1;
    } while (bVar1);
  }
  return;
}


/* ifft  entry 0x406d84  size 12 bytes */

void ifft(int param_1,int param_2)

{
  bool bVar1;
  uint uVar2;
  uint uVar3;
  uint uVar4;
  int iVar5;
  int iVar6;
  uint uVar7;
  int iVar8;
  float *pfVar9;
  float *pfVar10;
  float fVar11;
  float fVar12;
  float fVar13;
  float fVar14;
  
  iVar5 = *(int *)(&pwr_two + param_2 * 4);
  iVar6 = (&DAT_100000e0)[param_2];
  uVar3 = param_2 - 1;
  uVar2 = 1;
  if (param_2 != 0) {
    uVar4 = uVar3;
    do {
      uVar7 = 0;
      if (iVar6 + -1 != -1) {
        iVar8 = iVar6 + -1;
        do {
          pfVar9 = (float *)(param_1 + (uVar7 + uVar2) * 8);
          pfVar10 = (float *)(DAT_100000e0 + (uVar7 << (uVar3 & 0x1f) & iVar5 - 1U) * 8);
          fVar13 = pfVar10[1];
          fVar14 = *pfVar10;
          fVar12 = *pfVar9;
          pfVar10 = (float *)(param_1 + uVar7 * 8);
          uVar7 = uVar7 + uVar2 + 1 & ~uVar2;
          fVar11 = fVar13 * pfVar9[1] + fVar12 * fVar14;
          *pfVar9 = *pfVar10 - fVar11;
          fVar12 = fVar14 * pfVar9[1] - fVar12 * fVar13;
          pfVar9[1] = pfVar10[1] - fVar12;
          *pfVar10 = *pfVar10 + fVar11;
          pfVar10[1] = pfVar10[1] + fVar12;
          bVar1 = iVar8 != 0;
          iVar8 = iVar8 + -1;
        } while (bVar1);
      }
      uVar2 = uVar2 << 1;
      uVar3 = uVar3 - 1;
      bVar1 = uVar4 != 0;
      uVar4 = uVar4 - 1;
    } while (bVar1);
  }
  return;
}


