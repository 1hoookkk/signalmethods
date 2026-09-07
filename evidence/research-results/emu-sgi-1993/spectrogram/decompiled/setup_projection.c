/* setup_projection  entry 0x411fa0  size 12 bytes */

void setup_projection(undefined4 param_1,undefined4 param_2)

{
  if (DAT_1000b914 == 0) {
    ortho((float)(-theScreen / 10),(float)(theScreen / 10),param_1,param_2,
          (float)(-DAT_1000ac8c / 10),(float)(DAT_1000ac8c / 10),0xc47a0000,0x447a0000);
  }
  else {
    perspective((int)DAT_1000acec,0x3faaaaab,(float)DAT_1000aca4,(float)DAT_1000aca8);
  }
  polarview(DAT_1000b954 + DAT_1000b958);
  return;
}

