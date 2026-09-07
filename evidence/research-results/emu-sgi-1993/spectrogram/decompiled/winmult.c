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

