/* drawsurf  entry 0x4087a0  size 12 bytes */

void drawsurf(void)

{
  int iStack_4;
  
  mmode(2);
  iStack_4 = DAT_1000b91c;
  if (DAT_1000b91c < DAT_1000acb0 + DAT_1000b91c) {
    do {
      pushmatrix();
      FUN_0040a3d8(iStack_4);
      FUN_00408a20(iStack_4);
      popmatrix();
      if ((domon != 0) || (DAT_1000b968 != 0)) {
        FUN_00415ea4(&cx,&arry,DAT_1000b978);
        FUN_00410ae4(&arry,0);
        if (domon != 0) {
          FUN_0042b644(oport,&arry,DAT_1000b974);
        }
      }
      if (writeback != 0) {
        FUN_00408964();
      }
      FUN_0041973c();
      iStack_4 = iStack_4 + 1;
    } while (iStack_4 < DAT_1000acb0 + DAT_1000b91c);
  }
  if (domon != 0) {
    FUN_00410ae4(&arry,1);
  }
  return;
}


/* writesamps  entry 0x408958  size 12 bytes */

void writesamps(void)

{
  FUN_0040c1d4(&arry,&x,DAT_1000b974);
  if (DAT_12052708 == 0) {
    FUN_00416938(wafd,DAT_1000b974,&x);
  }
  FUN_00416938(wafd,DAT_1000b974,&x);
  AFsyncfile(wafd);
  DAT_12052708 = 1;
  return;
}


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


/* drawspec  entry 0x409dac  size 12 bytes */

void drawspec(void)

{
  int iVar1;
  undefined4 uVar2;
  undefined4 auStack_420 [256];
  int iStack_20;
  int iStack_1c;
  FILE *pFStack_14;
  int iStack_10;
  int iStack_8;
  int iStack_4;
  
  iStack_8 = 0;
  iStack_1c = DAT_1000b978;
  iStack_20 = 0;
  malloc(0x28);
  mmode(2);
  if (DAT_1000b920 == 1) {
    DAT_1000b978 = 1;
    if (1 < DAT_1000ac8c) {
      do {
        DAT_1000b978 = DAT_1000b978 << 1;
      } while (DAT_1000b978 < DAT_1000ac8c);
    }
    printf("order: %i\n",DAT_1000b978);
  }
  for (; DAT_1000b978 < theParms; DAT_1000b978 = DAT_1000b978 << 1) {
  }
  if (DAT_1000b978 != iStack_1c) {
    FUN_004116a4();
  }
  if (iStack_8 < DAT_1000acb0) {
    while (DAT_1000b90c == 0) {
      if (input == 1) {
        FUN_0040a734();
      }
      else {
        FUN_0040a8ec();
      }
      if (domon != 0) {
        FUN_0042b644(oport,&arry,DAT_1000b974);
      }
      if ((pipe_in == 0) || (import == 0)) {
        if (DAT_1000b92c == 0) {
          if (DAT_1000b990 != 8) {
            FUN_0040be54(&wintbl,&arry,theParms);
          }
          mp = FUN_00415c1c(&arry,DAT_1000b978,&fx,DAT_1000b978);
          FUN_0040c38c(&fx,DAT_1000b97c);
        }
        else {
          iStack_4 = 0;
          if (0 < DAT_1000b978) {
            do {
              FUN_00416f6c(*(undefined4 *)(&arry + iStack_4 * 4));
              uVar2 = FUN_004171cc((float)((uint)(iStack_4 == 0) * 32000));
              auStack_420[iStack_4] = uVar2;
              iStack_4 = iStack_4 + 1;
            } while (iStack_4 < DAT_1000b978);
          }
          mp = FUN_00415c1c(auStack_420,DAT_1000b978,&fx,DAT_1000b978);
          FUN_0040c38c(&fx,DAT_1000b97c);
        }
      }
      else {
        iStack_4 = 0;
        if (0 < DAT_1000b978) {
          do {
            (&fx)[iStack_4] = *(undefined4 *)(&arry + iStack_4 * 4);
            iStack_4 = iStack_4 + 1;
          } while (iStack_4 < DAT_1000b978);
        }
      }
      pushmatrix();
      FUN_0040a3d8(DAT_1000b918 + iStack_8);
      FUN_0040aadc(iStack_8);
      if (DAT_1000acbc != 0) {
        FUN_0040b7d0(iStack_8);
      }
      popmatrix();
      if (soundfiletype == 0) {
        iStack_10 = afd;
      }
      else if (soundfiletype == 1) {
        pFStack_14 = file;
      }
      iStack_8 = iStack_8 + 1;
      iStack_20 = iStack_20 + DAT_1000b974;
      if ((pipe_in == 0) && (input == 3)) {
        if (soundfiletype == 0) {
          iVar1 = FUN_00416990(iStack_10,iStack_20);
          if (iVar1 < 0) {
            perror("seek error!");
                    /* WARNING: Subroutine does not return */
            exit(1);
          }
        }
        else if ((soundfiletype == 1) && (iVar1 = fseek(pFStack_14,iStack_20 * 2,0), iVar1 < 0)) {
          perror("seek error!");
                    /* WARNING: Subroutine does not return */
          exit(1);
        }
      }
      FUN_0041973c();
      if ((soundfiletype == 0) && (iStack_10 != afd)) {
        return;
      }
      if ((soundfiletype == 1) && (pFStack_14 != file)) {
        return;
      }
      if (DAT_1000acb0 <= iStack_8) {
        return;
      }
    }
  }
  return;
}


/* setup_transformation  entry 0x40a3cc  size 12 bytes */

void setup_transformation(int param_1,undefined4 param_2)

{
  longlong lVar1;
  int iVar2;
  
  if (DAT_1000acc0 == 0) {
    ortho(0xbf000000,(float)theScreen - 0.5,param_1,param_2,0xbf000000,(float)DAT_1000ac8c - 0.5,
          0xc47a0000,0x447a0000);
    rotate(900,0x78);
    scale(DAT_1000acf4,0x3f800000);
  }
  else {
    FUN_00411fac();
    rotate(900,0x78);
    if (DAT_1000acb8 == 0) {
      scale(DAT_1000acf4,DAT_1000acf8);
      iVar2 = -DAT_1000acb0;
      if (iVar2 < 0) {
        iVar2 = iVar2 + 1;
      }
      translate((float)((iVar2 >> 1) - DAT_1000b91c),0);
      translate(DAT_1000ace0,DAT_1000ace4);
    }
    else {
      if ((float)DAT_1000b914 * DAT_1000acf4 == 0.0) {
        lVar1 = 0x3fe00000;
      }
      else {
        lVar1 = 0x3ff00000;
      }
      scale((float)(double)(lVar1 << 0x20),DAT_1000acf8);
      rotate(0xfffffc7c,0x79);
    }
  }
  if ((DAT_1000acb8 != 0) && (-1 < param_1)) {
    rotate((int)((float)(param_1 % 0xe10) * DAT_1000acf4),0x79);
  }
  return;
}


/* read_adc_samps  entry 0x40a728  size 12 bytes */

void read_adc_samps(void)

{
  undefined2 auStack_100c [2050];
  int iStack_8;
  int iStack_4;
  
  iStack_8 = theParms - DAT_1000b974;
  if (DAT_100000d0 != 0) {
    FUN_0042bbc4(port,&x + iStack_8 * 2,DAT_1000b980);
    iStack_4 = 0;
    if (0 < iStack_8) {
      do {
        *(undefined2 *)(&x + iStack_4 * 2) = auStack_100c[iStack_4];
        iStack_4 = iStack_4 + 1;
      } while (iStack_4 < iStack_8);
    }
    DAT_1205271c = 0;
    DAT_100000d0 = 0;
  }
  FUN_0040c170(&x + DAT_1205271c * 2,&arry,theParms);
  DAT_1205271c = DAT_1205271c + DAT_1000b974;
  if (DAT_1000b980 + iStack_8 < DAT_1205271c + theParms) {
    DAT_100000d0 = 1;
    iStack_4 = 0;
    if (0 < iStack_8) {
      do {
        auStack_100c[iStack_4] = *(undefined2 *)(&x + (DAT_1205271c + iStack_4) * 2);
        iStack_4 = iStack_4 + 1;
      } while (iStack_4 < iStack_8);
    }
  }
  return;
}


/* read_file_samps  entry 0x40a8e0  size 12 bytes */

void read_file_samps(void)

{
  size_t __n;
  size_t sVar1;
  int iVar2;
  
  if (soundfiletype == 0) {
    FUN_004168e0(afd,theParms,&x);
    if (sfstereo != 0) {
      FUN_0040aa64(&x,theParms);
    }
  }
  else if (soundfiletype == 1) {
    if (sfstereo == 0) {
      iVar2 = 1;
    }
    else {
      iVar2 = 2;
    }
    __n = iVar2 * theParms;
    sVar1 = fread(&x,2,__n,file);
    if (sVar1 != __n) {
      perror("Sound file read error!\n");
                    /* WARNING: Subroutine does not return */
      exit(-1);
    }
    if (sfstereo != 0) {
      FUN_0040aa64(&x,theParms);
    }
  }
  FUN_0040c170(&x,&arry,theParms);
  return;
}


/* mixdown  entry 0x40aa58  size 12 bytes */

void mixdown(int param_1,int param_2)

{
  int iVar1;
  undefined2 uVar2;
  undefined4 uStack_4;
  
  uStack_4 = 0;
  if (0 < param_2) {
    do {
      iVar1 = (int)*(short *)(param_1 + uStack_4 * 4 + 2) + (int)*(short *)(param_1 + uStack_4 * 4);
      uVar2 = (undefined2)(iVar1 >> 1);
      if (iVar1 < 0) {
        uVar2 = (undefined2)(iVar1 + 1 >> 1);
      }
      *(undefined2 *)(param_1 + uStack_4 * 2) = uVar2;
      uStack_4 = uStack_4 + 1;
    } while (uStack_4 < param_2);
  }
  return;
}


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


/* lutlimit  entry 0x40b73c  size 12 bytes */

float lutlimit(float param_1)

{
  if (param_1 < 1.0) {
    param_1 = 1.0;
  }
  else if (254.0 < param_1) {
    param_1 = 254.0;
  }
  return param_1;
}


/* draw_bargraph  entry 0x40b7c4  size 12 bytes */

void draw_bargraph(int param_1)

{
  float fVar1;
  int iStack_10;
  int iStack_c;
  int iStack_8;
  int iStack_4;
  
  iStack_4 = ALgetfilled(port);
  bgnline();
  color(0);
  iStack_10 = (uint)(DAT_1000acb8 == 0) * param_1;
  iStack_c = 0;
  iStack_8 = DAT_1000b97c + 10;
  v3i(&iStack_10);
  fVar1 = (float)iStack_4;
  colorf(fVar1 * 0.00255);
  iStack_c = (int)(fVar1 * 0.00255);
  v3i(&iStack_10);
  endline();
  return;
}


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


/* winmult  entry 0x40be48  size 12 bytes */

void winmult(int param_1,int param_2,int param_3)

{
  int iVar1;
  int iVar2;
  int iVar3;
  
  iVar1 = 0;
  if (0 < param_3) {
    do {
      *(float *)(param_2 + iVar1 * 4) =
           *(float *)(param_2 + iVar1 * 4) * *(float *)(param_1 + iVar1 * 4);
      iVar1 = iVar1 + 1;
    } while (iVar1 < param_3);
  }
  if (DAT_1000b978 != theParms) {
    iVar1 = DAT_1000b978 + theParms;
    if (iVar1 < 0) {
      iVar1 = iVar1 + 1;
    }
    iVar1 = iVar1 >> 1;
    iVar3 = DAT_1000b978 - theParms;
    if (iVar3 < 0) {
      iVar3 = iVar3 + 1;
    }
    iVar2 = param_3;
    if (param_3 < DAT_1000b978) {
      do {
        *(undefined4 *)(param_2 + iVar2 * 4) = 0;
        iVar2 = iVar2 + 1;
      } while (iVar2 < DAT_1000b978);
    }
    for (; iVar3 >> 1 <= iVar1; iVar1 = iVar1 + -1) {
      param_3 = param_3 + -1;
      *(undefined4 *)(param_2 + iVar1 * 4) = *(undefined4 *)(param_2 + param_3 * 4);
    }
    iVar3 = 0;
    iVar1 = DAT_1000b978 - theParms;
    if (iVar1 < 0) {
      iVar1 = iVar1 + 1;
    }
    if (0 < iVar1 >> 1) {
      do {
        *(undefined4 *)(param_2 + iVar3 * 4) = 0;
        iVar1 = DAT_1000b978 + theParms;
        if (iVar1 < 0) {
          iVar1 = iVar1 + 1;
        }
        *(undefined4 *)(param_2 + (iVar3 + (iVar1 >> 1)) * 4) = 0;
        iVar3 = iVar3 + 1;
        iVar1 = DAT_1000b978 - theParms;
        if (iVar1 < 0) {
          iVar1 = iVar1 + 1;
        }
      } while (iVar3 < iVar1 >> 1);
    }
  }
  return;
}


/* winmult2  entry 0x40c044  size 12 bytes */

void winmult2(int param_1,int param_2,int param_3)

{
  int iVar1;
  
  iVar1 = 0;
  if (0 < param_3) {
    do {
      *(float *)(param_2 + iVar1 * 4) =
           *(float *)(param_2 + iVar1 * 4) * *(float *)(param_1 + iVar1 * 4);
      iVar1 = iVar1 + 1;
    } while (iVar1 < param_3);
  }
  return;
}


/* linmult  entry 0x40c0ac  size 32 bytes */

void linmult(int param_1,int param_2)

{
  int iVar1;
  int iVar2;
  
  iVar1 = param_2;
  if (param_2 < 0) {
    iVar1 = param_2 + 1;
  }
  iVar2 = 0;
  if (0 < param_2) {
    do {
      *(float *)(param_1 + iVar2 * 4) =
           *(float *)(param_1 + iVar2 * 4) *
           (1.0 - ABS((float)iVar2 - (float)(iVar1 >> 1)) / (float)(iVar1 >> 1));
      iVar2 = iVar2 + 1;
    } while (iVar2 < param_2);
  }
  return;
}


/* floatit  entry 0x40c164  size 12 bytes */

void floatit(int param_1,int param_2,int param_3)

