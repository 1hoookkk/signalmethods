/* length_C  entry 0x40fa1c  size 168 bytes */

void length_C(void)

{
  char *__nptr;
  
  __nptr = (char *)fl_get_input(lengthInput);
  DAT_1000acb0 = atoi(__nptr);
  FUN_0041f800(xsl,(float)-DAT_1000acb0,0);
  FUN_0040d178();
  return;
}

