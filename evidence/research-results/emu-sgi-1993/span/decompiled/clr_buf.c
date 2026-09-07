/* clr_buf  entry 0x404f1c  size 12 bytes */

void clr_buf(undefined4 *param_1)

{
  int iVar1;
  int iVar2;
  
  iVar2 = 0;
  iVar1 = order;
  if (order < 0) {
    iVar1 = order + 1;
  }
  if (-1 < iVar1 >> 1) {
    do {
      *param_1 = 0xc2c80000;
      iVar2 = iVar2 + 1;
      param_1 = param_1 + 1;
      iVar1 = order;
      if (order < 0) {
        iVar1 = order + 1;
      }
    } while (iVar2 <= iVar1 >> 1);
  }
  return;
}

