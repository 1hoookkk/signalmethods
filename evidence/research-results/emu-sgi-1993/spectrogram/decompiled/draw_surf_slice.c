/* draw_surf_slice  entry 0x408a14  size 12 bytes */

/* WARNING: Removing unreachable block (ram,0x00409c34) */
/* WARNING: Removing unreachable block (ram,0x00409678) */
/* WARNING: Removing unreachable block (ram,0x0040931c) */
/* WARNING: Removing unreachable block (ram,0x00409030) */
/* WARNING: Removing unreachable block (ram,0x00408d48) */
/* WARNING: Removing unreachable block (ram,0x0040904c) */
/* WARNING: Removing unreachable block (ram,0x00409064) */
/* WARNING: Removing unreachable block (ram,0x00409080) */
/* WARNING: Removing unreachable block (ram,0x00409338) */
/* WARNING: Removing unreachable block (ram,0x00409350) */
/* WARNING: Removing unreachable block (ram,0x0040936c) */
/* WARNING: Removing unreachable block (ram,0x00409928) */
/* WARNING: Removing unreachable block (ram,0x00409694) */
/* WARNING: Removing unreachable block (ram,0x004096ac) */
/* WARNING: Removing unreachable block (ram,0x004096c8) */
/* WARNING: Removing unreachable block (ram,0x00409c50) */
/* WARNING: Removing unreachable block (ram,0x00409c68) */
/* WARNING: Removing unreachable block (ram,0x00409c84) */
/* WARNING: Removing unreachable block (ram,0x00409944) */
/* WARNING: Removing unreachable block (ram,0x0040995c) */
/* WARNING: Removing unreachable block (ram,0x00408d64) */
/* WARNING: Removing unreachable block (ram,0x00408d7c) */
/* WARNING: Removing unreachable block (ram,0x00408d98) */

void draw_surf_slice(int param_1)

