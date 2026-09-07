/* MapMenu_C  entry 0x40e30c  size 156 bytes */

void MapMenu_C(undefined4 param_1)

{
  int iVar1;
  
  iVar1 = fl_get_menu(param_1);
  if (iVar1 == 1) {
    malloc(0x32);
    iVar1 = FUN_0041b93c("Select files",&DAT_10000b68,"*.map",0);
    if (iVar1 != 0) {
      iVar1 = sproc(loadPal,0xffffff,iVar1);
      if (iVar1 < 0) {
        perror("Oops!");
      }
      else {
        sginap(100);
        mapcolor(0,0,0,0);
        mapcolor(0xff,0xff,0xff,0xff);
      }
    }
  }
  else if (iVar1 == 2) {
    DAT_1000b908 = 0;
  }
  else if (iVar1 == 3) {
    DAT_1000b908 = 1;
  }
  else if (iVar1 == 4) {
    FUN_004115e4();
  }
  return;
}

