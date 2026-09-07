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

