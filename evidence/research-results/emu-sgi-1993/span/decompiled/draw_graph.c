/* draw_graph  entry 0x403f90  size 12 bytes */

void draw_graph(float *param_1)

{
  int iVar1;
  int iVar2;
  float fVar3;
  float fVar4;
  float fStack_8;
  float fStack_4;
  
  fVar3 = (float)width;
  fVar4 = (float)order;
  color(0xff);
  pushmatrix();
  scale(fsc,asc);
  translate(foff,aoff);
  bgnline();
  iVar2 = 0;
  iVar1 = order;
  if (order < 0) {
    iVar1 = order + 1;
  }
  if (0 < iVar1 >> 1) {
    do {
      fStack_8 = (float)iVar2 * ((fVar3 * 2.0) / fVar4);
      fStack_4 = (*param_1 / 20.0 + -2.0) * (float)height * 0.125 + (float)height;
      v2f(&fStack_8);
      iVar2 = iVar2 + 1;
      param_1 = param_1 + 1;
      iVar1 = order;
      if (order < 0) {
        iVar1 = order + 1;
      }
    } while (iVar2 < iVar1 >> 1);
  }
  endline();
  popmatrix();
  iVar1 = ALgetfilled(port);
  color(2);
  bgnline();
  fStack_8 = 0.0;
  fStack_4 = 10.0;
  v2f(&fStack_8);
  fStack_8 = (float)((iVar1 * width) / 8000);
  v2f(&fStack_8);
  endline();
  return;
}

