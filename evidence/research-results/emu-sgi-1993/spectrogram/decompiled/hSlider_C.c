/* hSlider_C  entry 0x40ef84  size 152 bytes */

void hSlider_C(void)

{
  DAT_1000acd4 = fl_get_slider_value(hRedSlider);
  DAT_1000acd8 = fl_get_slider_value(hGreenSlider);
  DAT_1000acdc = fl_get_slider_value(hBlueSlider);
  if (DAT_1000b908 == 0) {
    FUN_00410578();
  }
  else {
    FUN_004106d8();
  }
  return;
}

