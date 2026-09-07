/* check_sr  entry 0x4035a0  size 12 bytes */

void check_sr(void)

{
  undefined4 uStack_8;
  int iStack_4;
  
  uStack_8 = 3;
  FUN_00406ecc(1,&uStack_8,2);
  sampling_rate = iStack_4;
  if (iStack_4 == -1) {
    uStack_8 = 0;
    FUN_00406ecc(1,&uStack_8,2);
    if (iStack_4 != 2) {
      fprintf((FILE *)0xfb52904,"Whoa there! Weird sampling rate.\n");
                    /* WARNING: Subroutine does not return */
      exit(-1);
    }
    uStack_8 = 0x14;
    FUN_00406ecc(1,&uStack_8,2);
    if (iStack_4 < 0) {
      fprintf((FILE *)0xfb52904,"Whoa there! Weird sampling rate.\n");
                    /* WARNING: Subroutine does not return */
      exit(-1);
    }
    sampling_rate = iStack_4;
  }
  return;
}