{
  double dVar1;
  int iVar2;
  float fVar3;
  float fVar4;
  int iStack_1c;
  int iStack_18;
  int iStack_14;
  float fStack_10;
  float fStack_c;
  int iStack_8;
  int iStack_4;
  
  iStack_8 = 1;
  if (DAT_1000acac == 4) {
    bgntmesh();
    if (DAT_1000b988 == 0) {
      fStack_10 = 1.0;
    }
    else {
      fStack_10 = (float)flttbl;
    }
    if (DAT_1000b984 == 0) {
      fVar3 = *(float *)(&surface + param_1 * 0x2000);
    }
    else {
      fVar3 = 20000.0;
    }
    if (DAT_1000b984 == 0) {
      fVar4 = *(float *)(&DAT_10048704 + param_1 * 0x2000);
    }
    else {
      fVar4 = 0.0;
    }
    dVar1 = (double)FUN_00411414(fVar3 * fStack_10,fVar4 * fStack_10);
    fStack_c = (float)dVar1;
    fStack_c = (float)FUN_0040b748(fStack_c);
    fVar3 = fStack_c;
    if (DAT_1000acb0 == 1) {
      fVar3 = 255.0;
    }
    iVar2 = (int)FLOOR(fVar3);
    if (iVar2 < 0) {
      iVar2 = -1;
    }
    color(iVar2);
    iStack_1c = (uint)(DAT_1000acb8 == 0) * param_1;
    iStack_18 = (int)((float)DAT_1000acc0 * fStack_c);
    if (DAT_1000acc4 == 0) {
      iStack_14 = 0;
    }
    else {
      iStack_14 = (int)zlogpos;
    }
    v3i(&iStack_1c);
    iStack_4 = 0;
    if (0 < DAT_1000b97c) {
      do {
        if (DAT_1000b988 == 0) {
          fStack_10 = 1.0;
        }
        else {
          fStack_10 = (float)*(double *)(&DAT_1104a700 + param_1 * 0x2000 + iStack_4 * 8);
        }
        if (DAT_1000b984 == 0) {
          fVar3 = *(float *)(&DAT_1004a700 + param_1 * 0x2000 + iStack_4 * 8);
        }
        else {
          fVar3 = (float)(iStack_8 * 20000);
        }
        *(float *)(&cx + iStack_4 * 8) = fVar3 * fStack_10;
        if (DAT_1000b984 == 0) {
          fVar3 = *(float *)(&DAT_1004a704 + param_1 * 0x2000 + iStack_4 * 8);
        }
        else {
          fVar3 = 0.0;
        }
        *(float *)(&DAT_10044704 + iStack_4 * 8) = fVar3 * fStack_10;
        *(undefined4 *)(&cx + (DAT_1000b978 - iStack_4) * 8) = *(undefined4 *)(&cx + iStack_4 * 8);
        *(float *)(&DAT_10044704 + (DAT_1000b978 - iStack_4) * 8) =
             -*(float *)(&DAT_10044704 + iStack_4 * 8);
        dVar1 = (double)FUN_00411414(*(undefined4 *)(&cx + iStack_4 * 8),
                                     *(undefined4 *)(&DAT_10044704 + iStack_4 * 8));
        fStack_c = (float)dVar1;
        fStack_c = (float)FUN_0040b748(fStack_c);
        fVar3 = fStack_c;
        if (DAT_1000acb0 == 1) {
          fVar3 = 255.0;
        }
        iVar2 = (int)FLOOR(fVar3);
        if (iVar2 < 0) {
          iVar2 = -1;
        }
        color(iVar2);
        iStack_1c = iStack_1c + (uint)(DAT_1000acb8 == 0);
        iStack_18 = (int)((float)DAT_1000acc0 * fStack_c);
        if (DAT_1000acc4 == 0) {
          fVar3 = (float)iStack_4;
        }
        else {
          fVar3 = (&zlogpos)[iStack_4];
        }
        iStack_14 = (int)fVar3;
        v3i(&iStack_1c);
        if (DAT_1000b988 == 0) {
          fStack_10 = 1.0;
        }
        else {
          fStack_10 = (float)*(double *)(&DAT_11048708 + param_1 * 0x2000 + iStack_4 * 8);
        }
        if (DAT_1000b984 == 0) {
          fVar3 = *(float *)(&DAT_10048708 + param_1 * 0x2000 + iStack_4 * 8);
        }
        else {
          fVar3 = 20000.0;
        }
        if (DAT_1000b984 == 0) {
          fVar4 = *(float *)(&DAT_1004870c + param_1 * 0x2000 + iStack_4 * 8);
        }
        else {
          fVar4 = 0.0;
        }
        dVar1 = (double)FUN_00411414(fVar3 * fStack_10,fVar4 * fStack_10);
        fStack_c = (float)dVar1;
        fStack_c = (float)FUN_0040b748(fStack_c);
        fVar3 = fStack_c;
        if (DAT_1000acb0 == 1) {
          fVar3 = 255.0;
        }
        iVar2 = (int)FLOOR(fVar3);
        if (iVar2 < 0) {
          iVar2 = -1;
        }
        color(iVar2);
        iStack_1c = iStack_1c - (uint)(DAT_1000acb8 == 0);
        iStack_18 = (int)((float)DAT_1000acc0 * fStack_c);
        if (DAT_1000acc4 == 0) {
          fVar3 = (float)(iStack_4 + 1);
        }
        else {
          fVar3 = *(float *)(&DAT_1204e70c + iStack_4 * 4);
        }
        iStack_14 = (int)fVar3;
        v3i(&iStack_1c);
        iStack_8 = -iStack_8;
        iStack_4 = iStack_4 + 1;
      } while (iStack_4 < DAT_1000b97c);
    }
    if (DAT_1000b988 == 0) {
      fStack_10 = 1.0;
    }
    else {
      fStack_10 = (float)*(double *)(&DAT_1104a700 + param_1 * 0x2000 + iStack_4 * 8);
    }
    if (DAT_1000b984 == 0) {
      fVar3 = *(float *)(&DAT_1004a700 + param_1 * 0x2000 + iStack_4 * 8);
    }
    else {
      fVar3 = 20000.0;
    }
    *(float *)(&cx + iStack_4 * 8) = fVar3 * fStack_10;
    if (DAT_1000b984 == 0) {
      fVar3 = *(float *)(&DAT_1004a704 + param_1 * 0x2000 + iStack_4 * 8);
    }
    else {
      fVar3 = 0.0;
    }
    *(float *)(&DAT_10044704 + iStack_4 * 8) = fVar3 * fStack_10;
    dVar1 = (double)FUN_00411414(*(undefined4 *)(&cx + iStack_4 * 8),
                                 *(undefined4 *)(&DAT_10044704 + iStack_4 * 8));
    fStack_c = (float)dVar1;
    fStack_c = (float)FUN_0040b748(fStack_c);
    fVar3 = fStack_c;
    if (DAT_1000acb0 == 1) {
      fVar3 = 255.0;
    }
    iVar2 = (int)FLOOR(fVar3);
    if (iVar2 < 0) {
      iVar2 = -1;
    }
    color(iVar2);
    iStack_1c = iStack_1c + (uint)(DAT_1000acb8 == 0);
    iStack_18 = (int)((float)DAT_1000acc0 * fStack_c);
    if (DAT_1000acc4 == 0) {
      fVar3 = (float)iStack_4;
    }
    else {
      fVar3 = (&zlogpos)[iStack_4];
    }
    iStack_14 = (int)fVar3;
    v3i(&iStack_1c);
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
    iStack_1c = (uint)(DAT_1000acb8 == 0) * param_1;
    iStack_18 = 0;
    iStack_14 = 0;
    v3i(&iStack_1c);
    iStack_4 = 0;
    if (0 < DAT_1000b97c) {
      do {
        if (DAT_1000b988 == 0) {
          fStack_10 = 1.0;
        }
        else {
          fStack_10 = (float)(&flttbl)[iStack_4 + param_1 * 0x400];
        }
        if (DAT_1000b984 == 0) {
          fVar3 = *(float *)(&surface + param_1 * 0x2000 + iStack_4 * 8);
        }
        else {
          fVar3 = (float)(iStack_8 * 20000);
        }
        *(float *)(&cx + iStack_4 * 8) = fVar3 * fStack_10;
        if (DAT_1000b984 == 0) {
          fVar3 = *(float *)(&DAT_10048704 + param_1 * 0x2000 + iStack_4 * 8);
        }
        else {
          fVar3 = 0.0;
        }
        *(float *)(&DAT_10044704 + iStack_4 * 8) = fVar3 * fStack_10;
        *(undefined4 *)(&cx + (DAT_1000b978 - iStack_4) * 8) = *(undefined4 *)(&cx + iStack_4 * 8);
        *(float *)(&DAT_10044704 + (DAT_1000b978 - iStack_4) * 8) =
             -*(float *)(&DAT_10044704 + iStack_4 * 8);
        dVar1 = (double)FUN_00411414(*(undefined4 *)(&cx + iStack_4 * 8),
                                     *(undefined4 *)(&DAT_10044704 + iStack_4 * 8));
        fStack_c = (float)dVar1;
        fStack_c = (float)FUN_0040b748(fStack_c);
        fVar3 = fStack_c;
        if (DAT_1000acb0 == 1) {
          fVar3 = 255.0;
        }
        iVar2 = (int)FLOOR(fVar3);
        if (iVar2 < 0) {
          iVar2 = -1;
        }
        color(iVar2);
        iStack_18 = (int)((float)DAT_1000acc0 * fStack_c);
        if (DAT_1000acc4 == 0) {
          fVar3 = (float)iStack_4;
        }
        else {
          fVar3 = (&zlogpos)[iStack_4];
        }
        iStack_14 = (int)fVar3;
        v3i(&iStack_1c);
        iStack_8 = -iStack_8;
        iStack_4 = iStack_4 + 1;
      } while (iStack_4 < DAT_1000b97c);
    }
    if (DAT_1000b988 == 0) {
      fStack_10 = 1.0;
    }
    else {
      fStack_10 = (float)(&flttbl)[iStack_4 + param_1 * 0x400];
    }
    if (DAT_1000b984 == 0) {
      fVar3 = *(float *)(&surface + param_1 * 0x2000 + iStack_4 * 8);
    }
    else {
      fVar3 = 20000.0;
    }
    *(float *)(&cx + iStack_4 * 8) = fVar3 * fStack_10;
    if (DAT_1000b984 == 0) {
      fVar3 = *(float *)(&DAT_10048704 + param_1 * 0x2000 + iStack_4 * 8);
    }
    else {
      fVar3 = 0.0;
    }
    *(float *)(&DAT_10044704 + iStack_4 * 8) = fVar3 * fStack_10;
    dVar1 = (double)FUN_00411414(*(undefined4 *)(&cx + iStack_4 * 8),
                                 *(undefined4 *)(&DAT_10044704 + iStack_4 * 8));
    fStack_c = (float)dVar1;
    fStack_c = (float)FUN_0040b748(fStack_c);
    fVar3 = fStack_c;
    if (DAT_1000acb0 == 1) {
      fVar3 = 255.0;
    }
    iVar2 = (int)FLOOR(fVar3);
    if (iVar2 < 0) {
      iVar2 = -1;
    }
    color(iVar2);
    iStack_18 = (int)((float)DAT_1000acc0 * fStack_c);
    if (DAT_1000acc4 == 0) {
      fVar3 = (float)iStack_4;
    }
    else {
      fVar3 = (&zlogpos)[iStack_4];
    }
    iStack_14 = (int)fVar3;
    v3i(&iStack_1c);
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

