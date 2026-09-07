/* fof_transf  entry 0x416c78  size 12 bytes */

void fof_transf(int param_1,uint param_2,undefined4 param_3,undefined4 param_4,double param_5,
               double param_6,double param_7)

{
  undefined8 uVar1;
  float fVar2;
  uint uStack_30;
  
  uStack_30 = 0;
  do {
    uVar1 = FUN_00416dfc(param_6 * 3.1415,3.1415 / param_7);
    *(undefined8 *)(param_1 + uStack_30 * 8) = uVar1;
    uStack_30 = uStack_30 + 1;
  } while (uStack_30 <= param_2);
  fVar2 = (float)FUN_00416b9c(param_1,param_2);
  FUN_00416c1c(param_1,param_2,(float)(param_5 / (double)fVar2));
  return;
}

