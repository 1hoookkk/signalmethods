/* draw_slice  entry 0x40aad0  size 12 bytes */

/* WARNING: Removing unreachable block (ram,0x0040b5bc) */
/* WARNING: Removing unreachable block (ram,0x0040b3b8) */
/* WARNING: Removing unreachable block (ram,0x0040b384) */
/* WARNING: Removing unreachable block (ram,0x0040af48) */
/* WARNING: Removing unreachable block (ram,0x0040ac68) */
/* WARNING: Removing unreachable block (ram,0x0040ac80) */
/* WARNING: Removing unreachable block (ram,0x0040ac9c) */
/* WARNING: Removing unreachable block (ram,0x0040ac4c) */
/* WARNING: Removing unreachable block (ram,0x0040af64) */
/* WARNING: Removing unreachable block (ram,0x0040af7c) */
/* WARNING: Removing unreachable block (ram,0x0040af98) */
/* WARNING: Removing unreachable block (ram,0x0040b150) */
/* WARNING: Removing unreachable block (ram,0x0040b16c) */
/* WARNING: Removing unreachable block (ram,0x0040b184) */
/* WARNING: Removing unreachable block (ram,0x0040b1a0) */
/* WARNING: Removing unreachable block (ram,0x0040b3a0) */
/* WARNING: Removing unreachable block (ram,0x0040b3d4) */
/* WARNING: Removing unreachable block (ram,0x0040b5d8) */
/* WARNING: Removing unreachable block (ram,0x0040b5f0) */
/* WARNING: Removing unreachable block (ram,0x0040b60c) */

void draw_slice(int param_1)

