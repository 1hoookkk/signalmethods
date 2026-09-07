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

