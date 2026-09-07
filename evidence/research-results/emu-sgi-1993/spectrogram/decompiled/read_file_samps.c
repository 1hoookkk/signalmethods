/* read_file_samps  entry 0x40a8e0  size 12 bytes */

void read_file_samps(void)

{
  size_t __n;
  size_t sVar1;
  int iVar2;
  
  if (soundfiletype == 0) {
    FUN_004168e0(afd,theParms,&x);
    if (sfstereo != 0) {
      FUN_0040aa64(&x,theParms);
    }
  }
  else if (soundfiletype == 1) {
    if (sfstereo == 0) {
      iVar2 = 1;
    }
    else {
      iVar2 = 2;
    }
    __n = iVar2 * theParms;
    sVar1 = fread(&x,2,__n,file);
    if (sVar1 != __n) {
      perror("Sound file read error!\n");
                    /* WARNING: Subroutine does not return */
      exit(-1);
    }
    if (sfstereo != 0) {
      FUN_0040aa64(&x,theParms);
    }
  }
  FUN_0040c170(&x,&arry,theParms);
  return;
}

