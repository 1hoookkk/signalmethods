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

