/* WinMenu_C  entry 0x40e55c  size 132 bytes */

void WinMenu_C(undefined4 param_1)

{
  int iVar1;
  
  iVar1 = fl_get_menu(param_1);
  iVar1 = iVar1 + -1;
  if ((DAT_1000b98c != 0) && (iVar1 == 1)) {
    perror("Blackman Window NOT valid for use with a Synthesis window!\n");
    ringbell();
    iVar1 = DAT_1000b990;
  }
  DAT_1000b990 = iVar1;
  switch(DAT_1000b990) {
  case 0:
    FUN_0041c6f4(param_1,"ExtBlk");
    break;
  case 1:
    if (DAT_1000b98c == 0) {
      FUN_0041c6f4(param_1,"Blkman");
    }
    break;
  case 2:
    FUN_0041c6f4(param_1,"B-H 1");
    break;
  case 3:
    FUN_0041c6f4(param_1,"B-H 2");
    break;
  case 4:
    FUN_0041c6f4(param_1,"B-H 3");
    break;
  case 5:
    FUN_0041c6f4(param_1,"B-H 4");
    break;
  case 6:
    FUN_0041c6f4(param_1,&DAT_10000bfc);
    break;
  case 7:
    FUN_0041c6f4(param_1,&DAT_10000c04);
    break;
  case 8:
    FUN_0041c6f4(param_1,&DAT_10000c0c);
  }
  FUN_0040b910(&wintbl,DAT_1000b990,theParms);
  FUN_0040d178();
  return;
}

