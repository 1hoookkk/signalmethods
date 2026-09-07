/* draw_grid  entry 0x40c478  size 12 bytes */

void draw_grid(void)

{
  short sVar1;
  int iVar2;
  int iVar3;
  int iVar4;
  float fVar5;
  float fVar6;
  undefined4 uVar7;
  double dVar8;
  double dVar9;
  char acStack_3c [24];
  int iStack_24;
  int iStack_20;
  int iStack_1c;
  
  winset(DAT_1000ac98);
  FUN_00411f20();
  while( true ) {
    FUN_004121c0();
    color(0xff);
    pushmatrix();
    FUN_0040a3d8(0xffffffff);
    if (DAT_1000acb8 == 0) {
      iStack_20 = 0;
      iVar3 = 0;
      if (-1 < DAT_1000b97c) {
        do {
          bgnline();
          iStack_24 = DAT_1000b91c;
          if (DAT_1000acc4 == 0) {
            fVar6 = (float)iVar3;
          }
          else {
            fVar6 = (float)(&zlogpos)[iVar3];
          }
          iStack_1c = (int)fVar6;
          v3i(&iStack_24);
          iStack_24 = DAT_1000b91c + DAT_1000acb0;
          v3i(&iStack_24);
          endline();
          if (DAT_1000b93c != 0) {
            iVar2 = sampling_rate * iVar3;
            dVar8 = (double)DAT_1000b978;
            cmov((float)iStack_24,(float)(iStack_20 + -10));
            gcvt(((double)iVar2 * 0.001) / dVar8,3,acStack_3c);
            fmprstr(acStack_3c);
          }
          iVar3 = iVar3 + 0x14;
        } while (iVar3 <= DAT_1000b97c);
      }
      iVar2 = DAT_1000b91c;
      if (DAT_1000b91c <= DAT_1000b91c + DAT_1000acb0) {
        do {
          bgnline();
          iStack_1c = 0;
          iStack_24 = iVar2;
          v3i(&iStack_24);
          if (DAT_1000acc4 == 0) {
            fVar6 = (float)(iVar3 + -0x14);
          }
          else {
            fVar6 = *(float *)(iVar3 * 4 + 0x1204e6b8);
          }
          iStack_1c = (int)fVar6;
          v3i(&iStack_24);
          endline();
          if (DAT_1000b93c != 0) {
            dVar9 = (double)DAT_1000b974;
            dVar8 = (double)sampling_rate;
            cmov((float)iStack_24,(float)iStack_20);
            gcvt(((double)iVar2 * dVar9) / dVar8,3,acStack_3c);
            fmprstr(acStack_3c);
          }
          iVar2 = iVar2 + 0x14;
        } while (iVar2 <= DAT_1000b91c + DAT_1000acb0);
      }
      color(0xff);
      bgnclosedline();
      iStack_24 = DAT_1000b91c;
      iStack_20 = 0;
      iStack_1c = 0;
      v3i(&iStack_24);
      iStack_1c = DAT_1000b97c;
      v3i(&iStack_24);
      iStack_20 = 0xff;
      v3i(&iStack_24);
      iStack_1c = 0;
      v3i(&iStack_24);
      endclosedline();
      bgnline();
      iStack_24 = DAT_1000b91c;
      fVar6 = (float)FUN_0040c238(DAT_1000b91c);
      iStack_20 = (int)fVar6;
      iStack_1c = 0;
      v3i(&iStack_24);
      iStack_1c = DAT_1000b97c;
      v3i(&iStack_24);
      endline();
      if (DAT_1000b93c != 0) {
        iVar3 = 0;
        do {
          bgnline();
          iStack_24 = DAT_1000b91c;
          fVar6 = powf(10.0,(float)(0xc - iVar3));
          fVar6 = log10f((float)(((double)fVar6 * largest) / 1000000000000.0));
          iStack_20 = (int)((double)fVar6 * m + b);
          iStack_1c = DAT_1000b97c + -3;
          v3i(&iStack_24);
          iStack_1c = DAT_1000b97c;
          v3i(&iStack_24);
          uVar7 = 0;
          endline();
          cmov(CONCAT44(uVar7,(float)iStack_24),(float)(iStack_20 + -4));
          gcvt((double)(iVar3 * -5),3,acStack_3c);
          fmprstr(acStack_3c);
          iVar3 = iVar3 + 1;
        } while (iVar3 < 0xd);
      }
    }
    else {
      if (DAT_1000acc4 == 1) {
        fVar6 = *(float *)(&DAT_1204e704 + DAT_1000b97c * 4);
      }
      else {
        fVar6 = (float)(DAT_1000b97c + -1);
      }
      iVar2 = (int)fVar6;
      iStack_20 = 0;
      iVar3 = 0;
      if (-1 < iVar2) {
        do {
          bgnline();
          iVar4 = 0;
          do {
            fVar6 = ((float)iVar4 * 6.2831855) / 360.0;
            fVar5 = cosf(fVar6);
            iStack_24 = (int)(fVar5 * (float)iVar3);
            fVar6 = sinf(fVar6);
            iStack_1c = (int)(fVar6 * (float)iVar3);
            v3i(&iStack_24);
            iVar4 = iVar4 + 1;
          } while (iVar4 < 0x169);
          endline();
          iVar3 = iVar3 + 0x14;
        } while (iVar3 <= iVar2);
      }
      iStack_1c = 0;
      iVar3 = 0;
      do {
        bgnline();
        iStack_24 = 0;
        iStack_1c = 0;
        v3i(&iStack_24);
        fVar6 = ((float)iVar3 * 6.2831855) / 360.0;
        fVar5 = cosf(fVar6);
        iStack_24 = (int)(fVar5 * (float)iVar2);
        fVar6 = sinf(fVar6);
        iStack_1c = (int)(fVar6 * (float)iVar2);
        v3i(&iStack_24);
        endline();
        iVar3 = iVar3 + 0x14;
      } while (iVar3 < 0x168);
    }
    popmatrix();
    if (function == 1) {
      sVar1 = getvaluator(0x10a);
      DAT_1000b944 = (float)(int)sVar1;
      sVar1 = getvaluator(0x10b);
      DAT_1000b948 = (float)(int)sVar1;
      DAT_1000b960 = (undefined2)
                     (int)(((DAT_1000b94c - DAT_1000b944) * 2.0 * 1279.0) / (float)theScreen);
      DAT_1000b962 = (undefined2)
                     (int)(((DAT_1000b948 - DAT_1000b950) * 2.0 * 1023.0) / (float)DAT_1000ac8c);
    }
    else if (function == 2) {
      sVar1 = getvaluator(0x10a);
      DAT_1000b944 = (float)(int)sVar1;
      sVar1 = getvaluator(0x10b);
      DAT_1000b948 = (float)(int)sVar1;
      DAT_1000b958 = (((DAT_1000b94c - DAT_1000b944) + (DAT_1000b948 - DAT_1000b950)) * 1279.0) /
                     (float)theScreen;
    }
    if ((DAT_1000b964 == 0) || (DAT_1000b90c != 0)) break;
    FUN_0041973c();
    swapbuffers();
  }
  return;
}

