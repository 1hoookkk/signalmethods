/* setup_transformation  entry 0x40a3cc  size 12 bytes */

void setup_transformation(int param_1,undefined4 param_2)

{
  longlong lVar1;
  int iVar2;
  
  if (DAT_1000acc0 == 0) {
    ortho(0xbf000000,(float)theScreen - 0.5,param_1,param_2,0xbf000000,(float)DAT_1000ac8c - 0.5,
          0xc47a0000,0x447a0000);
    rotate(900,0x78);
    scale(DAT_1000acf4,0x3f800000);
  }
  else {
    FUN_00411fac();
    rotate(900,0x78);
    if (DAT_1000acb8 == 0) {
      scale(DAT_1000acf4,DAT_1000acf8);
      iVar2 = -DAT_1000acb0;
      if (iVar2 < 0) {
        iVar2 = iVar2 + 1;
      }
      translate((float)((iVar2 >> 1) - DAT_1000b91c),0);
      translate(DAT_1000ace0,DAT_1000ace4);
    }
    else {
      if ((float)DAT_1000b914 * DAT_1000acf4 == 0.0) {
        lVar1 = 0x3fe00000;
      }
      else {
        lVar1 = 0x3ff00000;
      }
      scale((float)(double)(lVar1 << 0x20),DAT_1000acf8);
      rotate(0xfffffc7c,0x79);
    }
  }
  if ((DAT_1000acb8 != 0) && (-1 < param_1)) {
    rotate((int)((float)(param_1 % 0xe10) * DAT_1000acf4),0x79);
  }
  return;
}

