
undefined8 * FUN_180137370(undefined8 *param_1,undefined8 param_2)

{
  code *pcVar1;
  undefined8 *puVar2;
  longlong *plVar3;
  undefined8 *puVar4;
  longlong lVar5;
  undefined8 *puVar6;
  
  FUN_1801dd5b0(param_1,param_2,0,0x27,0xfffffffffffffffe);
  puVar6 = (undefined8 *)0x0;
  param_1[0x24] = 0;
  *param_1 = CPhantomFilterPlot::vftable;
  param_1[0x15] = CPhantomFilterPlot::vftable;
  param_1[0x1b] = CPhantomFilterPlot::vftable;
  param_1[0x23] = CPhantomFilterPlot::vftable;
  param_1[0x25] = 0;
  puVar2 = (undefined8 *)FUN_180497618(0x50);
  puVar4 = puVar6;
  if (puVar2 != (undefined8 *)0x0) {
    *puVar2 = PhantomPlotArtist::vftable;
    *(undefined4 *)(puVar2 + 1) = 0;
    *(undefined4 *)((longlong)puVar2 + 0xc) = 0;
    *(undefined4 *)(puVar2 + 2) = 0;
    *(undefined4 *)((longlong)puVar2 + 0x14) = 0;
    puVar2[3] = 0;
    *(undefined4 *)(puVar2 + 4) = 0;
    *(undefined4 *)((longlong)puVar2 + 0x24) = DAT_18088e37c;
    *(undefined4 *)(puVar2 + 5) = DAT_1808ad480;
    *(undefined4 *)((longlong)puVar2 + 0x2c) = 3;
    *(undefined4 *)(puVar2 + 6) = 1;
    plVar3 = (longlong *)FUN_1804b76d8();
    if (plVar3 == (longlong *)0x0) {
      FUN_180009b70(&DAT_80004005);
      pcVar1 = (code *)swi(3);
      puVar4 = (undefined8 *)(*pcVar1)();
      return puVar4;
    }
    lVar5 = (**(code **)(*plVar3 + 0x18))(plVar3);
    puVar2[7] = lVar5 + 0x18;
    *puVar2 = PhantomFloatTablePlotArtist::vftable;
    *(undefined4 *)(puVar2 + 9) = 2000;
    *(undefined4 *)((longlong)puVar2 + 0x4c) = 0x4b;
    puVar2[8] = param_1 + 0x28;
    puVar4 = puVar2;
  }
  param_1[0x27] = puVar4;
  *(undefined1 *)((longlong)param_1 + 0x2084) = 1;
  lVar5 = FUN_180497618(0x48);
  if (lVar5 != 0) {
    puVar6 = (undefined8 *)FUN_180460620(lVar5);
  }
  param_1[0x26] = puVar6;
  *(undefined4 *)(param_1 + 0x410) = 0xffffffff;
  return param_1;
}