{
  int iVar1;
  
  iVar1 = 0;
  if (0 < param_3) {
    do {
      *(float *)(param_2 + iVar1 * 4) = (float)(int)*(short *)(param_1 + iVar1 * 2);
      iVar1 = iVar1 + 1;
    } while (iVar1 < param_3);
  }
  return;
}


/* shortit  entry 0x40c1c8  size 12 bytes */

void shortit(int param_1,int param_2,int param_3)

{
  int iVar1;
  
  iVar1 = 0;
  if (0 < param_3) {
    do {
      *(short *)(param_2 + iVar1 * 2) = (short)(int)*(float *)(param_1 + iVar1 * 4);
      iVar1 = iVar1 + 1;
    } while (iVar1 < param_3);
  }
  return;
}


/* findmax  entry 0x40c22c  size 12 bytes */

undefined4 findmax(int param_1)

{
  int iVar1;
  int iVar2;
  float fVar3;
  ulonglong uVar4;
  undefined4 uVar5;
  
  uVar5 = 0xc7c34f80;
  uVar4 = 0xc7c34f80;
  if (DAT_1000b90c == 0) {
    iVar2 = 0;
    if (0 < DAT_1000b97c) {
      do {
        if ((float)uVar4 < (float)(&fx)[iVar2]) {
          uVar4 = (ulonglong)(uint)(&fx)[iVar2];
        }
        uVar5 = (undefined4)uVar4;
        iVar2 = iVar2 + 1;
      } while (iVar2 < DAT_1000b97c);
    }
  }
  else {
    iVar2 = 0;
    if (0 < DAT_1000b97c) {
      do {
        iVar1 = param_1 * 0x2000 + iVar2 * 8;
        fVar3 = (float)FUN_004113d8(*(undefined4 *)(&surface + iVar1),
                                    *(undefined4 *)(&DAT_10048704 + iVar1));
        if ((float)uVar4 < fVar3) {
          uVar4 = (ulonglong)(uint)fVar3;
        }
        uVar5 = (undefined4)uVar4;
        iVar2 = iVar2 + 1;
      } while (iVar2 < DAT_1000b97c);
    }
  }
  return uVar5;
}


/* log_of  entry 0x40c380  size 12 bytes */

void log_of(int param_1,int param_2)

{
  int iVar1;
  float fVar2;
  
  iVar1 = 0;
  if (-1 < param_2) {
    do {
      if (*(float *)(param_1 + iVar1 * 4) < 1e-05) {
        *(undefined4 *)(param_1 + iVar1 * 4) = 0x3727c5ac;
      }
      fVar2 = log10f(*(float *)(param_1 + iVar1 * 4));
      *(float *)(param_1 + iVar1 * 4) = (float)((double)fVar2 * m + b);
      iVar1 = iVar1 + 1;
    } while (iVar1 <= param_2);
  }
  return;
}


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


/* rewindsoundfile  entry 0x40d084  size 12 bytes */

void rewindsoundfile(void)

{
  int iVar1;
  
  if (soundfiletype == 0) {
    iVar1 = FUN_00416990(afd,0);
    if (iVar1 < 0) {
      perror("seek failed!\n");
                    /* WARNING: Subroutine does not return */
      exit(1);
    }
  }
  else if ((soundfiletype == 1) && (iVar1 = fseek(file,0x200,0), iVar1 < 0)) {
    perror("seek failed!\n");
                    /* WARNING: Subroutine does not return */
    exit(1);
  }
  return;
}


/* redraw_everything_and_resize  entry 0x40d16c  size 12 bytes */

void redraw_everything_and_resize(void)

{
  undefined4 uStack_c;
  undefined4 uStack_8;
  
  winset(DAT_1000ac98);
  color(0);
  if (DAT_1000b90c == 0) {
    if (input == 1) {
      uStack_c = 3;
      FUN_0042c1c4(1,&uStack_c,2);
      sampling_rate = uStack_8;
    }
    if (DAT_1000acb8 == 0) {
      clear();
    }
    if (input == 3) {
      FUN_0040d090();
      if (DAT_1000acc0 != 0) {
        FUN_0040c484();
      }
      if (DAT_1000b924 != 0) {
        FUN_00409db8();
      }
      swapbuffers();
    }
    while ((input == 1 && (DAT_1000b90c == 0))) {
      if ((DAT_1000acc0 != 0) && (DAT_1000acb8 == 0)) {
        FUN_004121c0();
        FUN_0040c484();
      }
      FUN_00409db8();
      swapbuffers();
      DAT_1000b918 = DAT_1000b918 + DAT_1000acb0;
      if (0xe0f < DAT_1000b918) {
        DAT_1000b918 = DAT_1000b918 + -0xe10;
        zclear();
      }
    }
  }
  else {
    do {
      if (DAT_1000acc0 != 0) {
        FUN_0040c484();
      }
      if (DAT_1000b924 != 0) {
        FUN_004087ac();
      }
      FUN_0041973c();
      swapbuffers();
    } while ((DAT_1000b964 != 0) && (DAT_1000b90c != 0));
  }
  return;
}


/* pEvent_Handler  entry 0x40d43c  size 204 bytes */

void pEvent_Handler(short param_1,short param_2)

{
  short sVar2;
  int iVar1;
  float local_34;
  float local_30;
  undefined4 local_2c;
  int local_28;
  undefined4 local_24 [4];
  undefined4 local_14;
  undefined4 local_10;
  int local_c;
  int local_4;
  
  local_24[0] = DAT_10000164;
  local_24[1] = DAT_10000168;
  local_24[3] = DAT_10000170;
  local_24[2] = DAT_1000016c;
  local_14 = DAT_10000174;
  local_10 = DAT_10000178;
  local_28 = winget();
  if (local_28 != DAT_1000ac98) {
    return;
  }
  if (DAT_1000b940 == 1) {
    return;
  }
  if (param_1 == 0x65) {
    if (param_2 == 1) {
      if (DAT_1000acc0 == 0) {
        DAT_1000ac40 = DAT_1000ac40 + 1;
        if (2 < DAT_1000ac40) {
          DAT_1000ac40 = 0;
        }
        local_4 = 0;
        if (-1 < DAT_1000acb0) {
          do {
            *(undefined4 *)(&freqarry + local_4 * 0xc + DAT_1000ac40 * 4) = 0;
            local_4 = local_4 + 1;
          } while (local_4 <= DAT_1000acb0);
        }
        FUN_00411a58(DAT_1000ac40,local_c,2);
      }
      FUN_0040d178();
      goto FUN_0040de4c;
    }
  }
  else if (param_1 != 0x66) {
    if (param_1 == 0x67) {
      if (param_2 == 0) {
        function = 0;
        DAT_1000b964 = 0;
        DAT_1000b954 = DAT_1000b954 + DAT_1000b958;
        DAT_1000b958 = 0.0;
      }
      else {
        sVar2 = getvaluator(0x10a);
        DAT_1000b944 = (float)(int)sVar2;
        sVar2 = getvaluator(0x10b);
        DAT_1000b948 = (float)(int)sVar2;
        if (DAT_1000acc0 == 0) {
          function = 0;
          DAT_1000b964 = 0;
          FUN_00411f20();
          pushmatrix();
          frontbuffer(1);
          ortho(0xbf000000,(float)theScreen - 0.5);
          local_34 = DAT_1000b944 - (float)DAT_1000ac90;
          local_30 = DAT_1000b948 - (float)DAT_1000ac94;
          local_2c = 0;
          color(local_24[DAT_1000ac40]);
          bgnline();
          v3f(&DAT_1000ac48);
          v3f(&local_34);
          endline();
          DAT_1000ac48 = local_34;
          DAT_1000ac4c = local_30;
          DAT_1000ac50 = 0;
          local_c = (int)(local_34 / DAT_1000acf4);
          *(float *)(&freqarry + local_c * 0xc + DAT_1000ac40 * 4) = local_30 / 4.0;
          iVar1 = getbutton(0x67);
          while (iVar1 != 0) {
            sVar2 = getvaluator(0x10a);
            DAT_1000b944 = (float)(int)sVar2;
            sVar2 = getvaluator(0x10b);
            DAT_1000b948 = (float)(int)sVar2;
            local_34 = DAT_1000b944 - (float)DAT_1000ac90;
            local_30 = DAT_1000b948 - (float)DAT_1000ac94;
            bgnpoint();
            v3f(&local_34);
            endpoint();
            local_c = (int)(local_34 / DAT_1000acf4);
            *(float *)(&freqarry + local_c * 0xc + DAT_1000ac40 * 4) = local_30 / 4.0;
            iVar1 = getbutton(0x67);
          }
          popmatrix();
          FUN_00411a58(DAT_1000ac40,local_c,1);
        }
        else {
          function = 2;
          DAT_1000b964 = 1;
          DAT_1000b94c = DAT_1000b944;
          DAT_1000b950 = DAT_1000b948;
        }
      }
    }
    goto FUN_0040de4c;
  }
  if (param_2 == 0) {
    function = 0;
    DAT_1000b964 = 0;
    DAT_1000b95c = DAT_1000b95c + DAT_1000b960;
    DAT_1000b960 = 0;
    DAT_1000b95e = DAT_1000b95e + DAT_1000b962;
    DAT_1000b962 = 0;
  }
  else {
    sVar2 = getvaluator(0x10a);
    DAT_1000b944 = (float)(int)sVar2;
    sVar2 = getvaluator(0x10b);
    DAT_1000b948 = (float)(int)sVar2;
    if (DAT_1000acc0 == 0) {
      function = 0;
      DAT_1000b964 = 0;
      FUN_00411f20();
      pushmatrix();
      frontbuffer(1);
      ortho(0xbf000000,(float)theScreen - 0.5);
      local_34 = DAT_1000b944 - (float)DAT_1000ac90;
      local_30 = DAT_1000b948 - (float)DAT_1000ac94;
      local_2c = 0;
      color(0xff);
      bgnpoint();
      v3f(&local_34);
      endpoint();
      DAT_1000ac48 = local_34;
      DAT_1000ac4c = local_30;
      local_c = (int)((DAT_1000b944 - (float)DAT_1000ac90) / DAT_1000acf4);
      *(float *)(&freqarry + local_c * 0xc + DAT_1000ac40 * 4) =
           (DAT_1000b948 - (float)DAT_1000ac94) / 4.0;
      popmatrix();
      FUN_00411a58(DAT_1000ac40,local_c,0);
    }
    else {
      function = 1;
      DAT_1000b964 = 1;
      DAT_1000b94c = DAT_1000b944;
      DAT_1000b950 = DAT_1000b948;
    }
  }
FUN_0040de4c:
  if (((function != 0) && (DAT_1000acc0 != 0)) && (DAT_1000b90c == 0)) {
    FUN_0040c484();
  }
  return;
}


/* clearit  entry 0x40dea8  size 72 bytes */

void clearit(void)

{
  FUN_00412124();
  FUN_0040d178();
  return;
}


/* FileMenu_C  entry 0x40df08  size 112 bytes */

void FileMenu_C(undefined4 param_1)

{
  char *pcVar1;
  int iVar2;
  char acStack_c [8];
  int local_4;
  
  local_4 = fl_get_menu(param_1);
  if (local_4 == 1) {
    fname = malloc(0x32);
    fname = (char *)FUN_0041b93c("Select files","sounds","*.aiff",0);
    if (fname != (char *)0x0) {
      pcVar1 = strstr(fname,".aiff");
      if (pcVar1 != (char *)0x0) {
        soundfiletype = 0;
        afd = FUN_0041678c(fname,&DAT_10000b24);
        if (afd == 0) {
          perror("Sound file error");
                    /* WARNING: Subroutine does not return */
          exit(1);
        }
        FUN_004169e4(afd);
      }
      input = 3;
      FUN_0041c6f4(freezB,"FileIn");
      DAT_1000b90c = 0;
      DAT_1000b91c = 0;
      sprintf(acStack_c,"%i",DAT_1000acb0);
      FUN_0041ecac(lengthInput,acStack_c);
      FUN_0041f800(cursorsl,0,(float)DAT_1000acb0);
      FUN_0041f7b8(cursorsl,0);
      FUN_0041f800(xsl,(float)-DAT_1000acb0,0);
      FUN_0041ecac(mincurInput,&DAT_10000b48);
      FUN_0041ecac(maxcurInput,acStack_c);
      DAT_1000acbc = 0;
      FUN_00420744(bargrphB,0);
    }
  }
  else if (local_4 == 2) {
    input = 1;
    FUN_0041c6f4(freezB,"LiveIn");
    DAT_1000b90c = 0;
    DAT_1000b91c = 0;
    pcVar1 = (char *)fl_get_input(maxcurInput);
    iVar2 = atoi(pcVar1);
    FUN_0041f800(cursorsl,0,(float)iVar2);
    FUN_0041f7b8(cursorsl,0);
    FUN_0041ecac(mincurInput,&DAT_10000b54);
  }
  else if (local_4 == 3) {
    FUN_004115e4();
    sginap(5);
    AFclosefile(wafd);
                    /* WARNING: Subroutine does not return */
    exit(0);
  }
  FUN_0040d178();
  return;
}


/* MapMenu_C  entry 0x40e30c  size 156 bytes */

void MapMenu_C(undefined4 param_1)

{
  int iVar1;
  
  iVar1 = fl_get_menu(param_1);
  if (iVar1 == 1) {
    malloc(0x32);
    iVar1 = FUN_0041b93c("Select files",&DAT_10000b68,"*.map",0);
    if (iVar1 != 0) {
      iVar1 = sproc(loadPal,0xffffff,iVar1);
      if (iVar1 < 0) {
        perror("Oops!");
      }
      else {
        sginap(100);
        mapcolor(0,0,0,0);
        mapcolor(0xff,0xff,0xff,0xff);
      }
    }
  }
  else if (iVar1 == 2) {
    DAT_1000b908 = 0;
  }
  else if (iVar1 == 3) {
    DAT_1000b908 = 1;
  }
  else if (iVar1 == 4) {
    FUN_004115e4();
  }
  return;
}


/* loadPal  entry 0x40e4fc  size 80 bytes */

void loadPal(undefined4 param_1)

{
  execl("/usr/sbin/loadmap","/usr/sbin/loadmap",param_1,0);
  return;
}


/* WinMenu_C  entry 0x40e55c  size 132 bytes */

void WinMenu_C(undefined4 param_1)

