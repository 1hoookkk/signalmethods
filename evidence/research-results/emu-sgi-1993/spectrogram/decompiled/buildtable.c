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

