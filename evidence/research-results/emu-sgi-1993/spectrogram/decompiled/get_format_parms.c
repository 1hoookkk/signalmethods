/* get_format_parms  entry 0x413c18  size 108 bytes */

void get_format_parms(void)

{
  int iVar1;
  undefined4 local_8;
  undefined4 local_4;
  
  printf("Enter Format File display params...\n");
  printf("Minimum partial amplitude(^D gives default=.00001):\n");
  iVar1 = scanf("%f",&local_8);
  if (iVar1 != -1) {
    parminamp = local_8;
  }
  printf("Maximum number of partials(^D gives default=32):\n");
  iVar1 = scanf("%i",&local_4);
  if (iVar1 != -1) {
    parmaxnum = local_4;
  }
  return;
}

