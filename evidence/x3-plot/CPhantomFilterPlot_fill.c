
/* WARNING: Function: __chkstk replaced with injection: alloca_probe */

void FUN_1801376d0(longlong param_1)

{
  longlong lVar1;
  longlong lVar2;
  float *pfVar3;
  longlong lVar4;
  float fVar5;
  float local_fa08 [8000];
  undefined1 local_7d08 [31992];
  undefined8 uStack_10;
  
  uStack_10 = 0x1801376e0;
  lVar1 = 0;
  if (((*(longlong *)(param_1 + 0x128) == 0) || (*(longlong *)(param_1 + 0x130) == 0)) ||
     (lVar4 = *(longlong *)(*(longlong *)(param_1 + 0x128) + 0x98), lVar4 == 0)) {
    FUN_1804dbbb0(param_1 + 0x140,0,8000);
  }
  else {
    lVar4 = *(longlong *)(lVar4 + 0x10);
    if (lVar4 != 0) {
      FUN_1802b0f30(lVar4,*(longlong *)(param_1 + 0x130),8000,local_7d08,local_fa08,
                    *(undefined4 *)(param_1 + 0x2080));
    }
    pfVar3 = (float *)(param_1 + 0x140);
    lVar4 = 2000;
    do {
      *pfVar3 = 0.0;
      fVar5 = 0.0;
      lVar2 = 4;
      do {
        if (ABS(fVar5) < ABS(local_fa08[lVar1])) {
          fVar5 = local_fa08[lVar1];
        }
        lVar1 = lVar1 + 1;
        lVar2 = lVar2 + -1;
      } while (lVar2 != 0);
      *pfVar3 = fVar5;
      pfVar3 = pfVar3 + 1;
      lVar4 = lVar4 + -1;
    } while (lVar4 != 0);
    *(undefined1 *)(param_1 + 0x2084) = 0;
  }
  lVar1 = *(longlong *)(param_1 + 0x138);
  if (lVar1 != 0) {
    *(undefined4 *)(lVar1 + 8) = 0;
    *(undefined4 *)(lVar1 + 0xc) = 0;
    *(undefined4 *)(lVar1 + 0x10) = 0;
    *(undefined4 *)(lVar1 + 0x14) = 0;
  }
  return;
}

