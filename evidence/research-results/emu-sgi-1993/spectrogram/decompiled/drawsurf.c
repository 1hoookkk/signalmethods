/* drawsurf  entry 0x4087a0  size 12 bytes */

void drawsurf(void)

{
  int iStack_4;
  
  mmode(2);
  iStack_4 = DAT_1000b91c;
  if (DAT_1000b91c < DAT_1000acb0 + DAT_1000b91c) {
    do {
      pushmatrix();
      FUN_0040a3d8(iStack_4);
      FUN_00408a20(iStack_4);
      popmatrix();
      if ((domon != 0) || (DAT_1000b968 != 0)) {
        FUN_00415ea4(&cx,&arry,DAT_1000b978);
        FUN_00410ae4(&arry,0);
        if (domon != 0) {
          FUN_0042b644(oport,&arry,DAT_1000b974);
        }
      }
      if (writeback != 0) {
        FUN_00408964();
      }
      FUN_0041973c();
      iStack_4 = iStack_4 + 1;
    } while (iStack_4 < DAT_1000acb0 + DAT_1000b91c);
  }
  if (domon != 0) {
    FUN_00410ae4(&arry,1);
  }
  return;
}

