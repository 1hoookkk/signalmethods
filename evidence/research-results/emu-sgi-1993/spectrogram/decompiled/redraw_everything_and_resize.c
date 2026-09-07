/* redraw_everything_and_resize  entry 0x40d16c  size 12 bytes */

void redraw_everything_and_resize(void)

{
  undefined4 uStack_c;
  undefined4 uStack_8;
  
  winset(DAT_1000ac98);
  color(0);
  if (DAT_1000b90c == 0) {
    if (input == 1) {
      uStack_c = 3;
      FUN_0042c1c4(1,&uStack_c,2);
      sampling_rate = uStack_8;
    }
    if (DAT_1000acb8 == 0) {
      clear();
    }
    if (input == 3) {
      FUN_0040d090();
      if (DAT_1000acc0 != 0) {
        FUN_0040c484();
      }
      if (DAT_1000b924 != 0) {
        FUN_00409db8();
      }
      swapbuffers();
    }
    while ((input == 1 && (DAT_1000b90c == 0))) {
      if ((DAT_1000acc0 != 0) && (DAT_1000acb8 == 0)) {
        FUN_004121c0();
        FUN_0040c484();
      }
      FUN_00409db8();
      swapbuffers();
      DAT_1000b918 = DAT_1000b918 + DAT_1000acb0;
      if (0xe0f < DAT_1000b918) {
        DAT_1000b918 = DAT_1000b918 + -0xe10;
        zclear();
      }
    }
  }
  else {
    do {
      if (DAT_1000acc0 != 0) {
        FUN_0040c484();
      }
      if (DAT_1000b924 != 0) {
        FUN_004087ac();
      }
      FUN_0041973c();
      swapbuffers();
    } while ((DAT_1000b964 != 0) && (DAT_1000b90c != 0));
  }
  return;
}

