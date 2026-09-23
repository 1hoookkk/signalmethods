
void FUN_18013e8b0(longlong *param_1,longlong param_2,RECT *param_3)

{
  bool bVar1;
  bool bVar2;
  BOOL BVar3;
  int iVar4;
  undefined8 uVar5;
  undefined8 uVar6;
  undefined8 uVar7;
  undefined1 auStack_138 [32];
  float *local_118;
  undefined8 local_110;
  undefined8 local_108;
  undefined8 local_f8;
  int local_f0;
  undefined8 local_e8;
  float local_e0;
  float local_dc;
  float local_d8;
  float local_d4;
  undefined8 local_d0;
  undefined8 local_c8;
  undefined8 local_c0;
  undefined4 local_b8;
  undefined4 local_a8 [4];
  undefined4 local_98;
  undefined1 local_8e;
  WCHAR local_8c [34];
  ulonglong local_48;
  
  local_48 = DAT_180876550 ^ (ulonglong)auStack_138;
  BVar3 = EqualRect(param_3,(RECT *)(param_1 + 1));
  uVar7 = 0;
  if (BVar3 == 0) {
    thunk_FUN_1804df020(param_1[3]);
    param_1[3] = 0;
    *(undefined4 *)(param_1 + 4) = 0;
    *(LONG *)(param_1 + 1) = param_3->left;
    *(LONG *)((longlong)param_1 + 0xc) = param_3->top;
    *(LONG *)(param_1 + 2) = param_3->right;
    *(LONG *)((longlong)param_1 + 0x14) = param_3->bottom;
    (**(code **)(*param_1 + 0x18))(param_1);
  }
  if (param_1[3] != 0) {
    uVar5 = uVar7;
    if (param_2 != 0) {
      uVar5 = *(undefined8 *)(param_2 + 8);
    }
    local_e8 = 0;
    GdipCreateFromHDC(uVar5,&local_e8);
    local_c0 = 0;
    local_b8 = GdipCreatePen1(*(undefined4 *)((longlong)param_1 + 0x24),(float)(int)param_1[6],0,
                              &local_c0);
    uVar5 = local_e8;
    GdipSetSmoothingMode(local_e8,4);
    GdipDrawLines(uVar5,local_c0,param_1[3],(int)param_1[4]);
    if (*(int *)(param_1[7] + -0x10) != 0) {
      iVar4 = *(int *)((longlong)param_1 + 0x2c);
      if ((iVar4 == 0) || (iVar4 == 1)) {
        bVar2 = true;
      }
      else {
        bVar2 = false;
      }
      if ((iVar4 == 0) || (iVar4 == 2)) {
        bVar1 = true;
      }
      else {
        bVar1 = false;
      }
      FUN_1804dbbb0(local_a8,0,0x5c);
      local_a8[0] = 0xb;
      local_98 = 400;
      local_8e = 5;
      lstrcpyW(local_8c,L"Tahoma");
      uVar6 = uVar7;
      if (param_2 != 0) {
        uVar6 = *(undefined8 *)(param_2 + 8);
      }
      local_c8 = 0;
      GdipCreateFontFromLogfontW(uVar6,local_a8,&local_c8);
      GdipSetTextRenderingHint(uVar5,5);
      local_f8 = 0;
      local_f0 = GdipCreateStringFormat(0,0,&local_f8);
      uVar6 = 2;
      if (bVar1) {
        uVar6 = uVar7;
      }
      iVar4 = GdipSetStringFormatAlign(local_f8,uVar6);
      if (iVar4 != 0) {
        local_f0 = iVar4;
      }
      iVar4 = GdipSetStringFormatTrimming(local_f8,0);
      if (iVar4 != 0) {
        local_f0 = iVar4;
      }
      iVar4 = GdipSetStringFormatFlags(local_f8,0x1000);
      local_d0 = 0;
      if (iVar4 != 0) {
        local_f0 = iVar4;
      }
      GdipCreateSolidFill((int)param_1[5],&local_d0);
      uVar7 = local_c8;
      GdipGetFontHeight(local_c8,uVar5,&local_e8);
      local_d4 = (float)local_e8;
      local_e0 = (float)param_3->left + 1.0;
      local_dc = (float)param_3->top + 1.0;
      local_d8 = (float)(param_3->right - param_3->left) + 1.0;
      if (!bVar2) {
        local_dc = local_dc + (((float)(param_3->bottom - param_3->top) - (float)local_e8) - 2.0);
      }
      GdipSetTextRenderingHint(uVar5,5);
      uVar6 = local_d0;
      local_108 = local_d0;
      local_110 = local_f8;
      local_118 = &local_e0;
      GdipDrawString(uVar5,param_1[7],0xffffffff,uVar7);
      GdipDeleteBrush(uVar6);
      GdipDeleteStringFormat(local_f8);
      GdipDeleteFont(uVar7);
    }
    GdipDeletePen(local_c0);
    GdipDeleteGraphics(uVar5);
  }
  __security_check_cookie(local_48 ^ (ulonglong)auStack_138);
  return;
}