{
  double dVar1;
  float fVar2;
  int iVar3;
  float fVar4;
  int iStack_14;
  int iStack_10;
  int iStack_c;
  int iStack_8;
  int iStack_4;
  
  if (DAT_1000acac == 4) {
    bgntmesh();
    dVar1 = (double)FUN_00411414(*(undefined4 *)(&DAT_10046700 + param_1 * 0x2000),
                                 *(undefined4 *)(&DAT_10046704 + param_1 * 0x2000));
    fVar2 = (float)FUN_0040b748((float)dVar1);
    fVar4 = fVar2;
    if (DAT_1000acb0 == 1) {
      fVar4 = 255.0;
    }
    iVar3 = (int)FLOOR(fVar4);
    if (iVar3 < 0) {
      iVar3 = -1;
    }
    color(iVar3);
    iStack_14 = (uint)(DAT_1000acb8 == 0) * param_1;
    iStack_10 = (int)((float)DAT_1000acc0 * fVar2);
    if (DAT_1000acc4 == 0) {
      iStack_c = 0;
    }
    else {
      iStack_c = (int)zlogpos;
    }
    v3i(&iStack_14);
    iStack_4 = 0;
    if (-1 < DAT_1000b97c) {
      do {
        *(undefined4 *)(&surface + param_1 * 0x2000 + iStack_4 * 8) = (&tmpc)[iStack_4 * 2];
        *(undefined4 *)(&DAT_10048704 + param_1 * 0x2000 + iStack_4 * 8) =
             (&DAT_1003c704)[iStack_4 * 2];
        fVar2 = (float)FUN_0040b748((&fx)[iStack_4]);
        fVar4 = fVar2;
        if (DAT_1000acb0 == 1) {
          fVar4 = 255.0;
        }
        iVar3 = (int)FLOOR(fVar4);
        if (iVar3 < 0) {
          iVar3 = -1;
        }
        color(iVar3);
        iStack_14 = iStack_14 + (uint)(DAT_1000acb8 == 0);
        iStack_10 = (int)((float)DAT_1000acc0 * fVar2);
        if (DAT_1000acc4 == 0) {
          fVar4 = (float)iStack_4;
        }
        else {
          fVar4 = (&zlogpos)[iStack_4];
        }
        iStack_c = (int)fVar4;
        v3i(&iStack_14);
        if (param_1 == 0) {
          iStack_8 = 0;
        }
        else {
          iStack_8 = param_1 + -1;
        }
        dVar1 = (double)FUN_00411414(*(undefined4 *)
                                      (&DAT_10048708 + iStack_8 * 0x2000 + iStack_4 * 8),
                                     *(undefined4 *)
                                      (&DAT_1004870c + iStack_8 * 0x2000 + iStack_4 * 8));
        fVar2 = (float)FUN_0040b748((float)dVar1);
        fVar4 = fVar2;
        if (DAT_1000acb0 == 1) {
          fVar4 = 255.0;
        }
        iVar3 = (int)FLOOR(fVar4);
        if (iVar3 < 0) {
          iVar3 = -1;
        }
        color(iVar3);
        iStack_14 = iStack_14 - (uint)(DAT_1000acb8 == 0);
        iStack_10 = (int)((float)DAT_1000acc0 * fVar2);
        if (DAT_1000acc4 == 0) {
          fVar4 = (float)(iStack_4 + 1);
        }
        else {
          fVar4 = *(float *)(&DAT_1204e70c + iStack_4 * 4);
        }
        iStack_c = (int)fVar4;
        v3i(&iStack_14);
        iStack_4 = iStack_4 + 1;
      } while (iStack_4 <= DAT_1000b97c);
    }
    dVar1 = (double)FUN_00411414(*(undefined4 *)(&surface + param_1 * 0x2000 + iStack_4 * 8),
                                 *(undefined4 *)(&DAT_10048704 + param_1 * 0x2000 + iStack_4 * 8));
    fVar2 = (float)FUN_0040b748((float)dVar1);
    fVar4 = fVar2;
    if (DAT_1000acb0 == 1) {
      fVar4 = 255.0;
    }
    iVar3 = (int)FLOOR(fVar4);
    if (iVar3 < 0) {
      iVar3 = -1;
    }
    color(iVar3);
    iStack_14 = (uint)(DAT_1000acb8 == 0) * param_1 + 1;
    iStack_10 = (int)((float)DAT_1000acc0 * fVar2);
    if (DAT_1000acc4 == 0) {
      fVar4 = (float)iStack_4;
    }
    else {
      fVar4 = (&zlogpos)[iStack_4];
    }
    iStack_c = (int)fVar4;
    v3i(&iStack_14);
    endtmesh();
  }
  else {
    if (DAT_1000acac == 1) {
      bgnline();
    }
    else if (DAT_1000acac == 2) {
      bgnpoint();
    }
    else if (DAT_1000acac == 3) {
      bgnpolygon();
    }
    color(0);
    iStack_14 = (uint)(DAT_1000acb8 == 0) * param_1;
    iStack_10 = 0;
    iStack_c = 0;
    v3i(&iStack_14);
    iStack_4 = 0;
    if (-1 < DAT_1000b97c) {
      do {
        fVar2 = (float)FUN_0040b748((&fx)[iStack_4]);
        fVar4 = fVar2;
        if (DAT_1000acb0 == 1) {
          fVar4 = 255.0;
        }
        iVar3 = (int)FLOOR(fVar4);
        if (iVar3 < 0) {
          iVar3 = -1;
        }
        color(iVar3);
        iStack_10 = (int)((float)DAT_1000acc0 * fVar2);
        if (DAT_1000acc4 == 0) {
          fVar4 = (float)iStack_4;
        }
        else {
          fVar4 = (&zlogpos)[iStack_4];
        }
        iStack_c = (int)fVar4;
        v3i(&iStack_14);
        *(undefined4 *)(&surface + param_1 * 0x2000 + iStack_4 * 8) = (&tmpc)[iStack_4 * 2];
        *(undefined4 *)(&DAT_10048704 + param_1 * 0x2000 + iStack_4 * 8) =
             (&DAT_1003c704)[iStack_4 * 2];
        iStack_4 = iStack_4 + 1;
      } while (iStack_4 <= DAT_1000b97c);
    }
    iStack_10 = 0;
    v3i(&iStack_14);
    if (DAT_1000acac == 1) {
      endline();
    }
    else if (DAT_1000acac == 2) {
      endpoint();
    }
    else if (DAT_1000acac == 3) {
      endpolygon();
    }
  }
  return;
}