{
  int iVar1;
  
  iVar1 = fl_get_menu(param_1);
  iVar1 = iVar1 + -1;
  if ((DAT_1000b98c != 0) && (iVar1 == 1)) {
    perror("Blackman Window NOT valid for use with a Synthesis window!\n");
    ringbell();
    iVar1 = DAT_1000b990;
  }
  DAT_1000b990 = iVar1;
  switch(DAT_1000b990) {
  case 0:
    FUN_0041c6f4(param_1,"ExtBlk");
    break;
  case 1:
    if (DAT_1000b98c == 0) {
      FUN_0041c6f4(param_1,"Blkman");
    }
    break;
  case 2:
    FUN_0041c6f4(param_1,"B-H 1");
    break;
  case 3:
    FUN_0041c6f4(param_1,"B-H 2");
    break;
  case 4:
    FUN_0041c6f4(param_1,"B-H 3");
    break;
  case 5:
    FUN_0041c6f4(param_1,"B-H 4");
    break;
  case 6:
    FUN_0041c6f4(param_1,&DAT_10000bfc);
    break;
  case 7:
    FUN_0041c6f4(param_1,&DAT_10000c04);
    break;
  case 8:
    FUN_0041c6f4(param_1,&DAT_10000c0c);
  }
  FUN_0040b910(&wintbl,DAT_1000b990,theParms);
  FUN_0040d178();
  return;
}


/* SurfMenu_C  entry 0x40e79c  size 256 bytes */

void SurfMenu_C(undefined4 param_1)

{
  size_t sVar1;
  float fVar2;
  float fVar3;
  float local_1054;
  float local_1034;
  float local_1030 [1024];
  void *local_30;
  void *local_2c;
  void *local_28;
  char *local_20;
  char *local_1c;
  FILE *local_18;
  size_t local_10;
  int local_c;
  int local_8;
  int local_4;
  
  local_1034 = 1.0;
  local_28 = malloc(0x800);
  local_2c = malloc(0x800);
  malloc(0x200);
  local_30 = malloc(0x1000);
  local_c = fl_get_menu(param_1);
  if (local_c == 1) {
    local_20 = malloc(0x32);
    local_20 = (char *)FUN_0041b93c("Select files",&DAT_10000c24,"*.filt",0);
    if (local_20 != (char *)0x0) {
      local_10 = strlen(local_20);
      local_1c = malloc(0x32);
      strncpy(local_1c,local_20,local_10);
      local_18 = fopen(local_20,"r");
      if (local_18 == (FILE *)0x0) {
        perror("File error");
                    /* WARNING: Subroutine does not return */
        exit(1);
      }
      local_8 = 0;
      if (0 < DAT_1000acb0) {
        do {
          sVar1 = fread(local_1030,4,DAT_1000b97c + 1,local_18);
          if (sVar1 != DAT_1000b97c + 1U) {
            fclose(local_18);
            return;
          }
          (&flttbl)[local_8 * 0x400] = (double)local_1030[0];
          local_4 = 1;
          if (0 < DAT_1000b97c) {
            do {
              (&flttbl)[local_4 + local_8 * 0x400] = (double)local_1030[local_4];
              (&flttbl)[(DAT_1000b978 - local_4) + local_8 * 0x400] = (double)local_1030[local_4];
              local_4 = local_4 + 1;
            } while (local_4 <= DAT_1000b97c);
          }
          local_8 = local_8 + 1;
        } while (local_8 < DAT_1000acb0);
      }
    }
  }
  else if (local_c == 2) {
    local_20 = malloc(0x32);
    local_20 = (char *)FUN_0041b93c("Select files",&DAT_10000c54,"*.filt",0);
    if (local_20 != (char *)0x0) {
      local_10 = strlen(local_20);
      local_1c = malloc(0x32);
      strncpy(local_1c,local_20,local_10);
      local_18 = fopen(local_1c,"w");
      if (local_18 == (FILE *)0x0) {
        perror("File error");
                    /* WARNING: Subroutine does not return */
        exit(1);
      }
      if ((DAT_1000b984 == 0) && (local_8 = 0, 0 < DAT_1000acb0)) {
        do {
          fVar2 = (float)FUN_0040c238(local_8);
          fVar2 = sqrtf(fVar2);
          if (fVar2 <= local_1034) {
            fVar2 = local_1034;
          }
          local_1034 = fVar2;
          local_8 = local_8 + 1;
        } while (local_8 < DAT_1000acb0);
      }
      local_8 = 0;
      if (0 < DAT_1000acb0) {
        do {
          local_4 = 0;
          if (-1 < DAT_1000b97c) {
            do {
              if (DAT_1000b988 == 0) {
                local_1054 = 1.0;
              }
              else {
                local_1054 = (float)(double)(&flttbl)[local_4 + local_8 * 0x400];
              }
              if (DAT_1000b984 == 0) {
                fVar2 = *(float *)(&surface + local_8 * 0x2000 + local_4 * 8) / local_1034;
              }
              else {
                fVar2 = 1.0;
              }
              if (DAT_1000b984 == 0) {
                fVar3 = *(float *)(&DAT_10048704 + local_8 * 0x2000 + local_4 * 8) / local_1034;
              }
              else {
                fVar3 = 0.0;
              }
              fVar2 = (float)FUN_004113d8(fVar2 * local_1054,fVar3 * local_1054);
              fVar2 = sqrtf(fVar2);
              local_1030[local_4] = fVar2 * 3.0;
              local_4 = local_4 + 1;
            } while (local_4 <= DAT_1000b97c);
          }
          sVar1 = fwrite(local_1030,4,DAT_1000b97c + 1,local_18);
          if (sVar1 != DAT_1000b97c + 1U) {
                    /* WARNING: Subroutine does not return */
            exit(1);
          }
          local_8 = local_8 + 1;
        } while (local_8 < DAT_1000acb0);
      }
    }
  }
  if (local_18 != (FILE *)0x0) {
    fclose(local_18);
  }
  return;
}


/* lSlider_C  entry 0x40eebc  size 152 bytes */

void lSlider_C(void)

{
  DAT_1000acc8 = fl_get_slider_value(lRedSlider);
  DAT_1000accc = fl_get_slider_value(lGreenSlider);
  DAT_1000acd0 = fl_get_slider_value(lBlueSlider);
  if (DAT_1000b908 == 0) {
    FUN_00410578();
  }
  else {
    FUN_004106d8();
  }
  return;
}


/* hSlider_C  entry 0x40ef84  size 152 bytes */

void hSlider_C(void)

{
  DAT_1000acd4 = fl_get_slider_value(hRedSlider);
  DAT_1000acd8 = fl_get_slider_value(hGreenSlider);
  DAT_1000acdc = fl_get_slider_value(hBlueSlider);
  if (DAT_1000b908 == 0) {
    FUN_00410578();
  }
  else {
    FUN_004106d8();
  }
  return;
}


/* cursor_C  entry 0x40f04c  size 84 bytes */

void cursor_C(int param_1)

{
  char *pcVar1;
  float fVar2;
  
  if (DAT_1000b90c == 0) {
    FUN_0041f7b8(cursorsl,0);
  }
  else if ((DAT_1000b940 == 0) && (DAT_1000acc0 != 0)) {
    if (DAT_12052714 == 0) {
      DAT_1205270c = 0;
    }
    if (DAT_12052718 == 0) {
      DAT_12052710 = DAT_1000acb0;
    }
    if (param_1 == maxcurInput) {
      pcVar1 = (char *)fl_get_input(param_1);
      DAT_12052710 = atoi(pcVar1);
      FUN_0041f800(cursorsl,(float)DAT_1205270c,(float)DAT_12052710);
      DAT_12052718 = 1;
    }
    else if (param_1 == mincurInput) {
      pcVar1 = (char *)fl_get_input(param_1);
      DAT_1205270c = atoi(pcVar1);
      FUN_0041f800(cursorsl,(float)DAT_1205270c,(float)DAT_12052710);
      DAT_1000b91c = DAT_1205270c;
      FUN_0041f7b8(cursorsl,(float)DAT_1205270c);
      DAT_12052714 = 1;
    }
    else {
      fVar2 = (float)fl_get_slider_value(param_1);
      DAT_1000b91c = (int)fVar2;
    }
    DAT_1000b940 = 1;
    FUN_0040d178();
    DAT_1000b940 = 0;
  }
  return;
}


/* button_C  entry 0x40f2dc  size 100 bytes */

void button_C(int param_1)

{
  char *__nptr;
  int iVar1;
  
  winset(DAT_1000ac98);
  if (param_1 == synthwinB) {
    DAT_1000b98c = DAT_1000b98c ^ 1;
  }
  if (param_1 == perspB) {
    DAT_1000b914 = DAT_1000b914 ^ 1;
  }
  if (param_1 == lpcenvB) {
    DAT_1000b92c = DAT_1000b92c ^ 1;
  }
  else if (param_1 == freezB) {
    DAT_1000b90c = DAT_1000b90c ^ 1;
    if (DAT_1000b90c == 0) {
      DAT_1000b91c = 0;
      __nptr = (char *)fl_get_input(maxcurInput);
      iVar1 = atoi(__nptr);
      FUN_0041f800(cursorsl,0,(float)iVar1);
      FUN_0041f7b8(cursorsl,0);
      FUN_0041ecac(mincurInput,&DAT_10000c7c);
      if (input == 3) {
        FUN_0041c6f4(freezB,"FileIn");
      }
      else if (input == 1) {
        FUN_0041c6f4(freezB,"LiveIn");
      }
      FUN_0040d178();
    }
    else {
      FUN_0041c6f4(freezB,"Pause");
      winset(DAT_1000ac98);
    }
  }
  else if (param_1 == bargrphB) {
    DAT_1000acbc = DAT_1000acbc ^ 1;
  }
  else if (param_1 == axesB) {
    DAT_1000b93c = DAT_1000b93c ^ 1;
  }
  else if (param_1 == polarB) {
    DAT_1000acb8 = DAT_1000acb8 ^ 1;
    if (DAT_1000acb8 != 0) {
      DAT_1000acc0 = 1;
      FUN_0040c484();
    }
  }
  else if (param_1 == zbB) {
    DAT_1000b938 = DAT_1000b938 ^ 1;
    winset(DAT_1000ac98);
    zbuffer(DAT_1000b938);
    zclear();
  }
  else if (param_1 == dbB) {
    winset(DAT_1000ac98);
    DAT_1000b910 = DAT_1000b910 ^ 1;
    if (DAT_1000b910 == 0) {
      singlebuffer();
      gconfig();
    }
    else {
      doublebuffer();
      gconfig();
    }
  }
  else if (param_1 == tgraphB) {
    DAT_1000b968 = DAT_1000b968 ^ 1;
    if ((DAT_1000aca0 == 0) && (DAT_1000b968 != 0)) {
      DAT_1000aca0 = winopen("Waveform");
    }
    if ((DAT_1000b968 == 0) && (DAT_1000aca0 != 0)) {
      winclose(DAT_1000aca0);
      DAT_1000aca0 = 0;
    }
  }
  else if (param_1 == modB) {
    DAT_1000b988 = DAT_1000b988 ^ 1;
  }
  else if (param_1 == monitorB) {
    domon = domon ^ 1;
  }
  else if (param_1 == logfreqB) {
    DAT_1000acc4 = DAT_1000acc4 ^ 1;
    FUN_004121c0();
    if (DAT_1000acc0 != 0) {
      FUN_0040c484();
    }
  }
  else if (param_1 == draw3dB) {
    DAT_1000acc0 = DAT_1000acc0 ^ 1;
    FUN_004121c0();
    if (DAT_1000acc0 != 0) {
      FUN_0040c484();
    }
  }
  else if (param_1 == sourceB) {
    DAT_1000b984 = DAT_1000b984 ^ 1;
  }
  return;
}


/* shape_C  entry 0x40f8c4  size 112 bytes */

void shape_C(void)

{
  if (DAT_1000acc0 == 0) {
    DAT_1000acac = 4;
  }
  else if (DAT_1000acac == 2) {
    DAT_1000acac = 3;
    FUN_0041c6f4(drawB,&DAT_10000c9c);
  }
  else if (DAT_1000acac == 3) {
    DAT_1000acac = 4;
    FUN_0041c6f4(drawB,&DAT_10000ca4);
  }
  else if (DAT_1000acac == 4) {
    DAT_1000acac = 1;
    FUN_0041c6f4(drawB,&DAT_10000cac);
  }
  else if (DAT_1000acac == 1) {
    DAT_1000acac = 2;
    FUN_0041c6f4(drawB,"Point");
  }
  return;
}


/* length_C  entry 0x40fa1c  size 168 bytes */

void length_C(void)

{
  char *__nptr;
  
  __nptr = (char *)fl_get_input(lengthInput);
  DAT_1000acb0 = atoi(__nptr);
  FUN_0041f800(xsl,(float)-DAT_1000acb0,0);
  FUN_0040d178();
  return;
}


/* Input_C  entry 0x40fae0  size 132 bytes */

void Input_C(int param_1)

{
  int iVar1;
  char *pcVar2;
  char acStack_4 [4];
  
  if (param_1 == orderInput) {
    pcVar2 = (char *)fl_get_input(param_1);
    DAT_1000b978 = atoi(pcVar2);
    iVar1 = DAT_1000b978;
    if (DAT_1000b978 < 0) {
      iVar1 = DAT_1000b978 + 1;
    }
    DAT_1000b97c = iVar1 >> 1;
  }
  else if (param_1 == winsizeInput) {
    pcVar2 = (char *)fl_get_input(param_1);
    theParms = atoi(pcVar2);
  }
  else if (param_1 == strideInput) {
    pcVar2 = (char *)fl_get_input(param_1);
    DAT_1000b974 = atoi(pcVar2);
    if (afd != 0) {
      FUN_004169e4(afd);
      iVar1 = FUN_00416990(afd,0);
      if (iVar1 < 0) {
        perror("seek failed!\n");
                    /* WARNING: Subroutine does not return */
        exit(1);
      }
      sprintf(acStack_4,"%i",DAT_1000acb0);
      FUN_0041ecac(lengthInput,acStack_4);
    }
  }
  FUN_0040b910(&wintbl,DAT_1000b990,theParms);
  return;
}


/* viewsl_C  entry 0x40fce8  size 56 bytes */

void viewsl_C(void)

