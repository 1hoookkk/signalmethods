/* aifopen  entry 0x40591c  size 12 bytes */

void aifopen(undefined4 param_1)

{
  undefined4 uVar1;
  
  uVar1 = AFnewfilesetup();
  AFopenfile(param_1,&DAT_100009b4,uVar1);
  return;
}

