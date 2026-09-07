/* oinit  entry 0x4037e4  size 12 bytes */

void oinit(void)

{
  graph = winopen("Waveform");
  cmode();
  mmode(0);
  deflinestyle(1,0xf0f0);
  gconfig();
  FUN_004036d4();
  return;
}

