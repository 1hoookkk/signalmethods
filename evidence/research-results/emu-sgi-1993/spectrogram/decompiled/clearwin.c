/* clearwin  entry 0x4121b4  size 12 bytes */

void clearwin(void)

{
  winset(DAT_1000ac98);
  color(0);
  clear();
  zclear();
  return;
}