{
  float fVar1;
  
  if ((DAT_1000b940 == 0) && (DAT_1000acc0 != 0)) {
    DAT_1000ace0 = fl_get_slider_value(xsl);
    DAT_1000ace4 = fl_get_slider_value(ysl);
    DAT_1000ace8 = fl_get_slider_value(zsl);
    DAT_1000acec = fl_get_slider_value(fovsl);
    fVar1 = (float)fl_get_slider_value(twistsl);
    DAT_1000acf0 = (fVar1 * 2.0 + -1.0) * 900.0;
    if (DAT_1000b90c != 0) {
      DAT_1000b940 = 1;
      FUN_0040d178();
      DAT_1000b940 = 0;
    }
  }
  return;
}


/* scale_C  entry 0x40fe4c  size 56 bytes */

void scale_C(int param_1)

{
  float fVar1;
  
  if (DAT_1000b940 == 0) {
    if (param_1 == zscalesl) {
      fVar1 = (float)fl_get_slider_value(param_1);
      DAT_1000acfc = fVar1 * 2.0;
    }
    else if (param_1 == yscalesl) {
      fVar1 = (float)fl_get_slider_value(param_1);
      DAT_1000acf8 = fVar1 * 0.5;
    }
    else if (param_1 == xscalesl) {
      DAT_1000acf4 = fl_get_slider_value(param_1);
    }
    if (DAT_1000b90c != 0) {
      DAT_1000b940 = 1;
    }
    FUN_0040d178();
    DAT_1000b940 = 0;
  }
  return;
}


/* gainsl_C  entry 0x40ff94  size 60 bytes */

void gainsl_C(undefined4 param_1)

{
  float fVar1;
  float fVar2;
  
  if (DAT_1000b940 == 0) {
    fVar1 = (float)fl_get_slider_value(param_1);
    DAT_1000ad00 = fVar1 * 40.0;
    fVar1 = log10f((float)largest);
    fVar2 = log10f((float)smallest);
    m = ((double)DAT_1000ad00 * 12.75 - (double)DAT_1000ad04) / (double)(fVar1 - fVar2);
    b = (double)DAT_1000ad04 + m * 6.0;
    if (DAT_1000b90c != 0) {
      DAT_1000b940 = 1;
      FUN_0040d178();
      DAT_1000b940 = 0;
    }
  }
  return;
}


/* floorsl_C  entry 0x4100f8  size 60 bytes */

void floorsl_C(undefined4 param_1)

{
  float fVar1;
  float fVar2;
  
  if (DAT_1000b940 == 0) {
    DAT_1000ad04 = (float)fl_get_slider_value(param_1);
    fVar1 = log10f((float)largest);
    fVar2 = log10f((float)smallest);
    m = ((double)DAT_1000ad00 * 12.75 - (double)DAT_1000ad04) / (double)(fVar1 - fVar2);
    b = (double)DAT_1000ad04 + m * 6.0;
    if (DAT_1000b90c != 0) {
      DAT_1000b940 = 1;
      FUN_0040d178();
      DAT_1000b940 = 0;
    }
  }
  return;
}


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


/* ola  entry 0x410904  size 68 bytes */

void ola(int param_1)

{
  int iVar1;
  int iVar2;
  int iVar3;
  
  _sbss = _sbss ^ 1;
  if (DAT_1000b974 != theParms) {
    iVar2 = 0;
    iVar3 = DAT_1000b978;
    if (DAT_1000b978 < 0) {
      iVar3 = DAT_1000b978 + 1;
    }
    iVar3 = iVar3 >> 1;
    if (0 < iVar3) {
      do {
        if (_sbss == 0) {
          (&DAT_10006438)[iVar2] = *(undefined4 *)(param_1 + iVar3 * 4);
        }
        else {
          (&DAT_10004438)[iVar2] = *(undefined4 *)(param_1 + iVar3 * 4);
        }
        iVar2 = iVar2 + 1;
        iVar3 = iVar3 + 1;
        iVar1 = DAT_1000b978;
        if (DAT_1000b978 < 0) {
          iVar1 = DAT_1000b978 + 1;
        }
      } while (iVar2 < iVar1 >> 1);
    }
    if (_sbss == 0) {
      iVar3 = 0;
      if (0 < DAT_1000b974) {
        do {
          *(float *)(param_1 + iVar3 * 4) =
               *(float *)(param_1 + iVar3 * 4) + (float)(&DAT_10004438)[iVar3];
          iVar3 = iVar3 + 1;
        } while (iVar3 < DAT_1000b974);
      }
    }
    else {
      iVar3 = 0;
      if (0 < DAT_1000b974) {
        do {
          *(float *)(param_1 + iVar3 * 4) =
               *(float *)(param_1 + iVar3 * 4) + (float)(&DAT_10006438)[iVar3];
          iVar3 = iVar3 + 1;
        } while (iVar3 < DAT_1000b974);
      }
    }
  }
  return;
}


/* olap  entry 0x410ad8  size 12 bytes */

void olap(int param_1,int param_2)

{
  int iVar1;
  int iVar2;
  float fStack_1c;
  float fStack_18;
  float fStack_14;
  float fStack_10;
  float fStack_c;
  
  if (param_2 == 0) {
    if (DAT_1000b968 != 0) {
      winset(DAT_1000aca0);
      pushmatrix();
      getsize(&theScreen,&DAT_1000ac8c);
      viewport(0,theScreen + -1,0,DAT_1000ac8c + -1);
      ortho(0xbf000000,(float)theScreen - 0.5);
      fStack_14 = (float)DAT_1000ac8c / 196608.0;
      color(0);
      clear();
      color(0xff);
    }
    if ((DAT_1000b98c == 0) && (DAT_1000b974 == theParms)) {
      if (DAT_1000b968 != 0) {
        bgnline();
        iVar1 = 0;
        if (0 < DAT_1000b978) {
          do {
            fStack_1c = (float)iVar1;
            fStack_18 = *(float *)(param_1 + iVar1 * 4) * 3.0 * fStack_14 +
                        (float)DAT_1000ac8c * 0.5;
            v2f(&fStack_1c);
            iVar1 = iVar1 + 1;
          } while (iVar1 < DAT_1000b978);
        }
        endline();
        popmatrix();
        winset(DAT_1000ac98);
        getsize(&theScreen,&DAT_1000ac8c);
      }
    }
    else {
      fStack_c = (float)DAT_1000b974 / (float)theParms;
      fStack_10 = fStack_c;
      if (1.0 < fStack_c) {
        fStack_10 = 1.0;
      }
      if ((DAT_1000b98c != 0) && (DAT_1000b990 != 8)) {
        iVar1 = DAT_1000b978 - theParms;
        if (iVar1 < 0) {
          iVar1 = iVar1 + 1;
        }
        FUN_0040c050(&wintbl,(iVar1 >> 1) * 4 + param_1,theParms);
        iVar2 = 0;
        iVar1 = DAT_1000b978 - theParms;
        if (iVar1 < 0) {
          iVar1 = iVar1 + 1;
        }
        if (0 < iVar1 >> 1) {
          do {
            *(undefined4 *)(param_1 + iVar2 * 4) = 0;
            *(undefined4 *)(param_1 + (DAT_1000b978 - iVar2) * 4 + -4) = 0;
            iVar2 = iVar2 + 1;
            iVar1 = DAT_1000b978 - theParms;
            if (iVar1 < 0) {
              iVar1 = iVar1 + 1;
            }
          } while (iVar2 < iVar1 >> 1);
        }
      }
      if (DAT_1000b968 != 0) {
        bgnline();
        iVar1 = 0;
        if (0 < DAT_1000b978 + DAT_1000b974) {
          do {
            fStack_1c = (float)iVar1;
            fStack_18 = (float)(&DAT_10008438)[iVar1] * fStack_14 + (float)DAT_1000ac8c * 0.5;
            v2f(&fStack_1c);
            iVar1 = iVar1 + 1;
          } while (iVar1 < DAT_1000b978 + DAT_1000b974);
        }
        endline();
      }
      iVar1 = 0;
      if (0 < DAT_1000b978) {
        do {
          (&DAT_10008438)[DAT_1000b974 + iVar1] =
               (float)(&DAT_10008438)[DAT_1000b974 + iVar1] + *(float *)(param_1 + iVar1 * 4);
          iVar1 = iVar1 + 1;
        } while (iVar1 < DAT_1000b978);
      }
      if (DAT_1000b968 != 0) {
        bgnline();
        iVar1 = 0;
        if (0 < DAT_1000b978) {
          do {
            fStack_1c = (float)(iVar1 + DAT_1000b974);
            fStack_18 = *(float *)(param_1 + iVar1 * 4) * fStack_14 + (float)DAT_1000ac8c * 0.75;
            v2f(&fStack_1c);
            iVar1 = iVar1 + 1;
          } while (iVar1 < DAT_1000b978);
        }
        endline();
        bgnline();
        iVar1 = 0;
        if (0 < DAT_1000b974) {
          do {
            fStack_1c = (float)iVar1;
            fStack_18 = (float)(&DAT_10008438)[iVar1] * fStack_14 + (float)DAT_1000ac8c * 0.25;
            v2f(&fStack_1c);
            iVar1 = iVar1 + 1;
          } while (iVar1 < DAT_1000b974);
        }
        endline();
        popmatrix();
        winset(DAT_1000ac98);
        getsize(&theScreen,&DAT_1000ac8c);
      }
      iVar1 = 0;
      if (0 < DAT_1000b974) {
        do {
          *(float *)(param_1 + iVar1 * 4) = (float)(&DAT_10008438)[iVar1] * fStack_10;
          iVar1 = iVar1 + 1;
        } while (iVar1 < DAT_1000b974);
      }
      iVar1 = 0;
      if (0 < DAT_1000b978) {
        do {
          (&DAT_10008438)[iVar1] = (&DAT_10008438)[iVar1 + DAT_1000b974];
          iVar1 = iVar1 + 1;
        } while (iVar1 < DAT_1000b978);
      }
      iVar1 = 0;
      if (0 < DAT_1000b974) {
        do {
          (&DAT_10008438)[iVar1 + DAT_1000b978] = 0;
          iVar1 = iVar1 + 1;
        } while (iVar1 < DAT_1000b974);
      }
    }
  }
  else {
    iVar1 = 0;
    if (0 < DAT_1000b974 + DAT_1000b978) {
      do {
        (&DAT_10008438)[iVar1] = 0;
        iVar1 = iVar1 + 1;
      } while (iVar1 < DAT_1000b974 + DAT_1000b978);
    }
  }
  return;
}


/* mag2  entry 0x4113cc  size 12 bytes */

float mag2(float param_1,float param_2)

{
  return param_1 * param_1 + param_2 * param_2;
}


/* magl  entry 0x411408  size 12 bytes */

double magl(float param_1,float param_2)

{
  float in_f0;
  float __x;
  
  __x = (param_1 * param_1 + param_2 * param_2) / (float)(DAT_1000b978 * DAT_1000b978);
  if (__x < 1e-05) {
    __x = 1e-05;
  }
  log10f(__x);
  return (double)in_f0 * m + b;
}


/* init_lut  entry 0x4114e0  size 12 bytes */

void init_lut(void)

{
  short sStack_10;
  short sStack_e;
  short sStack_c;
  short sStack_a;
  short sStack_8;
  short sStack_6;
  int iStack_4;
  
  iStack_4 = 0;
  do {
    sStack_a = (short)iStack_4;
    sStack_8 = sStack_a;
    sStack_6 = sStack_a;
    getmcolor(iStack_4,&sStack_c,&sStack_e,&sStack_10);
    mapcolor(iStack_4,(int)sStack_6,(int)sStack_8,(int)sStack_a);
    *(int *)(&DAT_1000ad08 + iStack_4 * 0xc) = (int)sStack_c;
    *(int *)(&DAT_1000ad0c + iStack_4 * 0xc) = (int)sStack_e;
    *(int *)(&DAT_1000ad10 + iStack_4 * 0xc) = (int)sStack_10;
    iStack_4 = iStack_4 + 1;
  } while (iStack_4 < 0x100);
  return;
}


/* restore_lut  entry 0x4115d8  size 12 bytes */

void restore_lut(void)

{
  int iVar1;
  int iStack_4;
  
  iStack_4 = 0;
  do {
    iVar1 = iStack_4 * 0xc;
    mapcolor(iStack_4,*(undefined4 *)(&DAT_1000ad08 + iVar1),*(undefined4 *)(&DAT_1000ad0c + iVar1),
             *(undefined4 *)(&DAT_1000ad10 + iVar1));
    iStack_4 = iStack_4 + 1;
  } while (iStack_4 < 0x100);
  winset(DAT_1000ac98);
  mmode(2);
  getsize(&theScreen,&DAT_1000ac8c);
  return;
}


/* init_logftbl  entry 0x411698  size 12 bytes */

void init_logftbl(void)

{
  float fVar1;
  float fVar2;
  float fVar3;
  int iStack_8;
  
  fVar1 = logf((float)DAT_1000b97c);
  fVar3 = (float)DAT_1000b97c;
  iStack_8 = 0;
  if (-1 < DAT_1000b97c) {
    do {
      fVar2 = logf((float)(iStack_8 + 1));
      (&zlogpos)[iStack_8] = fVar2 * (fVar3 / fVar1);
      iStack_8 = iStack_8 + 1;
    } while (iStack_8 <= DAT_1000b97c);
  }
  return;
}


/* init_flttbl  entry 0x41176c  size 12 bytes */

void init_flttbl(void)

