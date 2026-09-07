/* writesamps  entry 0x408958  size 12 bytes */

void writesamps(void)

{
  FUN_0040c1d4(&arry,&x,DAT_1000b974);
  if (DAT_12052708 == 0) {
    FUN_00416938(wafd,DAT_1000b974,&x);
  }
  FUN_00416938(wafd,DAT_1000b974,&x);
  AFsyncfile(wafd);
  DAT_12052708 = 1;
  return;
}

