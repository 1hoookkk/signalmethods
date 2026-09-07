/* setup_viewport  entry 0x411f14  size 12 bytes */

void setup_viewport(void)

{
  getorigin(&DAT_1000ac90,&DAT_1000ac94);
  getsize(&theScreen,&DAT_1000ac8c);
  viewport(0,theScreen + -1,0,DAT_1000ac8c + -1);
  return;
}

