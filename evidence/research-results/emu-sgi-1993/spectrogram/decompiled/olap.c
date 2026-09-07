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

