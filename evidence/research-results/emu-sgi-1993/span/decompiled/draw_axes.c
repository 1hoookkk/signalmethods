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

