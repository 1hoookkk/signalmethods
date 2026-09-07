/* aiff_open  entry 0x416780  size 12 bytes */

undefined4 aiff_open(undefined4 param_1,char *param_2)

{
  undefined4 uVar1;
  char *pcVar2;
  int iVar3;
  undefined4 uStack_4;
  
  uVar1 = AFnewfilesetup();
  pcVar2 = strstr(param_2,"r");
  if (pcVar2 == (char *)0x0) {
    AFinitchannels(uVar1,0x3e9,1);
    AFinitrate(uVar1,0x3e9,(int)((ulonglong)(double)sampling_rate >> 0x20),
               SUB84((double)sampling_rate,0));
    AFinitfilefmt(uVar1,2);
    uStack_4 = AFopenfile(param_1,param_2,uVar1);
  }
  else {
    uStack_4 = AFopenfile(param_1,param_2,uVar1);
    iVar3 = AFgetchannels(uStack_4,0x3e9);
    if (iVar3 == 2) {
      printf("Stereo file detected- mixing down to mono...\n");
    }
  }
  return uStack_4;
}

