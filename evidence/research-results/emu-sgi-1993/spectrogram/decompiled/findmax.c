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