{
  double adStack_2028 [1024];
  void *pvStack_28;
  void *pvStack_24;
  double dStack_20;
  undefined8 uStack_18;
  int iStack_c;
  int iStack_8;
  int iStack_4;
  
  uStack_18 = 0x3f589374bc6a7efa;
  pvStack_24 = malloc(0x800);
  pvStack_28 = malloc(0x800);
  malloc(0x200);
  malloc(0x1000);
  dStack_20 = (double)sampling_rate;
  iStack_8 = 0;
  if (-1 < DAT_1000acb4) {
    do {
      iStack_c = 0;
      do {
        FUN_00416c84(adStack_2028,DAT_1000b97c,*(undefined4 *)(&foffreq + iStack_c),
                     *(undefined4 *)((int)&foffreq + iStack_c * 8 + 4),0x3ff0000000000000,
                     (&fofbw)[iStack_c],uStack_18,dStack_20);
        (&flttbl)[iStack_8 * 0x400] = (double)(&flttbl)[iStack_8 * 0x400] + adStack_2028[0];
        (&flttbl)[DAT_1000b97c + iStack_8 * 0x400] =
             (double)(&flttbl)[DAT_1000b97c + iStack_8 * 0x400] + adStack_2028[DAT_1000b97c];
        iStack_4 = 1;
        if (1 < DAT_1000b97c) {
          do {
            (&flttbl)[iStack_4 + iStack_8 * 0x400] =
                 (double)(&flttbl)[iStack_4 + iStack_8 * 0x400] + adStack_2028[iStack_4];
            (&flttbl)[(DAT_1000b978 - iStack_4) + iStack_8 * 0x400] =
                 (&flttbl)[iStack_4 + iStack_8 * 0x400];
            iStack_4 = iStack_4 + 1;
          } while (iStack_4 < DAT_1000b97c);
        }
        iStack_c = iStack_c + 1;
      } while (iStack_c < 1);
      foffreq = foffreq - 1.0;
      DAT_10000048 = DAT_10000048 - 1.5;
      DAT_10000050 = DAT_10000050 - 7.0;
      iStack_8 = iStack_8 + 1;
    } while (iStack_8 <= DAT_1000acb4);
  }
  return;
}


/* drw_flttbl  entry 0x411a4c  size 12 bytes */

/* WARNING: Heritage AFTER dead removal. Example location: r0x1000acb0 : 0x00411ee4 */
/* WARNING: Restarted to delay deadcode elimination for space: ram */

void drw_flttbl(int param_1,int param_2,int param_3)

{
  int iVar1;
  int iVar2;
  int iStackX_0;
  double adStack_2050 [1024];
  double dStack_50;
  double dStack_48;
  undefined8 uStack_38;
  float fStack_2c;
  float fStack_28;
  float fStack_24;
  int iStack_14;
  int iStack_10;
  int iStack_c;
  int iStack_8;
  int iStack_4;
  
  uStack_38 = 0x3f589374bc6a7efa;
  dStack_50 = (double)sampling_rate;
  if (DAT_1000b978 == 0) {
    trap(0x1c00);
  }
  if ((DAT_1000b978 == -1) && (sampling_rate == -0x80000000)) {
    trap(0x1800);
  }
  dStack_48 = (double)(sampling_rate / DAT_1000b978);
  if (param_3 == 0) {
    DAT_1000ac3c = *(float *)(&freqarry + param_2 * 0xc + param_1 * 4);
    DAT_1000ac38 = param_2;
  }
  else {
    iVar1 = DAT_1000ac38;
    if (param_3 == 1) {
      iStack_14 = param_2 - DAT_1000ac38;
      fStack_2c = *(float *)(&freqarry + param_2 * 0xc + param_1 * 4);
      fStack_28 = fStack_2c - DAT_1000ac3c;
      fStack_24 = fStack_28 / (float)iStack_14;
      iStack_c = 0;
      for (iVar2 = DAT_1000ac38; iStack_10 = param_2, iVar1 = param_2, DAT_1000ac3c = fStack_2c,
          iVar2 <= param_2; iVar2 = iVar2 + 1) {
        *(float *)(&freqarry + iVar2 * 0xc + param_1 * 4) =
             *(float *)(&freqarry + DAT_1000ac38 * 0xc + param_1 * 4) + (float)iStack_c * fStack_24;
        iStack_c = iStack_c + 1;
      }
    }
    DAT_1000ac38 = iVar1;
    iStack_8 = 0;
    if (-1 < DAT_1000acb0) {
      do {
        iStack_4 = 0;
        if (-1 < DAT_1000b97c) {
          do {
            (&flttbl)[iStack_4 + iStack_8 * 0x400] = 0;
            iStack_4 = iStack_4 + 1;
          } while (iStack_4 <= DAT_1000b97c);
        }
        iStackX_0 = 0;
        do {
          if (*(float *)(&freqarry + iStack_8 * 0xc + iStackX_0 * 4) != 0.0) {
            FUN_00416c84(adStack_2050,DAT_1000b97c + 1,
                         (int)((ulonglong)
                               ((double)*(float *)(&freqarry + iStack_8 * 0xc + iStackX_0 * 4) *
                               dStack_48) >> 0x20),
                         SUB84((double)*(float *)(&freqarry + iStack_8 * 0xc + iStackX_0 * 4) *
                               dStack_48,0),*(undefined8 *)(&fofamp + iStackX_0 * 8),
                         (&fofbw)[iStackX_0],uStack_38,dStack_50);
            (&flttbl)[iStack_8 * 0x400] = (double)(&flttbl)[iStack_8 * 0x400] + adStack_2050[0];
            (&flttbl)[DAT_1000b97c + iStack_8 * 0x400] =
                 (double)(&flttbl)[DAT_1000b97c + iStack_8 * 0x400] + adStack_2050[DAT_1000b97c];
            iStack_4 = 1;
            if (1 < DAT_1000b97c) {
              do {
                (&flttbl)[iStack_4 + iStack_8 * 0x400] =
                     (double)(&flttbl)[iStack_4 + iStack_8 * 0x400] + adStack_2050[iStack_4];
                (&flttbl)[(DAT_1000b978 - iStack_4) + iStack_8 * 0x400] =
                     (&flttbl)[iStack_4 + iStack_8 * 0x400];
                iStack_4 = iStack_4 + 1;
              } while (iStack_4 < DAT_1000b97c);
            }
          }
          iStackX_0 = iStackX_0 + 1;
        } while (iStackX_0 < 3);
        iStack_8 = iStack_8 + 1;
      } while (iStack_8 <= DAT_1000acb0);
    }
  }
  return;
}


/* setup_viewport  entry 0x411f14  size 12 bytes */

void setup_viewport(void)

{
  getorigin(&DAT_1000ac90,&DAT_1000ac94);
  getsize(&theScreen,&DAT_1000ac8c);
  viewport(0,theScreen + -1,0,DAT_1000ac8c + -1);
  return;
}


/* setup_projection  entry 0x411fa0  size 12 bytes */

void setup_projection(undefined4 param_1,undefined4 param_2)

{
  if (DAT_1000b914 == 0) {
    ortho((float)(-theScreen / 10),(float)(theScreen / 10),param_1,param_2,
          (float)(-DAT_1000ac8c / 10),(float)(DAT_1000ac8c / 10),0xc47a0000,0x447a0000);
  }
  else {
    perspective((int)DAT_1000acec,0x3faaaaab,(float)DAT_1000aca4,(float)DAT_1000aca8);
  }
  polarview(DAT_1000b954 + DAT_1000b958);
  return;
}


/* clearem  entry 0x412118  size 12 bytes */

void clearem(void)

{
  undefined4 uVar1;
  
  uVar1 = ALgetfilled(port);
  FUN_0042bbc4(port,&x,uVar1);
  FUN_004121c0();
  if (DAT_1000b968 != 0) {
    FUN_00412240();
  }
  return;
}


/* clearwin  entry 0x4121b4  size 12 bytes */

void clearwin(void)

{
  winset(DAT_1000ac98);
  color(0);
  clear();
  zclear();
  return;
}


/* cleartwin  entry 0x412234  size 12 bytes */

void cleartwin(void)

{
  winset(DAT_1000aca0);
  color(0);
  clear();
  winset(DAT_1000ac98);
  return;
}


/* clearfwin  entry 0x4122b8  size 108 bytes */

void clearfwin(void)

{
  winset(DAT_1000ac9c);
  color(0);
  clear();
  winset(DAT_1000ac98);
  return;
}


/* p_init  entry 0x41233c  size 12 bytes */

void p_init(void)

{
  int iVar1;
  undefined4 uVar2;
  float fVar3;
  float fVar4;
  undefined4 uVar5;
  char acStack_4 [4];
  
  FUN_00420b50();
  fminit();
  iVar1 = fmfindfont("Times-Roman");
  if (iVar1 != 0) {
    uVar2 = fmscalefont(iVar1);
    fmsetfont(uVar2);
    FUN_00415bd8();
    FUN_00417a98(Menu_Form,100,100);
    FUN_00417b04(Menu_Form,5,0,0);
    FUN_00420744(bargrphB,DAT_1000acbc);
    FUN_00420744(dbB,DAT_1000b910);
    FUN_00420744(zbB,DAT_1000b938);
    FUN_00420744(lpcenvB,DAT_1000b92c);
    FUN_00420744(synthwinB,DAT_1000b98c);
    FUN_0041f7b8(cursorsl,0);
    FUN_0041f800(cursorsl,0,(float)DAT_1000acb4);
    FUN_0041f800(xsl,(float)-DAT_1000acb4,0);
    FUN_0041f800(ysl,0xc3fa0000,0x43fa0000);
    FUN_0041f800(zsl,0xc4000000,0x44000000);
    FUN_0041f800(xscalesl,0xbf800000,0x40800000);
    FUN_0041f7b8(xscalesl,0x3f800000);
    FUN_0041f800(fovsl,0x40000000,0x447a0000);
    FUN_0041f7b8(fovsl,0x43cd0000);
    FUN_0041f800(floorsl,0xc43b8000,0x437a0000);
    FUN_0041f7b8(floorsl,0xc1a00000);
    sprintf(acStack_4,"%i",DAT_1000acb0);
    FUN_0041ecac(lengthInput,acStack_4);
    sprintf(acStack_4,"%i",DAT_1000b978);
    FUN_0041ecac(orderInput,acStack_4);
    sprintf(acStack_4,"%i",theParms);
    FUN_0041ecac(winsizeInput,acStack_4);
    sprintf(acStack_4,"%i",DAT_1000b974);
    FUN_0041ecac(strideInput,acStack_4);
    FUN_0041ecac(mincurInput,&DAT_10000d60);
    sprintf(acStack_4,"%i",DAT_1000acb0);
    FUN_0041ecac(maxcurInput,acStack_4);
    switch(DAT_1000b990) {
    case 0:
      FUN_0041c6f4(WinMenu,"ExtBlk");
      break;
    case 1:
      if (DAT_1000b98c == 0) {
        FUN_0041c6f4(WinMenu,"Blkman");
      }
      else {
        perror("Blackman Window NOT valid for use with a Synthesis window!\n");
        ringbell();
      }
      break;
    case 2:
      FUN_0041c6f4(WinMenu,"B-H 1");
      break;
    case 3:
      FUN_0041c6f4(WinMenu,"B-H 2");
      break;
    case 4:
      FUN_0041c6f4(WinMenu,"B-H 3");
      break;
    case 5:
      FUN_0041c6f4(WinMenu,"B-H 4");
      break;
    case 6:
      FUN_0041c6f4(WinMenu,&DAT_10000dd4);
      break;
    case 7:
      FUN_0041c6f4(WinMenu,&DAT_10000ddc);
      break;
    case 8:
      FUN_0041c6f4(WinMenu,&DAT_10000de4);
    }
    DAT_1000ac98 = winopen("3D Spectrogram Display");
    if (DAT_1000b968 != 0) {
      DAT_1000aca0 = winopen("Waveform");
    }
    drawmode(0x10);
    glcompat(1,0);
    zfunction(3);
    cmode();
    if (DAT_1000b910 != 0) {
      doublebuffer();
    }
    gconfig();
    FUN_004114ec();
    getplanes();
    FUN_00412124();
    concave(1);
    if (DAT_1000acc0 == 0) {
      DAT_1000acac = 4;
    }
    else {
      frontbuffer(1);
      zbuffer(DAT_1000b938);
      zclear();
      frontbuffer(0);
    }
    FUN_004211b4(0x67);
    FUN_004211b4(0x66);
    FUN_004211b4(0x65);
    FUN_004211b4(0x114);
    FUN_004211b4(0x115);
    FUN_004211b4(0x116);
    FUN_004211b4(0x117);
    FUN_004211b4(0x118);
    FUN_004211b4(0x119);
    FUN_004211b4(0x119);
    FUN_004211b4(0x11a);
    FUN_004211b4(0xb7);
    FUN_004211b4(0xb8);
    FUN_004211b4(0xb9);
    FUN_004211b4(0xba);
    FUN_004211b4(0xbb);
    FUN_004211b4(0xbc);
    FUN_004211b4(0xbd);
    FUN_004211b4(0xbe);
    FUN_004211b4(0xb6);
    FUN_00420f90(pEvent_Handler);
    FUN_0041a200(FileMenu,"Open|LiveInput|Quit");
    FUN_0041a200(MapMenu,"Open|Interpolate|SineColors|RestorePalette");
    FUN_0041a200(WinMenu,
                 "Exact Blackman|Blackman|Blackman-Harris 1|Blackman-Harris 2|Blackman-Harris 3|Blackman-Harris 4|Hamming|Hanning|None"
                );
    FUN_0041a200(SurfMenu,"Load|Save");
    winset(DAT_1000ac98);
    mmode(2);
    getsize(&theScreen,&DAT_1000ac8c);
    viewport(0,theScreen + -1,0,DAT_1000ac8c + -1);
    uVar2 = 0xc47a0000;
    uVar5 = 0x447a0000;
    ortho(0xbf000000,(float)theScreen - 0.5);
    if (DAT_1000acc0 != 0) {
      if (DAT_1000b914 == 0) {
        ortho((float)(-theScreen / 10),(float)(theScreen / 10));
      }
      else {
        perspective((int)DAT_1000acec,0x3faaaaab,(float)DAT_1000aca4,(float)DAT_1000aca8,uVar2,uVar5
                   );
      }
    }
    FUN_0040b910(&wintbl,DAT_1000b990,theParms);
    FUN_004116a4();
    FUN_00411778();
    fVar3 = log10f((float)largest);
    fVar4 = log10f((float)smallest);
    m = ((double)DAT_1000ad00 * 12.75 - (double)DAT_1000ad04) / (double)(fVar3 - fVar4);
    b = (double)DAT_1000ad04 + m * 6.0;
    if (pipe_in == 1) {
      file = (FILE *)&DAT_0fb528e4;
      if (import != 0) {
        fread(&tlength,2,1,(FILE *)&DAT_0fb528e4);
        fread(&torder,2,1,file);
        DAT_1000acb0 = (int)tlength;
        DAT_1000b97c = (int)torder;
        DAT_1000b978 = (int)torder << 1;
      }
      FUN_004116a4();
    }
    return;
  }
                    /* WARNING: Subroutine does not return */
  exit(1);
}


/* init_the_screen  entry 0x41301c  size 12 bytes */

void init_the_screen(void)

