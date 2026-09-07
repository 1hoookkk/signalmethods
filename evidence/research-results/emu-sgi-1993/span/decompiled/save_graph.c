/* save_graph  entry 0x404f8c  size 12 bytes */

void save_graph(float *param_1)

{
  int iVar1;
  int iVar2;
  double dVar3;
  
  iVar2 = 0;
  iVar1 = order;
  if (order < 0) {
    iVar1 = order + 1;
  }
  if (-1 < iVar1 >> 1) {
    do {
      if (order == 0) {
        trap(0x1c00);
      }
      if ((order == -1) && (sampling_rate * iVar2 == -0x80000000)) {
        trap(0x1800);
      }
      dVar3 = (double)((sampling_rate * iVar2) / order);
      fprintf(fd,"%f %f\n",(int)((ulonglong)dVar3 >> 0x20),SUB84(dVar3,0),
              (double)((*param_1 + 100.0) * 0.65789473 - 100.0));
      iVar2 = iVar2 + 1;
      param_1 = param_1 + 1;
      iVar1 = order;
      if (order < 0) {
        iVar1 = order + 1;
      }
    } while (iVar2 <= iVar1 >> 1);
  }
  fprintf(fd,"\n");
  return;
}

