/* shape_C  entry 0x40f8c4  size 112 bytes */

void shape_C(void)

{
  if (DAT_1000acc0 == 0) {
    DAT_1000acac = 4;
  }
  else if (DAT_1000acac == 2) {
    DAT_1000acac = 3;
    FUN_0041c6f4(drawB,&DAT_10000c9c);
  }
  else if (DAT_1000acac == 3) {
    DAT_1000acac = 4;
    FUN_0041c6f4(drawB,&DAT_10000ca4);
  }
  else if (DAT_1000acac == 4) {
    DAT_1000acac = 1;
    FUN_0041c6f4(drawB,&DAT_10000cac);
  }
  else if (DAT_1000acac == 1) {
    DAT_1000acac = 2;
    FUN_0041c6f4(drawB,"Point");
  }
  return;
}

