/* FileMenu_C  entry 0x40df08  size 112 bytes */

void FileMenu_C(undefined4 param_1)

{
  char *pcVar1;
  int iVar2;
  char acStack_c [8];
  int local_4;
  
  local_4 = fl_get_menu(param_1);
  if (local_4 == 1) {
    fname = malloc(0x32);
    fname = (char *)FUN_0041b93c("Select files","sounds","*.aiff",0);
    if (fname != (char *)0x0) {
      pcVar1 = strstr(fname,".aiff");
      if (pcVar1 != (char *)0x0) {
        soundfiletype = 0;
        afd = FUN_0041678c(fname,&DAT_10000b24);
        if (afd == 0) {
          perror("Sound file error");
                    /* WARNING: Subroutine does not return */
          exit(1);
        }
        FUN_004169e4(afd);
      }
      input = 3;
      FUN_0041c6f4(freezB,"FileIn");
      DAT_1000b90c = 0;
      DAT_1000b91c = 0;
      sprintf(acStack_c,"%i",DAT_1000acb0);
      FUN_0041ecac(lengthInput,acStack_c);
      FUN_0041f800(cursorsl,0,(float)DAT_1000acb0);
      FUN_0041f7b8(cursorsl,0);
      FUN_0041f800(xsl,(float)-DAT_1000acb0,0);
      FUN_0041ecac(mincurInput,&DAT_10000b48);
      FUN_0041ecac(maxcurInput,acStack_c);
      DAT_1000acbc = 0;
      FUN_00420744(bargrphB,0);
    }
  }
  else if (local_4 == 2) {
    input = 1;
    FUN_0041c6f4(freezB,"LiveIn");
    DAT_1000b90c = 0;
    DAT_1000b91c = 0;
    pcVar1 = (char *)fl_get_input(maxcurInput);
    iVar2 = atoi(pcVar1);
    FUN_0041f800(cursorsl,0,(float)iVar2);
    FUN_0041f7b8(cursorsl,0);
    FUN_0041ecac(mincurInput,&DAT_10000b54);
  }
  else if (local_4 == 3) {
    FUN_004115e4();
    sginap(5);
    AFclosefile(wafd);
                    /* WARNING: Subroutine does not return */
    exit(0);
  }
  FUN_0040d178();
  return;
}

