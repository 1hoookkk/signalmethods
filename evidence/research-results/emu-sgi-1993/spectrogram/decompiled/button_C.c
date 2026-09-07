/* button_C  entry 0x40f2dc  size 100 bytes */

void button_C(int param_1)

{
  char *__nptr;
  int iVar1;
  
  winset(DAT_1000ac98);
  if (param_1 == synthwinB) {
    DAT_1000b98c = DAT_1000b98c ^ 1;
  }
  if (param_1 == perspB) {
    DAT_1000b914 = DAT_1000b914 ^ 1;
  }
  if (param_1 == lpcenvB) {
    DAT_1000b92c = DAT_1000b92c ^ 1;
  }
  else if (param_1 == freezB) {
    DAT_1000b90c = DAT_1000b90c ^ 1;
    if (DAT_1000b90c == 0) {
      DAT_1000b91c = 0;
      __nptr = (char *)fl_get_input(maxcurInput);
      iVar1 = atoi(__nptr);
      FUN_0041f800(cursorsl,0,(float)iVar1);
      FUN_0041f7b8(cursorsl,0);
      FUN_0041ecac(mincurInput,&DAT_10000c7c);
      if (input == 3) {
        FUN_0041c6f4(freezB,"FileIn");
      }
      else if (input == 1) {
        FUN_0041c6f4(freezB,"LiveIn");
      }
      FUN_0040d178();
    }
    else {
      FUN_0041c6f4(freezB,"Pause");
      winset(DAT_1000ac98);
    }
  }
  else if (param_1 == bargrphB) {
    DAT_1000acbc = DAT_1000acbc ^ 1;
  }
  else if (param_1 == axesB) {
    DAT_1000b93c = DAT_1000b93c ^ 1;
  }
  else if (param_1 == polarB) {
    DAT_1000acb8 = DAT_1000acb8 ^ 1;
    if (DAT_1000acb8 != 0) {
      DAT_1000acc0 = 1;
      FUN_0040c484();
    }
  }
  else if (param_1 == zbB) {
    DAT_1000b938 = DAT_1000b938 ^ 1;
    winset(DAT_1000ac98);
    zbuffer(DAT_1000b938);
    zclear();
  }
  else if (param_1 == dbB) {
    winset(DAT_1000ac98);
    DAT_1000b910 = DAT_1000b910 ^ 1;
    if (DAT_1000b910 == 0) {
      singlebuffer();
      gconfig();
    }
    else {
      doublebuffer();
      gconfig();
    }
  }
  else if (param_1 == tgraphB) {
    DAT_1000b968 = DAT_1000b968 ^ 1;
    if ((DAT_1000aca0 == 0) && (DAT_1000b968 != 0)) {
      DAT_1000aca0 = winopen("Waveform");
    }
    if ((DAT_1000b968 == 0) && (DAT_1000aca0 != 0)) {
      winclose(DAT_1000aca0);
      DAT_1000aca0 = 0;
    }
  }
  else if (param_1 == modB) {
    DAT_1000b988 = DAT_1000b988 ^ 1;
  }
  else if (param_1 == monitorB) {
    domon = domon ^ 1;
  }
  else if (param_1 == logfreqB) {
    DAT_1000acc4 = DAT_1000acc4 ^ 1;
    FUN_004121c0();
    if (DAT_1000acc0 != 0) {
      FUN_0040c484();
    }
  }
  else if (param_1 == draw3dB) {
    DAT_1000acc0 = DAT_1000acc0 ^ 1;
    FUN_004121c0();
    if (DAT_1000acc0 != 0) {
      FUN_0040c484();
    }
  }
  else if (param_1 == sourceB) {
    DAT_1000b984 = DAT_1000b984 ^ 1;
  }
  return;
}

