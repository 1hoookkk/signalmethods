/* draw_bargraph  entry 0x40b7c4  size 12 bytes */

void draw_bargraph(int param_1)

{
  float fVar1;
  int iStack_10;
  int iStack_c;
  int iStack_8;
  int iStack_4;
  
  iStack_4 = ALgetfilled(port);
  bgnline();
  color(0);
  iStack_10 = (uint)(DAT_1000acb8 == 0) * param_1;
  iStack_c = 0;
  iStack_8 = DAT_1000b97c + 10;
  v3i(&iStack_10);
  fVar1 = (float)iStack_4;
  colorf(fVar1 * 0.00255);
  iStack_c = (int)(fVar1 * 0.00255);
  v3i(&iStack_10);
  endline();
  return;
}

