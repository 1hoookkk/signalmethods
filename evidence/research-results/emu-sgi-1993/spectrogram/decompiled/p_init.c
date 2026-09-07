/* p_init  entry 0x41233c  size 12 bytes */

void p_init(void)

{
  int iVar1;
  undefined4 uVar2;
  float fVar3;
  float fVar4;
  undefined4 uVar5;
  char acStack_4 [4];
  
  FUN_00420b50();
  fminit();
  iVar1 = fmfindfont("Times-Roman");
  if (iVar1 != 0) {
    uVar2 = fmscalefont(iVar1);
    fmsetfont(uVar2);
    FUN_00415bd8();
    FUN_00417a98(Menu_Form,100,100);
    FUN_00417b04(Menu_Form,5,0,0);
    FUN_00420744(bargrphB,DAT_1000acbc);
    FUN_00420744(dbB,DAT_1000b910);
    FUN_00420744(zbB,DAT_1000b938);
    FUN_00420744(lpcenvB,DAT_1000b92c);
    FUN_00420744(synthwinB,DAT_1000b98c);
    FUN_0041f7b8(cursorsl,0);
    FUN_0041f800(cursorsl,0,(float)DAT_1000acb4);
    FUN_0041f800(xsl,(float)-DAT_1000acb4,0);
    FUN_0041f800(ysl,0xc3fa0000,0x43fa0000);
    FUN_0041f800(zsl,0xc4000000,0x44000000);
    FUN_0041f800(xscalesl,0xbf800000,0x40800000);
    FUN_0041f7b8(xscalesl,0x3f800000);
    FUN_0041f800(fovsl,0x40000000,0x447a0000);
    FUN_0041f7b8(fovsl,0x43cd0000);
    FUN_0041f800(floorsl,0xc43b8000,0x437a0000);
    FUN_0041f7b8(floorsl,0xc1a00000);
    sprintf(acStack_4,"%i",DAT_1000acb0);
    FUN_0041ecac(lengthInput,acStack_4);
    sprintf(acStack_4,"%i",DAT_1000b978);
    FUN_0041ecac(orderInput,acStack_4);
    sprintf(acStack_4,"%i",theParms);
    FUN_0041ecac(winsizeInput,acStack_4);
    sprintf(acStack_4,"%i",DAT_1000b974);
    FUN_0041ecac(strideInput,acStack_4);
    FUN_0041ecac(mincurInput,&DAT_10000d60);
    sprintf(acStack_4,"%i",DAT_1000acb0);
    FUN_0041ecac(maxcurInput,acStack_4);
    switch(DAT_1000b990) {
    case 0:
      FUN_0041c6f4(WinMenu,"ExtBlk");
      break;
    case 1:
      if (DAT_1000b98c == 0) {
        FUN_0041c6f4(WinMenu,"Blkman");
      }
      else {
        perror("Blackman Window NOT valid for use with a Synthesis window!\n");
        ringbell();
      }
      break;
    case 2:
      FUN_0041c6f4(WinMenu,"B-H 1");
      break;
    case 3:
      FUN_0041c6f4(WinMenu,"B-H 2");
      break;
    case 4:
      FUN_0041c6f4(WinMenu,"B-H 3");
      break;
    case 5:
      FUN_0041c6f4(WinMenu,"B-H 4");
      break;
    case 6:
      FUN_0041c6f4(WinMenu,&DAT_10000dd4);
      break;
    case 7:
      FUN_0041c6f4(WinMenu,&DAT_10000ddc);
      break;
    case 8:
      FUN_0041c6f4(WinMenu,&DAT_10000de4);
    }
    DAT_1000ac98 = winopen("3D Spectrogram Display");
    if (DAT_1000b968 != 0) {
      DAT_1000aca0 = winopen("Waveform");
    }
    drawmode(0x10);
    glcompat(1,0);
    zfunction(3);
    cmode();
    if (DAT_1000b910 != 0) {
      doublebuffer();
    }
    gconfig();
    FUN_004114ec();
    getplanes();
    FUN_00412124();
    concave(1);
    if (DAT_1000acc0 == 0) {
      DAT_1000acac = 4;
    }
    else {
      frontbuffer(1);
      zbuffer(DAT_1000b938);
      zclear();
      frontbuffer(0);
    }
    FUN_004211b4(0x67);
    FUN_004211b4(0x66);
    FUN_004211b4(0x65);
    FUN_004211b4(0x114);
    FUN_004211b4(0x115);
    FUN_004211b4(0x116);
    FUN_004211b4(0x117);
    FUN_004211b4(0x118);
    FUN_004211b4(0x119);
    FUN_004211b4(0x119);
    FUN_004211b4(0x11a);
    FUN_004211b4(0xb7);
    FUN_004211b4(0xb8);
    FUN_004211b4(0xb9);
    FUN_004211b4(0xba);
    FUN_004211b4(0xbb);
    FUN_004211b4(0xbc);
    FUN_004211b4(0xbd);
    FUN_004211b4(0xbe);
    FUN_004211b4(0xb6);
    FUN_00420f90(pEvent_Handler);
    FUN_0041a200(FileMenu,"Open|LiveInput|Quit");
    FUN_0041a200(MapMenu,"Open|Interpolate|SineColors|RestorePalette");
    FUN_0041a200(WinMenu,
                 "Exact Blackman|Blackman|Blackman-Harris 1|Blackman-Harris 2|Blackman-Harris 3|Blackman-Harris 4|Hamming|Hanning|None"
                );
    FUN_0041a200(SurfMenu,"Load|Save");
    winset(DAT_1000ac98);
    mmode(2);
    getsize(&theScreen,&DAT_1000ac8c);
    viewport(0,theScreen + -1,0,DAT_1000ac8c + -1);
    uVar2 = 0xc47a0000;
    uVar5 = 0x447a0000;
    ortho(0xbf000000,(float)theScreen - 0.5);
    if (DAT_1000acc0 != 0) {
      if (DAT_1000b914 == 0) {
        ortho((float)(-theScreen / 10),(float)(theScreen / 10));
      }
      else {
        perspective((int)DAT_1000acec,0x3faaaaab,(float)DAT_1000aca4,(float)DAT_1000aca8,uVar2,uVar5
                   );
      }
    }
    FUN_0040b910(&wintbl,DAT_1000b990,theParms);
    FUN_004116a4();
    FUN_00411778();
    fVar3 = log10f((float)largest);
    fVar4 = log10f((float)smallest);
    m = ((double)DAT_1000ad00 * 12.75 - (double)DAT_1000ad04) / (double)(fVar3 - fVar4);
    b = (double)DAT_1000ad04 + m * 6.0;
    if (pipe_in == 1) {
      file = (FILE *)&DAT_0fb528e4;
      if (import != 0) {
        fread(&tlength,2,1,(FILE *)&DAT_0fb528e4);
        fread(&torder,2,1,file);
        DAT_1000acb0 = (int)tlength;
        DAT_1000b97c = (int)torder;
        DAT_1000b978 = (int)torder << 1;
      }
      FUN_004116a4();
    }
    return;
  }
                    /* WARNING: Subroutine does not return */
  exit(1);
}

