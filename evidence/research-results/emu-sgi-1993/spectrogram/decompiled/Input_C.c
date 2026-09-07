/* Input_C  entry 0x40fae0  size 132 bytes */

void Input_C(int param_1)

{
  int iVar1;
  char *pcVar2;
  char acStack_4 [4];
  
  if (param_1 == orderInput) {
    pcVar2 = (char *)fl_get_input(param_1);
    DAT_1000b978 = atoi(pcVar2);
    iVar1 = DAT_1000b978;
    if (DAT_1000b978 < 0) {
      iVar1 = DAT_1000b978 + 1;
    }
    DAT_1000b97c = iVar1 >> 1;
  }
  else if (param_1 == winsizeInput) {
    pcVar2 = (char *)fl_get_input(param_1);
    theParms = atoi(pcVar2);
  }
  else if (param_1 == strideInput) {
    pcVar2 = (char *)fl_get_input(param_1);
    DAT_1000b974 = atoi(pcVar2);
    if (afd != 0) {
      FUN_004169e4(afd);
      iVar1 = FUN_00416990(afd,0);
      if (iVar1 < 0) {
        perror("seek failed!\n");
                    /* WARNING: Subroutine does not return */
        exit(1);
      }
      sprintf(acStack_4,"%i",DAT_1000acb0);
      FUN_0041ecac(lengthInput,acStack_4);
    }
  }
  FUN_0040b910(&wintbl,DAT_1000b990,theParms);
  return;
}