{
  DAT_1000aca4 = 0;
  DAT_1000aca8 = 2000;
  DAT_1000acac = 4;
  DAT_1000acb0 = 500;
  DAT_1000acb4 = 1000;
  DAT_1000acb8 = 0;
  DAT_1000acbc = 1;
  DAT_1000acc0 = 1;
  DAT_1000acc4 = 0;
  DAT_1000acec = 0x43cd0000;
  DAT_1000acf0 = 0;
  DAT_1000acf4 = 0x3f800000;
  DAT_1000acf8 = 0x3e4ccccd;
  DAT_1000acfc = 0x3f800000;
  DAT_1000ad00 = 0x41a00000;
  DAT_1000ad04 = 0xc2f00000;
  DAT_1000b908 = 0;
  DAT_1000b90c = 0;
  DAT_1000b914 = 1;
  DAT_1000b920 = 0;
  DAT_1000b924 = 0;
  DAT_1000b928 = 0;
  DAT_1000b92c = 0;
  DAT_1000b930 = 0;
  DAT_1000b934 = 0;
  DAT_1000b938 = 0;
  DAT_1000b93c = 0;
  DAT_1000b940 = 0;
  DAT_1000b954 = 0x44000000;
  DAT_1000b958 = 0;
  DAT_1000b95c = 0x3a2;
  DAT_1000b95e = 700;
  DAT_1000b960 = 0;
  DAT_1000b962 = 0;
  DAT_1000b964 = 0;
  DAT_1000b968 = 0;
  return;
}


/* init_the_parms  entry 0x4131bc  size 12 bytes */

void init_the_parms(void)

{
  DAT_1000b980 = 0x1000;
  theParms = 0x100;
  DAT_1000b978 = 0x100;
  DAT_1000b97c = 0x80;
  DAT_1000b98c = 0;
  DAT_1000b990 = 7;
  DAT_1000b974 = 0x80;
  DAT_1000b984 = 0;
  DAT_1000b988 = 0;
  return;
}


/* usage  entry 0x413238  size 12 bytes */

void usage(void)

{
  printf("Usage:\n");
  printf("spectrogram [options] soundfile.[aiff]\n");
  printf("\toptions are (defaults in parentheses):\n");
  printf("\t    -b: double-buffer mode\n");
  printf("\t    -d n: number of frames (500)\n");
  printf("\t    -e: do LPC spectral envelope analysis of input\n");
  printf("\t    -f: 2D (flat) mode\n");
  printf("\t    -g: draw reconstructed waveform (FALSE)\n");
  printf("\t    -h help (this message)\n");
  printf("\t    -l: realtime input mode (FALSE)\n");
  printf("\t    -m n: draw mode (1=line, 2=point, 3=polygon, 4=mesh (DEFAULT))\n");
  printf("\t    -n n: number of samples to analyze/frame (256)\n");
  printf("\t    -o n: FFT size (256)\n");
  printf("\t    -p: polar mode (FALSE)\n");
  printf("\t    -s n: stride: no. of samples to advance each frame (128)\n");
  printf("\t    -S use a synthesis window\n");
  printf("\t    -t: do filter-function synthesis\n");
  printf("\t    -w n: type of window to use (7)\n");
  printf("\t\t0 = exact blackman, 1 = blackman, 2-5 = blackman-harris\n");
  printf("\t\t6 = hamming, 7 = hanning, 8 = rectangular (no windowing!)\n");
  printf("\t    -x: do cross synthesis\n");
  printf("\t    -y: supress perspective\n");
  printf("\t    -z: do z-buffering\n");
  printf("\n");
  return;
}


/* main  entry 0x413448  size 12 bytes */

void main(int param_1,undefined4 *param_2)

{
  int iVar1;
  char *pcVar2;
  size_t sVar3;
  char *pcStack_10;
  int iStack_4;
  
  pcStack_10 = "testout.aiff";
  printf("%s\n","Copyright (c) 1993 The Regents of the University of California");
  myname = *param_2;
  foreground();
  FUN_00413028();
  FUN_004131c8();
  iStack_4 = 1;
  if (1 < param_1) {
    do {
      if (*(char *)param_2[iStack_4] == '-') {
        if (*(char *)(param_2[iStack_4] + 1) == '\0') {
          pipe_in = 1;
        }
        else {
          switch(*(undefined1 *)(param_2[iStack_4] + 1)) {
          case 0x42:
            iVar1 = atoi((char *)(param_2[iStack_4] + 2));
            fofbw = (double)iVar1;
            break;
          default:
            FUN_00413244();
                    /* WARNING: Subroutine does not return */
            exit(1);
          case 0x4f:
            writeback = 1;
            pcStack_10 = (char *)(param_2[iStack_4] + 2);
            break;
          case 0x53:
            DAT_1000b98c = 1;
            break;
          case 0x61:
            DAT_1000b920 = 1;
            break;
          case 0x62:
            DAT_1000b910 = 1;
            break;
          case 100:
            DAT_1000acb0 = atoi((char *)(param_2[iStack_4] + 2));
            DAT_1000acb4 = DAT_1000acb0;
            break;
          case 0x65:
            DAT_1000b92c = 1;
            break;
          case 0x66:
            DAT_1000acc0 = 0;
            break;
          case 0x67:
            DAT_1000b968 = 1;
            break;
          case 0x68:
            FUN_00413244();
                    /* WARNING: Subroutine does not return */
            exit(0);
          case 0x69:
            import = 1;
            break;
          case 0x6c:
            input = 1;
            DAT_1000b924 = 1;
            break;
          case 0x6d:
            DAT_1000acac = atoi((char *)(param_2[iStack_4] + 2));
            break;
          case 0x6e:
            theParms = atoi((char *)(param_2[iStack_4] + 2));
            break;
          case 0x6f:
            DAT_1000b978 = atoi((char *)(param_2[iStack_4] + 2));
            iVar1 = DAT_1000b978;
            if (DAT_1000b978 < 0) {
              iVar1 = DAT_1000b978 + 1;
            }
            DAT_1000b97c = iVar1 >> 1;
            break;
          case 0x70:
            DAT_1000acb8 = 1;
            break;
          case 0x73:
            DAT_1000b974 = atoi((char *)(param_2[iStack_4] + 2));
            break;
          case 0x74:
            DAT_1000b984 = 1;
            break;
          case 0x77:
            DAT_1000b990 = atoi((char *)(param_2[iStack_4] + 2));
            break;
          case 0x78:
            DAT_1000b988 = 1;
            break;
          case 0x79:
            DAT_1000b914 = 0;
            break;
          case 0x7a:
            DAT_1000b938 = 1;
          }
        }
      }
      else {
        pcVar2 = strstr((char *)param_2[iStack_4],".aiff");
        if (pcVar2 == (char *)0x0) {
          FUN_00413244();
                    /* WARNING: Subroutine does not return */
          exit(1);
        }
        soundfiletype = 0;
        malloc(0x32);
        fname = (void *)param_2[iStack_4];
        afd = FUN_0041678c(fname,&DAT_100012e4);
        if (afd == 0) {
          perror("Sound file error!");
                    /* WARNING: Subroutine does not return */
          exit(1);
        }
        FUN_004169e4(afd);
        DAT_1000acbc = 0;
        DAT_1000b924 = 1;
        input = 3;
      }
      iStack_4 = iStack_4 + 1;
    } while (iStack_4 < param_1);
  }
  if (param_1 == 1) {
    fname = malloc(0x32);
    fname = (void *)FUN_0041b93c("Select files","sounds","*.aiff",0);
    afd = FUN_0041678c(fname,&DAT_1000131c);
    if (afd == 0) {
      perror("File error");
                    /* WARNING: Subroutine does not return */
      exit(1);
    }
    FUN_004169e4(afd);
    DAT_1000b924 = 1;
    DAT_1000acbc = 0;
    input = 3;
  }
  if (DAT_1000b924 == 0) {
    DAT_1000b90c = 1;
  }
  FUN_0041027c();
  FUN_00412348();
  if (writeback != 0) {
    sVar3 = strlen(pcStack_10);
    if (sVar3 == 0) {
      pcStack_10 = "testout.aiff";
    }
    wafd = FUN_0041678c(pcStack_10,&DAT_1000133c);
    if (wafd == 0) {
      perror("Error opening AIFF write file.");
                    /* WARNING: Subroutine does not return */
      exit(1);
    }
  }
  FUN_0040d178();
  do {
    do {
      FUN_0041973c();
    } while (DAT_1000b964 == 0);
    FUN_0040d178();
  } while( true );
}


/* get_format_parms  entry 0x413c18  size 108 bytes */

void get_format_parms(void)

{
  int iVar1;
  undefined4 local_8;
  undefined4 local_4;
  
  printf("Enter Format File display params...\n");
  printf("Minimum partial amplitude(^D gives default=.00001):\n");
  iVar1 = scanf("%f",&local_8);
  if (iVar1 != -1) {
    parminamp = local_8;
  }
  printf("Maximum number of partials(^D gives default=32):\n");
  iVar1 = scanf("%i",&local_4);
  if (iVar1 != -1) {
    parmaxnum = local_4;
  }
  return;
}


/* create_form_Menu_Form  entry 0x413ce0  size 12 bytes */

void create_form_Menu_Form(void)

