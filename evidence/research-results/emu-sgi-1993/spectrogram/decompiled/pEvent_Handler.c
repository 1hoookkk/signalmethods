/* pEvent_Handler  entry 0x40d43c  size 204 bytes */

void pEvent_Handler(short param_1,short param_2)

{
  short sVar2;
  int iVar1;
  float local_34;
  float local_30;
  undefined4 local_2c;
  int local_28;
  undefined4 local_24 [4];
  undefined4 local_14;
  undefined4 local_10;
  int local_c;
  int local_4;
  
  local_24[0] = DAT_10000164;
  local_24[1] = DAT_10000168;
  local_24[3] = DAT_10000170;
  local_24[2] = DAT_1000016c;
  local_14 = DAT_10000174;
  local_10 = DAT_10000178;
  local_28 = winget();
  if (local_28 != DAT_1000ac98) {
    return;
  }
  if (DAT_1000b940 == 1) {
    return;
  }
  if (param_1 == 0x65) {
    if (param_2 == 1) {
      if (DAT_1000acc0 == 0) {
        DAT_1000ac40 = DAT_1000ac40 + 1;
        if (2 < DAT_1000ac40) {
          DAT_1000ac40 = 0;
        }
        local_4 = 0;
        if (-1 < DAT_1000acb0) {
          do {
            *(undefined4 *)(&freqarry + local_4 * 0xc + DAT_1000ac40 * 4) = 0;
            local_4 = local_4 + 1;
          } while (local_4 <= DAT_1000acb0);
        }
        FUN_00411a58(DAT_1000ac40,local_c,2);
      }
      FUN_0040d178();
      goto FUN_0040de4c;
    }
  }
  else if (param_1 != 0x66) {
    if (param_1 == 0x67) {
      if (param_2 == 0) {
        function = 0;
        DAT_1000b964 = 0;
        DAT_1000b954 = DAT_1000b954 + DAT_1000b958;
        DAT_1000b958 = 0.0;
      }
      else {
        sVar2 = getvaluator(0x10a);
        DAT_1000b944 = (float)(int)sVar2;
        sVar2 = getvaluator(0x10b);
        DAT_1000b948 = (float)(int)sVar2;
        if (DAT_1000acc0 == 0) {
          function = 0;
          DAT_1000b964 = 0;
          FUN_00411f20();
          pushmatrix();
          frontbuffer(1);
          ortho(0xbf000000,(float)theScreen - 0.5);
          local_34 = DAT_1000b944 - (float)DAT_1000ac90;
          local_30 = DAT_1000b948 - (float)DAT_1000ac94;
          local_2c = 0;
          color(local_24[DAT_1000ac40]);
          bgnline();
          v3f(&DAT_1000ac48);
          v3f(&local_34);
          endline();
          DAT_1000ac48 = local_34;
          DAT_1000ac4c = local_30;
          DAT_1000ac50 = 0;
          local_c = (int)(local_34 / DAT_1000acf4);
          *(float *)(&freqarry + local_c * 0xc + DAT_1000ac40 * 4) = local_30 / 4.0;
          iVar1 = getbutton(0x67);
          while (iVar1 != 0) {
            sVar2 = getvaluator(0x10a);
            DAT_1000b944 = (float)(int)sVar2;
            sVar2 = getvaluator(0x10b);
            DAT_1000b948 = (float)(int)sVar2;
            local_34 = DAT_1000b944 - (float)DAT_1000ac90;
            local_30 = DAT_1000b948 - (float)DAT_1000ac94;
            bgnpoint();
            v3f(&local_34);
            endpoint();
            local_c = (int)(local_34 / DAT_1000acf4);
            *(float *)(&freqarry + local_c * 0xc + DAT_1000ac40 * 4) = local_30 / 4.0;
            iVar1 = getbutton(0x67);
          }
          popmatrix();
          FUN_00411a58(DAT_1000ac40,local_c,1);
        }
        else {
          function = 2;
          DAT_1000b964 = 1;
          DAT_1000b94c = DAT_1000b944;
          DAT_1000b950 = DAT_1000b948;
        }
      }
    }
    goto FUN_0040de4c;
  }
  if (param_2 == 0) {
    function = 0;
    DAT_1000b964 = 0;
    DAT_1000b95c = DAT_1000b95c + DAT_1000b960;
    DAT_1000b960 = 0;
    DAT_1000b95e = DAT_1000b95e + DAT_1000b962;
    DAT_1000b962 = 0;
  }
  else {
    sVar2 = getvaluator(0x10a);
    DAT_1000b944 = (float)(int)sVar2;
    sVar2 = getvaluator(0x10b);
    DAT_1000b948 = (float)(int)sVar2;
    if (DAT_1000acc0 == 0) {
      function = 0;
      DAT_1000b964 = 0;
      FUN_00411f20();
      pushmatrix();
      frontbuffer(1);
      ortho(0xbf000000,(float)theScreen - 0.5);
      local_34 = DAT_1000b944 - (float)DAT_1000ac90;
      local_30 = DAT_1000b948 - (float)DAT_1000ac94;
      local_2c = 0;
      color(0xff);
      bgnpoint();
      v3f(&local_34);
      endpoint();
      DAT_1000ac48 = local_34;
      DAT_1000ac4c = local_30;
      local_c = (int)((DAT_1000b944 - (float)DAT_1000ac90) / DAT_1000acf4);
      *(float *)(&freqarry + local_c * 0xc + DAT_1000ac40 * 4) =
           (DAT_1000b948 - (float)DAT_1000ac94) / 4.0;
      popmatrix();
      FUN_00411a58(DAT_1000ac40,local_c,0);
    }
    else {
      function = 1;
      DAT_1000b964 = 1;
      DAT_1000b94c = DAT_1000b944;
      DAT_1000b950 = DAT_1000b948;
    }
  }
FUN_0040de4c:
  if (((function != 0) && (DAT_1000acc0 != 0)) && (DAT_1000b90c == 0)) {
    FUN_0040c484();
  }
  return;
}

