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

