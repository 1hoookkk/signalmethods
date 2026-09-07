/* buildtable  entry 0x4069e0  size 12 bytes */

void buildtable(int param_1)

{
  void *pvVar1;
  void *pvVar2;
  int iVar3;
  double dVar4;
  double __x;
  double dVar5;
  
  iVar3 = *(int *)(&pwr_two + param_1 * 4);
  pvVar2 = (&DAT_100000e0)[param_1];
  if (DAT_100000e0 != (void *)0x0) {
    free(DAT_100000e0);
  }
  DAT_100000e0 = malloc((int)pvVar2 << 3);
  if (DAT_100000e0 == (void *)0x0) {
    printf("malloc failed\n");
                    /* WARNING: Subroutine does not return */
    exit(0);
  }
  pvVar1 = (void *)0x0;
  if (0 < (int)pvVar2) {
    dVar5 = (double)iVar3;
    iVar3 = 0;
    do {
      __x = (double)(float)(((double)(int)pvVar1 * 6.283185307179586) / dVar5);
      dVar4 = cos(__x);
      *(float *)((int)DAT_100000e0 + iVar3) = (float)dVar4;
      dVar4 = sin(__x);
      pvVar1 = (void *)((int)pvVar1 + 1);
      *(float *)((int)DAT_100000e0 + iVar3 + 4) = (float)-dVar4;
      iVar3 = iVar3 + 8;
    } while (pvVar1 != pvVar2);
  }
  return;
}

