/* SurfMenu_C  entry 0x40e79c  size 256 bytes */

void SurfMenu_C(undefined4 param_1)

{
  size_t sVar1;
  float fVar2;
  float fVar3;
  float local_1054;
  float local_1034;
  float local_1030 [1024];
  void *local_30;
  void *local_2c;
  void *local_28;
  char *local_20;
  char *local_1c;
  FILE *local_18;
  size_t local_10;
  int local_c;
  int local_8;
  int local_4;
  
  local_1034 = 1.0;
  local_28 = malloc(0x800);
  local_2c = malloc(0x800);
  malloc(0x200);
  local_30 = malloc(0x1000);
  local_c = fl_get_menu(param_1);
  if (local_c == 1) {
    local_20 = malloc(0x32);
    local_20 = (char *)FUN_0041b93c("Select files",&DAT_10000c24,"*.filt",0);
    if (local_20 != (char *)0x0) {
      local_10 = strlen(local_20);
      local_1c = malloc(0x32);
      strncpy(local_1c,local_20,local_10);
      local_18 = fopen(local_20,"r");
      if (local_18 == (FILE *)0x0) {
        perror("File error");
                    /* WARNING: Subroutine does not return */
        exit(1);
      }
      local_8 = 0;
      if (0 < DAT_1000acb0) {
        do {
          sVar1 = fread(local_1030,4,DAT_1000b97c + 1,local_18);
          if (sVar1 != DAT_1000b97c + 1U) {
            fclose(local_18);
            return;
          }
          (&flttbl)[local_8 * 0x400] = (double)local_1030[0];
          local_4 = 1;
          if (0 < DAT_1000b97c) {
            do {
              (&flttbl)[local_4 + local_8 * 0x400] = (double)local_1030[local_4];
              (&flttbl)[(DAT_1000b978 - local_4) + local_8 * 0x400] = (double)local_1030[local_4];
              local_4 = local_4 + 1;
            } while (local_4 <= DAT_1000b97c);
          }
          local_8 = local_8 + 1;
        } while (local_8 < DAT_1000acb0);
      }
    }
  }
  else if (local_c == 2) {
    local_20 = malloc(0x32);
    local_20 = (char *)FUN_0041b93c("Select files",&DAT_10000c54,"*.filt",0);
    if (local_20 != (char *)0x0) {
      local_10 = strlen(local_20);
      local_1c = malloc(0x32);
      strncpy(local_1c,local_20,local_10);
      local_18 = fopen(local_1c,"w");
      if (local_18 == (FILE *)0x0) {
        perror("File error");
                    /* WARNING: Subroutine does not return */
        exit(1);
      }
      if ((DAT_1000b984 == 0) && (local_8 = 0, 0 < DAT_1000acb0)) {
        do {
          fVar2 = (float)FUN_0040c238(local_8);
          fVar2 = sqrtf(fVar2);
          if (fVar2 <= local_1034) {
            fVar2 = local_1034;
          }
          local_1034 = fVar2;
          local_8 = local_8 + 1;
        } while (local_8 < DAT_1000acb0);
      }
      local_8 = 0;
      if (0 < DAT_1000acb0) {
        do {
          local_4 = 0;
          if (-1 < DAT_1000b97c) {
            do {
              if (DAT_1000b988 == 0) {
                local_1054 = 1.0;
              }
              else {
                local_1054 = (float)(double)(&flttbl)[local_4 + local_8 * 0x400];
              }
              if (DAT_1000b984 == 0) {
                fVar2 = *(float *)(&surface + local_8 * 0x2000 + local_4 * 8) / local_1034;
              }
              else {
                fVar2 = 1.0;
              }
              if (DAT_1000b984 == 0) {
                fVar3 = *(float *)(&DAT_10048704 + local_8 * 0x2000 + local_4 * 8) / local_1034;
              }
              else {
                fVar3 = 0.0;
              }
              fVar2 = (float)FUN_004113d8(fVar2 * local_1054,fVar3 * local_1054);
              fVar2 = sqrtf(fVar2);
              local_1030[local_4] = fVar2 * 3.0;
              local_4 = local_4 + 1;
            } while (local_4 <= DAT_1000b97c);
          }
          sVar1 = fwrite(local_1030,4,DAT_1000b97c + 1,local_18);
          if (sVar1 != DAT_1000b97c + 1U) {
                    /* WARNING: Subroutine does not return */
            exit(1);
          }
          local_8 = local_8 + 1;
        } while (local_8 < DAT_1000acb0);
      }
    }
  }
  if (local_18 != (FILE *)0x0) {
    fclose(local_18);
  }
  return;
}

