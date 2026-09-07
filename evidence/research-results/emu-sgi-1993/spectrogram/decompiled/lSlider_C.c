/* lSlider_C  entry 0x40eebc  size 152 bytes */

void lSlider_C(void)

{
  DAT_1000acc8 = fl_get_slider_value(lRedSlider);
  DAT_1000accc = fl_get_slider_value(lGreenSlider);
  DAT_1000acd0 = fl_get_slider_value(lBlueSlider);
  if (DAT_1000b908 == 0) {
    FUN_00410578();
  }
  else {
    FUN_004106d8();
  }
  return;
}

