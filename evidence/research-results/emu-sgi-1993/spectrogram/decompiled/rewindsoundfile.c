/* rewindsoundfile  entry 0x40d084  size 12 bytes */

void rewindsoundfile(void)

{
  int iVar1;
  
  if (soundfiletype == 0) {
    iVar1 = FUN_00416990(afd,0);
    if (iVar1 < 0) {
      perror("seek failed!\n");
                    /* WARNING: Subroutine does not return */
      exit(1);
    }
  }
  else if ((soundfiletype == 1) && (iVar1 = fseek(file,0x200,0), iVar1 < 0)) {
    perror("seek failed!\n");
                    /* WARNING: Subroutine does not return */
    exit(1);
  }
  return;
}