{
  undefined4 uVar1;
  
  Menu_Form = FUN_0041729c(0,0x43ac0000,0x43b80000);
  FUN_00421bb0(3,0,0,0x43ac0000,0x43b80000,&DAT_10001630);
  uVar1 = FUN_00419fec(1,0x41200000,0x43aa0000,0x4243999a,0x419ccccd,&DAT_10001634);
  FileMenu = uVar1;
  FUN_0041c684(uVar1,9,0x10);
  FUN_0041c7b4(uVar1,0x20);
  FUN_0041c8fc(uVar1,2);
  FUN_00420fa8(uVar1,FileMenu_C,0);
  lowcol = FUN_00417468();
  uVar1 = FUN_0041f674(2,0x41200000,0x41c80000,0x41a00000,0x43200000,&DAT_1000163c);
  lRedSlider = uVar1;
  FUN_0041c618(uVar1,6);
  FUN_0041c684(uVar1,9,1);
  FUN_0041c7b4(uVar1,1);
  FUN_0041c820(uVar1,0x41000000);
  FUN_00420fa8(uVar1,lSlider_C,0);
  uVar1 = FUN_0041f674(2,0x41f00000,0x41c80000,0x41a00000,0x43200000,&DAT_10001640);
  lGreenSlider = uVar1;
  FUN_0041c618(uVar1,4);
  FUN_0041c684(uVar1,10,2);
  FUN_0041c7b4(uVar1,2);
  FUN_0041c820(uVar1,0x41000000);
  FUN_00420fa8(uVar1,lSlider_C,0);
  uVar1 = FUN_0041f674(2,0x42480000,0x41c80000,0x41a00000,0x43200000,&DAT_10001644);
  lBlueSlider = uVar1;
  FUN_0041c618(uVar1,6);
  FUN_0041c684(uVar1,0x10,4);
  FUN_0041c7b4(uVar1,4);
  FUN_0041c820(uVar1,0x41000000);
  FUN_00420fa8(uVar1,lSlider_C,0);
  FUN_00417584();
  hicol = FUN_00417468();
  uVar1 = FUN_0041f674(2,0x42b40000,0x41c80000,0x41a00000,0x43200000,&DAT_10001648);
  hRedSlider = uVar1;
  FUN_0041c618(uVar1,6);
  FUN_0041c684(uVar1,9,1);
  FUN_0041c7b4(uVar1,1);
  FUN_0041c820(uVar1,0x41000000);
  FUN_00420fa8(uVar1,hSlider_C,0);
  uVar1 = FUN_0041f674(2,0x42dc0000,0x41c80000,0x41a00000,0x43200000,&DAT_1000164c);
  hGreenSlider = uVar1;
  FUN_0041c618(uVar1,6);
  FUN_0041c684(uVar1,10,2);
  FUN_0041c7b4(uVar1,2);
  FUN_0041c820(uVar1,0x41000000);
  FUN_00420fa8(uVar1,hSlider_C,0);
  uVar1 = FUN_0041f674(2,0x43020000,0x41c80000,0x41a00000,0x43200000,&DAT_10001650);
  hBlueSlider = uVar1;
  FUN_0041c618(uVar1,6);
  FUN_0041c684(uVar1,0x10,4);
  FUN_0041c7b4(uVar1,4);
  FUN_0041c820(uVar1,0x41000000);
  FUN_00420fa8(uVar1,hSlider_C,0);
  FUN_00417584();
  uVar1 = FUN_00419fec(1,0x428c0000,0x43aa0000,0x42480000,0x41a00000,"Colors");
  MapMenu = uVar1;
  FUN_0041c684(uVar1,9,0x25);
  FUN_00420fa8(uVar1,MapMenu_C,0);
  uVar1 = FUN_0041f674(1,0x43430000,0x42e60000,0x42e60000,0x41a00000,&DAT_1000165c);
  gainsl = uVar1;
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,2);
  FUN_00420fa8(uVar1,gainsl_C,0);
  uVar1 = FUN_0041f674(1,0x43430000,0x42be0000,0x42e60000,0x41a00000,"floor");
  floorsl = uVar1;
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,2);
  FUN_00420fa8(uVar1,floorsl_C,0);
  uVar1 = FUN_0041f674(1,0x43430000,0x42960000,0x42e60000,0x41a00000,"twist");
  twistsl = uVar1;
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,2);
  FUN_00420fa8(uVar1,viewsl_C,0);
  uVar1 = FUN_004209ec(0,0x42480000,0x43988000,0x42200000,0x41a00000,"Clear");
  clearB = uVar1;
  FUN_0041c820(uVar1,0x41000000);
  FUN_00420fa8(uVar1,clearit,0);
  uVar1 = FUN_004209ec(0,0x41200000,0x43988000,0x42200000,0x41a00000,"LiveIn");
  freezB = uVar1;
  FUN_0041c820(uVar1,0x41000000);
  FUN_00420fa8(uVar1,button_C,0);
  uVar1 = FUN_0041ec38(0,0x42c80000,0x436b0000,0x42480000,0x41f00000,"Length");
  lengthInput = uVar1;
  FUN_0041c684(uVar1,0xc,6);
  FUN_0041c820(uVar1,0x41200000);
  FUN_0041c8fc(uVar1,1);
  FUN_00420fa8(uVar1,length_C,0);
  uVar1 = FUN_004209ec(0,0x42b40000,0x43988000,0x42200000,0x41a00000,&DAT_1000168c);
  drawB = uVar1;
  FUN_0041c820(uVar1,0x41000000);
  FUN_00420fa8(uVar1,shape_C,0);
  uVar1 = FUN_0041f674(1,0x43430000,0x43070000,0x42e60000,0x41a00000,&DAT_10001694);
  fovsl = uVar1;
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,2);
  FUN_00420fa8(uVar1,viewsl_C,0);
  uVar1 = FUN_0041f674(1,0x43430000,0x425c0000,0x42e60000,0x41a00000,"tscale");
  xscalesl = uVar1;
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,2);
  FUN_00420fa8(uVar1,scale_C,0);
  uVar1 = FUN_0041f674(1,0x43430000,0x431b0000,0x42e60000,0x41a00000,"Cursor");
  cursorsl = uVar1;
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,0);
  FUN_00420fa8(uVar1,cursor_C,0);
  uVar1 = FUN_0041f674(1,0x437a0000,0x436f0000,0x42480000,0x41300000,&DAT_100016ac);
  xsl = uVar1;
  FUN_0041c618(uVar1,4);
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,3);
  FUN_00420fa8(uVar1,viewsl_C,0);
  uVar1 = FUN_00421d9c(0,0x437a0000,0x437a0000,0x42480000,0x41a00000,"Translation");
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,4);
  uVar1 = FUN_00419fec(1,0x42700000,0x43870000,0x425c0000,0x41a00000,&DAT_100016c0);
  WinMenu = uVar1;
  FUN_0041c684(uVar1,9,0x25);
  FUN_00420fa8(uVar1,WinMenu_C,0);
  uVar1 = FUN_0041ec38(0,0x42700000,0x437a0000,0x420c0000,0x41700000,"FFT Size");
  orderInput = uVar1;
  FUN_0041c618(uVar1,6);
  FUN_0041c684(uVar1,0xc,6);
  FUN_0041c820(uVar1,0x41000000);
  FUN_00420fa8(uVar1,Input_C,0);
  uVar1 = FUN_0041ec38(0,0x42700000,0x436b0000,0x420c0000,0x41700000,"Win Size");
  winsizeInput = uVar1;
  FUN_0041c618(uVar1,6);
  FUN_0041c684(uVar1,0xc,6);
  FUN_0041c820(uVar1,0x41000000);
  FUN_00420fa8(uVar1,Input_C,0);
  uVar1 = FUN_0041ec38(0,0x42700000,0x435c0000,0x420c0000,0x41700000,"Stride");
  strideInput = uVar1;
  FUN_0041c618(uVar1,6);
  FUN_0041c684(uVar1,0xc,6);
  FUN_0041c820(uVar1,0x41000000);
  FUN_00420fa8(uVar1,Input_C,0);
  uVar1 = FUN_00420a60(2,0x43520000,0x43848000,0x41c80000,0x40a00000,"Monitor");
  monitorB = uVar1;
  FUN_0041c618(uVar1,0);
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,2);
  FUN_00420fa8(uVar1,button_C,0);
  uVar1 = FUN_00420a60(2,0x43520000,0x437f0000,0x41c80000,0x40a00000,"Scope");
  tgraphB = uVar1;
  FUN_0041c618(uVar1,0);
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,2);
  FUN_00420fa8(uVar1,button_C,0);
  uVar1 = FUN_0041f674(1,0x43430000,0x420c0000,0x42e60000,0x41a00000,"ascale");
  yscalesl = uVar1;
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,2);
  FUN_00420fa8(uVar1,scale_C,0);
  uVar1 = FUN_0041f674(1,0x43430000,0x41700000,0x42e60000,0x41a00000,"fscale");
  zscalesl = uVar1;
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,2);
  FUN_00420fa8(uVar1,scale_C,0);
  uVar1 = FUN_00420a60(2,0x43740000,0x439f0000,0x42200000,0x41a00000,&DAT_10001708);
  zbB = uVar1;
  FUN_0041c618(uVar1,0);
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,2);
  FUN_00420fa8(uVar1,button_C,0);
  uVar1 = FUN_00420a60(2,0x43750000,0x43960000,0x42200000,0x41a00000,&DAT_1000170c);
  dbB = uVar1;
  FUN_0041c618(uVar1,0);
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,2);
  FUN_00420fa8(uVar1,button_C,0);
  uVar1 = FUN_00420a60(2,0x43520000,0x43750000,0x41c80000,0x40a00000,"Meter");
  bargrphB = uVar1;
  FUN_0041c618(uVar1,0);
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,2);
  FUN_00420fa8(uVar1,button_C,0);
  uVar1 = FUN_00419fec(1,0x43020000,0x43aa0000,0x425c0000,0x41a00000,"Filter");
  SurfMenu = uVar1;
  FUN_0041c684(uVar1,9,0x25);
  FUN_00420fa8(uVar1,SurfMenu_C,0);
  uVar1 = FUN_00420a60(2,0x43520000,0x43610000,0x41c80000,0x40a00000,"Peaks");
  partialB = uVar1;
  FUN_0041c618(uVar1,0);
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,2);
  FUN_00420fa8(uVar1,button_C,0);
  uVar1 = FUN_0041ec38(0,0x439b0000,0x431b0000,0x41f00000,0x41a00000,&DAT_10001728);
  maxcurInput = uVar1;
  FUN_0041c684(uVar1,0xc,6);
  FUN_0041c820(uVar1,0x40c00000);
  FUN_0041c8fc(uVar1,3);
  FUN_00420fa8(uVar1,cursor_C,0);
  uVar1 = FUN_0041ec38(0,0x43250000,0x431b0000,0x41f00000,0x41a00000,&DAT_1000172c);
  mincurInput = uVar1;
  FUN_0041c684(uVar1,0xc,6);
  FUN_0041c820(uVar1,0x40c00000);
  FUN_0041c8fc(uVar1,0);
  FUN_00420fa8(uVar1,cursor_C,0);
  uVar1 = FUN_00420a60(2,0x43520000,0x43570000,0x41c80000,0x40a00000,&DAT_10001730);
  F0B = uVar1;
  FUN_0041c618(uVar1,0);
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,2);
  FUN_00420fa8(uVar1,button_C,0);
  uVar1 = FUN_00420a60(2,0x43520000,0x436b0000,0x41c80000,0x40a00000,"Surface");
  specB = uVar1;
  FUN_0041c618(uVar1,0);
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,2);
  FUN_00420fa8(uVar1,button_C,0);
  uVar1 = FUN_00420a60(2,0x433e0000,0x43960000,0x42200000,0x41a00000,"Modify");
  modB = uVar1;
  FUN_0041c618(uVar1,0);
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,2);
  FUN_00420fa8(uVar1,button_C,0);
  uVar1 = FUN_00420a60(2,0x43750000,0x438c0000,0x42200000,0x41a00000,&DAT_10001744);
  logfreqB = uVar1;
  FUN_0041c618(uVar1,0);
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,2);
  FUN_00420fa8(uVar1,button_C,0);
  uVar1 = FUN_00420a60(2,0x433d0000,0x439f0000,0x42200000,0x41a00000,"Impulse");
  sourceB = uVar1;
  FUN_0041c618(uVar1,0);
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,2);
  FUN_00420fa8(uVar1,button_C,0);
  uVar1 = FUN_00420a60(2,0x43520000,0x434d0000,0x41c80000,0x40a00000,&DAT_10001754);
  lpcenvB = uVar1;
  FUN_0041c618(uVar1,0);
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,2);
  FUN_00420fa8(uVar1,button_C,0);
  uVar1 = FUN_0041f674(1,0x437a0000,0x43610000,0x42480000,0x41300000,&DAT_10001758);
  ysl = uVar1;
  FUN_0041c618(uVar1,4);
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,3);
  FUN_00420fa8(uVar1,viewsl_C,0);
  uVar1 = FUN_0041f674(1,0x437a0000,0x43520000,0x42480000,0x41300000,&DAT_1000175c);
  zsl = uVar1;
  FUN_0041c618(uVar1,4);
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,3);
  FUN_00420fa8(uVar1,viewsl_C,0);
  uVar1 = FUN_00420a60(2,0x43520000,0x43430000,0x41c80000,0x40a00000,"SynWin");
  synthwinB = uVar1;
  FUN_0041c618(uVar1,0);
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,2);
  FUN_00420fa8(uVar1,button_C,0);
  uVar1 = FUN_00420a60(2,0x433e0000,0x438c0000,0x42200000,0x41a00000,"Persp");
  perspB = uVar1;
  FUN_0041c618(uVar1,0);
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,2);
  FUN_00420fa8(uVar1,button_C,0);
  uVar1 = FUN_00420a60(2,0x43980000,0x438c0000,0x42200000,0x41a00000,&DAT_10001774);
  axesB = uVar1;
  FUN_0041c618(uVar1,0);
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,2);
  FUN_00420fa8(uVar1,button_C,0);
  uVar1 = FUN_00420a60(2,0x43980000,0x439f0000,0x42200000,0x41a00000,"Polar");
  polarB = uVar1;
  FUN_0041c618(uVar1,0);
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,2);
  FUN_00420fa8(uVar1,button_C,0);
  uVar1 = FUN_00420a60(2,0x43980000,0x43960000,0x42200000,0x41a00000,&DAT_10001784);
  draw3dB = uVar1;
  FUN_0041c618(uVar1,0);
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,2);
  FUN_00420fa8(uVar1,button_C,0);
  uVar1 = FUN_00421d9c(0,0x40a00000,0x43870000,0x42200000,0x41700000,"Window");
  FUN_0041c820(uVar1,0x41000000);
  FUN_0041c8fc(uVar1,4);
  uVar1 = FUN_00421d9c(0,0x40a00000,0x43938000,0x41f00000,0x40a00000,"Pause");
  FUN_0041c820(uVar1,0x41000000);
  FUN_00417374();
  return;
}


/* create_the_forms  entry 0x415bcc  size 12 bytes */

void create_the_forms(void)

{
  FUN_00413cec();
  return;
}


/* spectrum  entry 0x415c10  size 12 bytes */

void spectrum(undefined4 *param_1,int param_2,float *param_3,int param_4)

{
  int iVar1;
  float *pfStack_20;
  float *pfStack_10;
  int iStack_c;
  int iStack_8;
  
  iStack_c = 1;
  iStack_8 = 0;
  if (1 < param_4) {
    do {
      iStack_c = iStack_c << 1;
      iStack_8 = iStack_8 + 1;
    } while (iStack_c < param_4);
  }
  if (fftinitialized != iStack_8) {
    FUN_00416018(iStack_8);
    fftinitialized = iStack_8;
  }
  pfStack_20 = (float *)&tmpc;
  iVar1 = 0;
  pfStack_10 = (float *)param_1;
  if (0 < param_2) {
    do {
      *pfStack_20 = *pfStack_10;
      pfStack_10 = pfStack_10 + 1;
      pfStack_20[1] = 0.0;
      pfStack_20 = pfStack_20 + 2;
      iVar1 = iVar1 + 1;
    } while (iVar1 < param_2);
  }
  if ((param_2 < iStack_c) && (iVar1 = 0, 0 < iStack_c - param_2)) {
    do {
      *pfStack_20 = 0.0;
      pfStack_20[1] = 0.0;
      pfStack_20 = pfStack_20 + 2;
      iVar1 = iVar1 + 1;
    } while (iVar1 < iStack_c - param_2);
  }
  FUN_004161b4(&tmpc,iStack_8);
  FUN_004162ec(&tmpc,iStack_8);
  pfStack_20 = (float *)&tmpc;
  iVar1 = 0;
  pfStack_10 = param_3;
  if (0 < param_4) {
    do {
      *pfStack_10 = *pfStack_20 * *pfStack_20;
      *pfStack_10 = *pfStack_10 + pfStack_20[1] * pfStack_20[1];
      *pfStack_10 = *pfStack_10 / (float)(param_4 * param_4);
      pfStack_10 = pfStack_10 + 1;
      pfStack_20 = pfStack_20 + 2;
      iVar1 = iVar1 + 1;
    } while (iVar1 < param_4);
  }
  return;
}


/* inspect  entry 0x415e98  size 12 bytes */

void inspect(float *param_1,float *param_2,int param_3)

{
  int iVar1;
  float *pfStack_1c;
  float *pfStack_14;
  int iStack_10;
  int iStack_c;
  
  iStack_10 = 1;
  iStack_c = 0;
  if (1 < param_3) {
    do {
      iStack_10 = iStack_10 << 1;
      iStack_c = iStack_c + 1;
    } while (iStack_10 < param_3);
  }
  if (fftinitialized != iStack_c) {
    FUN_00416018(iStack_c);
    fftinitialized = iStack_c;
  }
  FUN_004161b4(param_1,iStack_c);
  FUN_00416548(param_1,iStack_c);
  iVar1 = 0;
  pfStack_1c = param_1;
  pfStack_14 = param_2;
  if (0 < param_3) {
    do {
      *pfStack_14 = *pfStack_1c * (1.0 / (float)param_3);
      pfStack_1c = pfStack_1c + 2;
      pfStack_14 = pfStack_14 + 1;
      iVar1 = iVar1 + 1;
    } while (iVar1 < param_3);
  }
  return;
}


/* buildtable  entry 0x41600c  size 12 bytes */

void buildtable(int param_1)

{
  float fVar1;
  int iVar2;
  void *pvVar3;
  double dVar4;
  int iStack_c;
  
  iVar2 = *(int *)(&pwr_two + param_1 * 4);
  pvVar3 = (&DAT_10000180)[param_1];
  if (DAT_10000180 != (void *)0x0) {
    free(DAT_10000180);
  }
  DAT_10000180 = malloc((int)pvVar3 << 3);
  if (DAT_10000180 == (void *)0x0) {
    printf("malloc failed\n");
                    /* WARNING: Subroutine does not return */
    exit(0);
  }
  iStack_c = 0;
  if (0 < (int)pvVar3) {
    do {
      fVar1 = ((float)iStack_c * 6.2831855) / (float)iVar2;
      dVar4 = cos((double)fVar1);
      *(float *)((int)DAT_10000180 + iStack_c * 8) = (float)dVar4;
      dVar4 = sin((double)fVar1);
      *(float *)((int)DAT_10000180 + iStack_c * 8 + 4) = (float)-dVar4;
      iStack_c = iStack_c + 1;
    } while (iStack_c < (int)pvVar3);
  }
  return;
}


