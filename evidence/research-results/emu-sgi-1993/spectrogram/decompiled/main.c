/* main  entry 0x413448  size 12 bytes */

void main(int param_1,undefined4 *param_2)

{
  int iVar1;
  char *pcVar2;
  size_t sVar3;
  char *pcStack_10;
  int iStack_4;
  
  pcStack_10 = "testout.aiff";
  printf("%s\n","Copyright (c) 1993 The Regents of the University of California");
  myname = *param_2;
  foreground();
  FUN_00413028();
  FUN_004131c8();
  iStack_4 = 1;
  if (1 < param_1) {
    do {
      if (*(char *)param_2[iStack_4] == '-') {
        if (*(char *)(param_2[iStack_4] + 1) == '\0') {
          pipe_in = 1;
        }
        else {
          switch(*(undefined1 *)(param_2[iStack_4] + 1)) {
          case 0x42:
            iVar1 = atoi((char *)(param_2[iStack_4] + 2));
            fofbw = (double)iVar1;
            break;
          default:
            FUN_00413244();
                    /* WARNING: Subroutine does not return */
            exit(1);
          case 0x4f:
            writeback = 1;
            pcStack_10 = (char *)(param_2[iStack_4] + 2);
            break;
          case 0x53:
            DAT_1000b98c = 1;
            break;
          case 0x61:
            DAT_1000b920 = 1;
            break;
          case 0x62:
            DAT_1000b910 = 1;
            break;
          case 100:
            DAT_1000acb0 = atoi((char *)(param_2[iStack_4] + 2));
            DAT_1000acb4 = DAT_1000acb0;
            break;
          case 0x65:
            DAT_1000b92c = 1;
            break;
          case 0x66:
            DAT_1000acc0 = 0;
            break;
          case 0x67:
            DAT_1000b968 = 1;
            break;
          case 0x68:
            FUN_00413244();
                    /* WARNING: Subroutine does not return */
            exit(0);
          case 0x69:
            import = 1;
            break;
          case 0x6c:
            input = 1;
            DAT_1000b924 = 1;
            break;
          case 0x6d:
            DAT_1000acac = atoi((char *)(param_2[iStack_4] + 2));
            break;
          case 0x6e:
            theParms = atoi((char *)(param_2[iStack_4] + 2));
            break;
          case 0x6f:
            DAT_1000b978 = atoi((char *)(param_2[iStack_4] + 2));
            iVar1 = DAT_1000b978;
            if (DAT_1000b978 < 0) {
              iVar1 = DAT_1000b978 + 1;
            }
            DAT_1000b97c = iVar1 >> 1;
            break;
          case 0x70:
            DAT_1000acb8 = 1;
            break;
          case 0x73:
            DAT_1000b974 = atoi((char *)(param_2[iStack_4] + 2));
            break;
          case 0x74:
            DAT_1000b984 = 1;
            break;
          case 0x77:
            DAT_1000b990 = atoi((char *)(param_2[iStack_4] + 2));
            break;
          case 0x78:
            DAT_1000b988 = 1;
            break;
          case 0x79:
            DAT_1000b914 = 0;
            break;
          case 0x7a:
            DAT_1000b938 = 1;
          }
        }
      }
      else {
        pcVar2 = strstr((char *)param_2[iStack_4],".aiff");
        if (pcVar2 == (char *)0x0) {
          FUN_00413244();
                    /* WARNING: Subroutine does not return */
          exit(1);
        }
        soundfiletype = 0;
        malloc(0x32);
        fname = (void *)param_2[iStack_4];
        afd = FUN_0041678c(fname,&DAT_100012e4);
        if (afd == 0) {
          perror("Sound file error!");
                    /* WARNING: Subroutine does not return */
          exit(1);
        }
        FUN_004169e4(afd);
        DAT_1000acbc = 0;
        DAT_1000b924 = 1;
        input = 3;
      }
      iStack_4 = iStack_4 + 1;
    } while (iStack_4 < param_1);
  }
  if (param_1 == 1) {
    fname = malloc(0x32);
    fname = (void *)FUN_0041b93c("Select files","sounds","*.aiff",0);
    afd = FUN_0041678c(fname,&DAT_1000131c);
    if (afd == 0) {
      perror("File error");
                    /* WARNING: Subroutine does not return */
      exit(1);
    }
    FUN_004169e4(afd);
    DAT_1000b924 = 1;
    DAT_1000acbc = 0;
    input = 3;
  }
  if (DAT_1000b924 == 0) {
    DAT_1000b90c = 1;
  }
  FUN_0041027c();
  FUN_00412348();
  if (writeback != 0) {
    sVar3 = strlen(pcStack_10);
    if (sVar3 == 0) {
      pcStack_10 = "testout.aiff";
    }
    wafd = FUN_0041678c(pcStack_10,&DAT_1000133c);
    if (wafd == 0) {
      perror("Error opening AIFF write file.");
                    /* WARNING: Subroutine does not return */
      exit(1);
    }
  }
  FUN_0040d178();
  do {
    do {
      FUN_0041973c();
    } while (DAT_1000b964 == 0);
    FUN_0040d178();
  } while( true );
}

