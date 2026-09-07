/* cursor_C  entry 0x40f04c  size 84 bytes */

void cursor_C(int param_1)

{
  char *pcVar1;
  float fVar2;
  
  if (DAT_1000b90c == 0) {
    FUN_0041f7b8(cursorsl,0);
  }
  else if ((DAT_1000b940 == 0) && (DAT_1000acc0 != 0)) {
    if (DAT_12052714 == 0) {
      DAT_1205270c = 0;
    }
    if (DAT_12052718 == 0) {
      DAT_12052710 = DAT_1000acb0;
    }
    if (param_1 == maxcurInput) {
      pcVar1 = (char *)fl_get_input(param_1);
      DAT_12052710 = atoi(pcVar1);
      FUN_0041f800(cursorsl,(float)DAT_1205270c,(float)DAT_12052710);
      DAT_12052718 = 1;
    }
    else if (param_1 == mincurInput) {
      pcVar1 = (char *)fl_get_input(param_1);
      DAT_1205270c = atoi(pcVar1);
      FUN_0041f800(cursorsl,(float)DAT_1205270c,(float)DAT_12052710);
      DAT_1000b91c = DAT_1205270c;
      FUN_0041f7b8(cursorsl,(float)DAT_1205270c);
      DAT_12052714 = 1;
    }
    else {
      fVar2 = (float)fl_get_slider_value(param_1);
      DAT_1000b91c = (int)fVar2;
    }
    DAT_1000b940 = 1;
    FUN_0040d178();
    DAT_1000b940 = 0;
  }
  return;
}

