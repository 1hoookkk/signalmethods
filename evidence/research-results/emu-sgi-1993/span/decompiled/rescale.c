/* rescale  entry 0x405114  size 12 bytes */

void rescale(void)

{
  float fVar1;
  float fVar2;
  int iVar3;
  double dVar4;
  double dVar5;
  double dVar6;
  double dVar7;
  uint in_fcsr;
  
  fVar1 = (float)width;
  iVar3 = sampling_rate;
  if (sampling_rate < 0) {
    iVar3 = sampling_rate + 1;
  }
  fVar2 = (float)height;
  getsize();
  reshapeviewport();
  color(0);
  clear();
  dVar4 = (double)width * 0.1;
  if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
    dVar4 = ROUND(dVar4);
  }
  else {
    dVar4 = FLOOR(dVar4);
  }
  dVar6 = (double)width * 0.9;
  if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
    dVar6 = ROUND(dVar6);
  }
  else {
    dVar6 = FLOOR(dVar6);
  }
  dVar7 = (double)height * 0.1;
  if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
    dVar7 = ROUND(dVar7);
  }
  else {
    dVar7 = FLOOR(dVar7);
  }
  dVar5 = (double)height * 0.9;
  if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
    dVar5 = ROUND(dVar5);
  }
  else {
    dVar5 = FLOOR(dVar5);
  }
  viewport((int)(short)(int)dVar4,(int)(short)(int)dVar6,(int)(short)(int)dVar7,
           (int)(short)(int)dVar5);
  ortho2(0xbf000000,(float)width - 0.5);
  fsc = (float)sampling_rate / ((fmax - fmin) * 2.0);
  asc = 160.0 / (amax - amin);
  foff = -fmin * ((fVar1 * 0.8) / (float)(iVar3 >> 1));
  aoff = (-120.0 - amin) * ((fVar2 * 0.8 * 0.125) / 20.0);
  drawmode(0x20);
  color(0);
  clear();
  FUN_004038a8();
  drawmode(0x10);
  return;
}

