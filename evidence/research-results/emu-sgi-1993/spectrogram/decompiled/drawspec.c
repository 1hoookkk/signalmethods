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

