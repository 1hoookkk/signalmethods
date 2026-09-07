/* clearem  entry 0x412118  size 12 bytes */

void clearem(void)

{
  undefined4 uVar1;
  
  uVar1 = ALgetfilled(port);
  FUN_0042bbc4(port,&x,uVar1);
  FUN_004121c0();
  if (DAT_1000b968 != 0) {
    FUN_00412240();
  }
  return;
}

