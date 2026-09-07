/* init_lut  entry 0x4114e0  size 12 bytes */

void init_lut(void)

{
  short sStack_10;
  short sStack_e;
  short sStack_c;
  short sStack_a;
  short sStack_8;
  short sStack_6;
  int iStack_4;
  
  iStack_4 = 0;
  do {
    sStack_a = (short)iStack_4;
    sStack_8 = sStack_a;
    sStack_6 = sStack_a;
    getmcolor(iStack_4,&sStack_c,&sStack_e,&sStack_10);
    mapcolor(iStack_4,(int)sStack_6,(int)sStack_8,(int)sStack_a);
    *(int *)(&DAT_1000ad08 + iStack_4 * 0xc) = (int)sStack_c;
    *(int *)(&DAT_1000ad0c + iStack_4 * 0xc) = (int)sStack_e;
    *(int *)(&DAT_1000ad10 + iStack_4 * 0xc) = (int)sStack_10;
    iStack_4 = iStack_4 + 1;
  } while (iStack_4 < 0x100);
  return;
}