/* bitreverse  entry 0x4161a8  size 12 bytes */

void bitreverse(int param_1,int param_2)

{
  int iVar1;
  int iVar2;
  uint uVar3;
  int iVar4;
  undefined4 uVar5;
  undefined4 uVar6;
  
  iVar4 = *(int *)(&pwr_two + param_2 * 4);
  uVar3 = 0;
  iVar2 = 0;
  while( true ) {
    if (iVar2 < (int)uVar3) {
      uVar5 = *(undefined4 *)(param_1 + iVar2 * 8);
      uVar6 = *(undefined4 *)(param_1 + iVar2 * 8 + 4);
      *(undefined4 *)(param_1 + iVar2 * 8) = *(undefined4 *)(param_1 + uVar3 * 8);
      *(undefined4 *)(param_1 + iVar2 * 8 + 4) = *(undefined4 *)(param_1 + uVar3 * 8 + 4);
      *(undefined4 *)(param_1 + uVar3 * 8) = uVar5;
      *(undefined4 *)(param_1 + uVar3 * 8 + 4) = uVar6;
    }
    iVar2 = iVar2 + 1;
    iVar1 = param_2;
    if (iVar2 == iVar4) break;
    while ((iVar1 = iVar1 + -1, -1 < iVar1 && ((*(uint *)(&pwr_two + iVar1 * 4) & uVar3) != 0))) {
      uVar3 = uVar3 - *(int *)(&pwr_two + iVar1 * 4);
    }
    uVar3 = *(int *)(&pwr_two + iVar1 * 4) + uVar3;
  }
  return;
}


/* fft  entry 0x4162e0  size 12 bytes */

void fft(int param_1,uint param_2)

{
  int iVar1;
  int iVar2;
  int iVar3;
  uint uVar4;
  float fVar5;
  float fVar6;
  float fVar7;
  float fVar8;
  float fVar9;
  int iStack_28;
  uint uStack_24;
  uint uStack_18;
  uint uStack_10;
  uint uStack_4;
  
  iVar3 = *(int *)(&pwr_two + param_2 * 4);
  iVar2 = (&DAT_10000180)[param_2];
  uStack_4 = 1;
  uStack_18 = param_2;
  while (param_2 != 0) {
    uStack_18 = uStack_18 - 1;
    uStack_24 = param_2 - 1;
    uStack_10 = 0;
    iStack_28 = iVar2;
    while (iStack_28 != 0) {
      iStack_28 = iStack_28 + -1;
      uVar4 = uStack_10 << (uStack_18 & 0x1f) & iVar3 - 1U;
      iVar1 = uStack_10 + uStack_4;
      fVar5 = *(float *)(param_1 + iVar1 * 8);
      fVar6 = *(float *)(param_1 + iVar1 * 8 + 4);
      fVar7 = *(float *)(DAT_10000180 + uVar4 * 8);
      fVar8 = *(float *)(DAT_10000180 + uVar4 * 8 + 4);
      fVar9 = fVar5 * fVar7 - fVar6 * fVar8;
      fVar7 = fVar5 * fVar8 + fVar6 * fVar7;
      fVar6 = *(float *)(param_1 + uStack_10 * 8);
      fVar5 = *(float *)(param_1 + uStack_10 * 8 + 4);
      *(float *)(param_1 + iVar1 * 8) = fVar6 - fVar9;
      *(float *)(param_1 + iVar1 * 8 + 4) = fVar5 - fVar7;
      *(float *)(param_1 + uStack_10 * 8) = fVar6 + fVar9;
      *(float *)(param_1 + uStack_10 * 8 + 4) = fVar5 + fVar7;
      uStack_10 = iVar1 + 1U & ~uStack_4;
    }
    uStack_4 = uStack_4 << 1;
    param_2 = uStack_24;
  }
  return;
}


/* ifft  entry 0x41653c  size 12 bytes */

void ifft(int param_1,uint param_2)

{
  uint uVar1;
  int iVar2;
  float *pfVar3;
  int iVar4;
  int iVar5;
  float fVar6;
  float fVar7;
  int iStack_28;
  uint uStack_24;
  uint uStack_18;
  uint uStack_10;
  uint uStack_4;
  
  iVar5 = *(int *)(&pwr_two + param_2 * 4);
  iVar2 = (&DAT_10000180)[param_2];
  uStack_4 = 1;
  uStack_18 = param_2;
  while (param_2 != 0) {
    uStack_18 = uStack_18 - 1;
    uStack_24 = param_2 - 1;
    uStack_10 = 0;
    iStack_28 = iVar2;
    while (iStack_28 != 0) {
      iStack_28 = iStack_28 + -1;
      iVar4 = uStack_10 + uStack_4;
      uVar1 = uStack_10 << (uStack_18 & 0x1f) & iVar5 - 1U;
      pfVar3 = (float *)(DAT_10000180 + uVar1 * 8);
      fVar6 = pfVar3[1] * *(float *)(param_1 + iVar4 * 8 + 4) +
              *(float *)(param_1 + iVar4 * 8) * *pfVar3;
      pfVar3 = (float *)(DAT_10000180 + uVar1 * 8);
      fVar7 = *pfVar3 * *(float *)(param_1 + iVar4 * 8 + 4) -
              *(float *)(param_1 + iVar4 * 8) * pfVar3[1];
      *(float *)(param_1 + iVar4 * 8) = *(float *)(param_1 + uStack_10 * 8) - fVar6;
      *(float *)(param_1 + iVar4 * 8 + 4) = *(float *)(param_1 + uStack_10 * 8 + 4) - fVar7;
      *(float *)(param_1 + uStack_10 * 8) = *(float *)(param_1 + uStack_10 * 8) + fVar6;
      *(float *)(param_1 + uStack_10 * 8 + 4) = *(float *)(param_1 + uStack_10 * 8 + 4) + fVar7;
      uStack_10 = iVar4 + 1U & ~uStack_4;
    }
    uStack_4 = uStack_4 << 1;
    param_2 = uStack_24;
  }
  return;
}


/* aiff_open  entry 0x416780  size 12 bytes */

undefined4 aiff_open(undefined4 param_1,char *param_2)

{
  undefined4 uVar1;
  char *pcVar2;
  int iVar3;
  undefined4 uStack_4;
  
  uVar1 = AFnewfilesetup();
  pcVar2 = strstr(param_2,"r");
  if (pcVar2 == (char *)0x0) {
    AFinitchannels(uVar1,0x3e9,1);
    AFinitrate(uVar1,0x3e9,(int)((ulonglong)(double)sampling_rate >> 0x20),
               SUB84((double)sampling_rate,0));
    AFinitfilefmt(uVar1,2);
    uStack_4 = AFopenfile(param_1,param_2,uVar1);
  }
  else {
    uStack_4 = AFopenfile(param_1,param_2,uVar1);
    iVar3 = AFgetchannels(uStack_4,0x3e9);
    if (iVar3 == 2) {
      printf("Stereo file detected- mixing down to mono...\n");
    }
  }
  return uStack_4;
}


/* aiff_read_samps  entry 0x4168d4  size 12 bytes */

void aiff_read_samps(undefined4 param_1,undefined4 param_2,undefined4 param_3)

{
  AFreadframes(param_1,0x3e9,param_3,param_2);
  return;
}


/* aiff_write_samps  entry 0x41692c  size 12 bytes */

void aiff_write_samps(undefined4 param_1,undefined4 param_2,undefined4 param_3)

{
  AFwriteframes(param_1,0x3e9,param_3,param_2);
  return;
}


/* aiff_seek  entry 0x416984  size 12 bytes */

void aiff_seek(undefined4 param_1,undefined4 param_2)

{
  AFseekframe(param_1,0x3e9,param_2);
  return;
}


/* getafparams  entry 0x4169d8  size 12 bytes */

void getafparams(undefined4 param_1)

{
  int iVar1;
  double in_f0_1;
  undefined4 uStack_10;
  int iStack_c;
  void *pvStack_8;
  int *piStack_4;
  
  piStack_4 = malloc(4);
  pvStack_8 = malloc(4);
  AFgetrate(param_1,0x3e9);
  sampling_rate = (int)in_f0_1;
  iVar1 = AFgetchannels(param_1,0x3e9);
  sfstereo = (uint)(iVar1 == 2);
  AFgetsampfmt(param_1,0x3e9,pvStack_8,piStack_4);
  if (*piStack_4 != 0x10) {
    ringbell();
    printf("This AIFF file doesn\'t have 16-bit data!\n");
  }
  uStack_10 = 3;
  FUN_0042c1c4(1,&uStack_10,2);
  uStack_10 = 4;
  iStack_c = sampling_rate;
  FUN_0042e204(1,&uStack_10,2);
  iVar1 = AFgetframecnt(param_1,0x3e9);
  if (DAT_1000b974 == 0) {
    trap(0x1c00);
  }
  if ((DAT_1000b974 == -1) && (iVar1 == -0x80000000)) {
    trap(0x1800);
  }
  DAT_1000acb0 = iVar1 / DAT_1000b974;
  return;
}


/* getmax  entry 0x416b90  size 12 bytes */

float getmax(double *param_1,uint param_2)

{
  uint uVar1;
  float fVar2;
  
  fVar2 = (float)*param_1;
  uVar1 = 0;
  if (param_2 != 0) {
    do {
      if ((double)fVar2 < param_1[uVar1]) {
        fVar2 = (float)param_1[uVar1];
      }
      uVar1 = uVar1 + 1;
    } while (uVar1 < param_2);
  }
  return fVar2;
}


/* normalize  entry 0x416c10  size 12 bytes */

void normalize(int param_1,uint param_2,float param_3)

{
  uint uVar1;
  
  uVar1 = 0;
  if (param_2 != 0) {
    do {
      *(double *)(param_1 + uVar1 * 8) = *(double *)(param_1 + uVar1 * 8) * (double)param_3;
      uVar1 = uVar1 + 1;
    } while (uVar1 < param_2);
  }
  return;
}


/* fof_transf  entry 0x416c78  size 12 bytes */

void fof_transf(int param_1,uint param_2,undefined4 param_3,undefined4 param_4,double param_5,
               double param_6,double param_7)

{
  undefined8 uVar1;
  float fVar2;
  uint uStack_30;
  
  uStack_30 = 0;
  do {
    uVar1 = FUN_00416dfc(param_6 * 3.1415,3.1415 / param_7);
    *(undefined8 *)(param_1 + uStack_30 * 8) = uVar1;
    uStack_30 = uStack_30 + 1;
  } while (uStack_30 <= param_2);
  fVar2 = (float)FUN_00416b9c(param_1,param_2);
  FUN_00416c1c(param_1,param_2,(float)(param_5 / (double)fVar2));
  return;
}


/* fof_value  entry 0x416df0  size 12 bytes */

double fof_value(double param_1,double param_2)

{
  double in_f0_1;
  double dVar1;
  double dVar2;
  double dVar3;
  double in_stack_00000010;
  
  dVar3 = param_2 * param_2;
  dVar2 = param_1 * param_1 + in_stack_00000010 * in_stack_00000010;
  exp(param_1 * -1.0 * (3.1416 / param_2));
  dVar1 = in_f0_1;
  cos(in_stack_00000010 * (3.1416 / param_2));
  return SQRT((dVar1 * in_f0_1 * 2.0 + in_f0_1 * in_f0_1 + 1.0) /
              ((dVar2 * dVar2 +
               dVar3 * (dVar3 + (param_1 * param_1 - in_stack_00000010 * in_stack_00000010) * 2.0))
              * dVar2)) * (dVar3 / 2.0);
}


/* gal  entry 0x416f60  size 12 bytes */

undefined4 * gal(undefined4 param_1)

{
  float *pfVar1;
  int iVar2;
  int iStack_4;
  
  iStack_4 = 1;
  DAT_12052828 = param_1;
  do {
    (&DAT_12052828)[iStack_4] =
         *(float *)(iStack_4 * 4 + 0x12052824) -
         *(float *)(iStack_4 * 4 + 0x1205285c) * (float)(&DAT_120527f0)[iStack_4];
    iStack_4 = iStack_4 + 1;
  } while (iStack_4 < 0xd);
  iStack_4 = 0xc;
  do {
    (&DAT_12052860)[iStack_4] =
         *(float *)(iStack_4 * 4 + 0x1205285c) -
         *(float *)(iStack_4 * 4 + 0x12052824) * (float)(&DAT_120527f0)[iStack_4];
    pfVar1 = (float *)(iStack_4 * 4 + 0x12052898);
    *pfVar1 = *pfVar1 * 0.998;
    iVar2 = iStack_4 * 4;
    *(float *)(iVar2 + 0x12052898) =
         *(float *)(iVar2 + 0x12052898) +
         (*(float *)(iVar2 + 0x12052824) * *(float *)(iVar2 + 0x12052824) +
         *(float *)(iVar2 + 0x1205285c) * *(float *)(iVar2 + 0x1205285c)) * 0.002;
    iVar2 = iStack_4 * 4;
    (&DAT_120527f0)[iStack_4] =
         (float)(&DAT_120527f0)[iStack_4] +
         (((float)(&DAT_12052828)[iStack_4] * *(float *)(iVar2 + 0x1205285c) +
          (float)(&DAT_12052860)[iStack_4] * *(float *)(iVar2 + 0x12052824)) * 0.002) /
         *(float *)(iVar2 + 0x12052898);
    iStack_4 = iStack_4 + -1;
  } while (0 < iStack_4);
  DAT_12052860 = param_1;
  DAT_120527f0 = DAT_12052858;
  return &DAT_120527f0;
}


/* lattice  entry 0x4171c0  size 12 bytes */

float lattice(float param_1)

{
  float fStack_8;
  int iStack_4;
  
  iStack_4 = 0xb;
  fStack_8 = param_1;
  do {
    fStack_8 = fStack_8 + (float)(&DAT_120527f4)[iStack_4] * (&DAT_120528d0)[iStack_4];
    *(float *)(iStack_4 * 4 + 0x120528d4) =
         (&DAT_120528d0)[iStack_4] - (float)(&DAT_120527f4)[iStack_4] * fStack_8;
    iStack_4 = iStack_4 + -1;
  } while (-1 < iStack_4);
  DAT_120528d0 = fStack_8;
  return fStack_8;
}


