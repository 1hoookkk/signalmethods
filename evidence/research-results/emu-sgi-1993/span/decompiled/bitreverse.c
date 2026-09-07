/* bitreverse  entry 0x406b7c  size 12 bytes */

void bitreverse(int param_1,int param_2)

{
  int iVar1;
  uint uVar2;
  int iVar3;
  undefined4 *puVar4;
  undefined4 *puVar5;
  int iVar6;
  uint *puVar7;
  uint uVar8;
  undefined4 uVar9;
  undefined4 uVar10;
  
  iVar1 = *(int *)(&pwr_two + param_2 * 4);
  uVar2 = 0;
  iVar3 = 0;
  do {
    if (iVar3 < (int)uVar2) {
      puVar5 = (undefined4 *)(param_1 + uVar2 * 8);
      puVar4 = (undefined4 *)(param_1 + iVar3 * 8);
      uVar9 = *puVar4;
      *puVar4 = *puVar5;
      uVar10 = puVar4[1];
      puVar4[1] = puVar5[1];
      *puVar5 = uVar9;
      puVar5[1] = uVar10;
    }
    iVar3 = iVar3 + 1;
    iVar6 = param_2 + -1;
    if (iVar3 == iVar1) {
      return;
    }
    if (-1 < iVar6) {
      puVar7 = (uint *)(&pwr_two + iVar6 * 4);
      do {
        uVar8 = *puVar7;
        puVar7 = puVar7 + -1;
        if ((uVar8 & uVar2) == 0) break;
        iVar6 = iVar6 + -1;
        uVar2 = uVar2 - uVar8;
      } while (-1 < iVar6);
    }
    uVar2 = *(int *)(&pwr_two + iVar6 * 4) + uVar2;
  } while( true );
}

