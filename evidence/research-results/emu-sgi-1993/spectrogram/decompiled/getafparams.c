/* getafparams  entry 0x4169d8  size 12 bytes */

void getafparams(undefined4 param_1)

{
  int iVar1;
  double in_f0_1;
  undefined4 uStack_10;
  int iStack_c;
  void *pvStack_8;
  int *piStack_4;
  
  piStack_4 = malloc(4);
  pvStack_8 = malloc(4);
  AFgetrate(param_1,0x3e9);
  sampling_rate = (int)in_f0_1;
  iVar1 = AFgetchannels(param_1,0x3e9);
  sfstereo = (uint)(iVar1 == 2);
  AFgetsampfmt(param_1,0x3e9,pvStack_8,piStack_4);
  if (*piStack_4 != 0x10) {
    ringbell();
    printf("This AIFF file doesn\'t have 16-bit data!\n");
  }
  uStack_10 = 3;
  FUN_0042c1c4(1,&uStack_10,2);
  uStack_10 = 4;
  iStack_c = sampling_rate;
  FUN_0042e204(1,&uStack_10,2);
  iVar1 = AFgetframecnt(param_1,0x3e9);
  if (DAT_1000b974 == 0) {
    trap(0x1c00);
  }
  if ((DAT_1000b974 == -1) && (iVar1 == -0x80000000)) {
    trap(0x1800);
  }
  DAT_1000acb0 = iVar1 / DAT_1000b974;
  return;
}

