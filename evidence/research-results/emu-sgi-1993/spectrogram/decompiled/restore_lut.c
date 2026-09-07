/* restore_lut  entry 0x4115d8  size 12 bytes */

void restore_lut(void)

{
  int iVar1;
  int iStack_4;
  
  iStack_4 = 0;
  do {
    iVar1 = iStack_4 * 0xc;
    mapcolor(iStack_4,*(undefined4 *)(&DAT_1000ad08 + iVar1),*(undefined4 *)(&DAT_1000ad0c + iVar1),
             *(undefined4 *)(&DAT_1000ad10 + iVar1));
    iStack_4 = iStack_4 + 1;
  } while (iStack_4 < 0x100);
  winset(DAT_1000ac98);
  mmode(2);
  getsize(&theScreen,&DAT_1000ac8c);
  return;
}

