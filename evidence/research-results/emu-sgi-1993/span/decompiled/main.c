/* main  entry 0x405c44  size 12 bytes */

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void main(int param_1,undefined4 *param_2)

{
  int iVar1;
  undefined4 uVar2;
  char *pcVar3;
  int iVar4;
  undefined4 uVar5;
  undefined4 uVar6;
  double dVar7;
  double dVar8;
  double dVar9;
  double dVar10;
  uint in_fcsr;
  short asStack_22 [3];
  int iStack_1c;
  undefined4 uStack_18;
  
  uVar6 = 0;
  foreground();
  printf("%s\n","Copyright (c) 1995 E-mu Systems, Inc.");
  iVar4 = 1;
  if (1 < param_1) {
    do {
      param_2 = param_2 + 1;
      pcVar3 = (char *)*param_2;
      if (*pcVar3 == '-') {
        dVar7 = _expg;
        switch(pcVar3[1]) {
        case 'a':
          dVar7 = atof(pcVar3 + 2);
          expk = (undefined4)((ulonglong)dVar7 >> 0x20);
          DAT_10000024 = SUB84(dVar7,0);
          expavg = 1;
          dVar7 = 1.0 - dVar7;
          break;
        default:
          FUN_00405978();
                    /* WARNING: Subroutine does not return */
          exit(1);
        case 'd':
          removeDC = 1;
          break;
        case 'f':
          fd = fopen(pcVar3 + 2,"w");
          dVar7 = _expg;
          break;
        case 'h':
          FUN_00405978();
                    /* WARNING: Subroutine does not return */
          exit(0);
        case 'n':
          winsize = atoi(pcVar3 + 2);
          dVar7 = _expg;
          break;
        case 'o':
          order = atoi(pcVar3 + 2);
          dVar7 = _expg;
          break;
        case 'p':
          power_mode = 1;
          break;
        case 't':
          FUN_00405bc4();
                    /* WARNING: Subroutine does not return */
          exit(0);
        case 'w':
          wintype = atoi(pcVar3 + 2);
          dVar7 = _expg;
        }
      }
      else {
        uVar6 = FUN_00405928(pcVar3);
        iVar1 = AFgetframecnt(uVar6,0x3e9);
        iStack_1c = iVar1 / winsize;
        if (winsize == 0) {
          trap(0x1c00);
        }
        if ((winsize == -1) && (iVar1 == -0x80000000)) {
          trap(0x1800);
        }
        in_fcsr = in_fcsr & 0xff7fffff;
        dVar7 = 1.0 / (double)iStack_1c;
        if (_expg != 0.0) {
          dVar7 = _expg;
        }
      }
      _expg = dVar7;
      iVar4 = iVar4 + 1;
    } while (iVar4 != param_1);
  }
  FUN_004037f0();
  fminit();
  iVar4 = fmfindfont("Times-Roman");
  if (iVar4 == 0) {
                    /* WARNING: Subroutine does not return */
    exit(1);
  }
  uVar2 = fmscalefont(iVar4);
  fmsetfont(uVar2);
  FUN_004035ac();
  iVar4 = sampling_rate;
  if (sampling_rate < 0) {
    iVar4 = sampling_rate + 1;
  }
  fmax = (float)(iVar4 >> 1);
  winset(graph);
  getsize(&width,&height);
  reshapeviewport();
  color(0);
  clear();
  dVar7 = (double)width * 0.1 - 1.0;
  if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
    dVar7 = ROUND(dVar7);
  }
  else {
    dVar7 = FLOOR(dVar7);
  }
  dVar9 = (double)width * 0.9;
  if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
    dVar9 = ROUND(dVar9);
  }
  else {
    dVar9 = FLOOR(dVar9);
  }
  dVar10 = (double)height * 0.1;
  if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
    dVar10 = ROUND(dVar10);
  }
  else {
    dVar10 = FLOOR(dVar10);
  }
  dVar8 = (double)height * 0.9;
  if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
    dVar8 = ROUND(dVar8);
  }
  else {
    dVar8 = FLOOR(dVar8);
  }
  viewport((int)(short)(int)dVar7,(int)(short)(int)dVar9,(int)(short)(int)dVar10,
           (int)(short)(int)dVar8);
  drawmode(0x20);
  FUN_004038a8();
  drawmode(0x10);
  FUN_004042a4(&wintbl,wintype,winsize);
  qdevice(0x210);
  qdevice(10);
  qdevice(0x18);
  qdevice(0x1c);
  qdevice(0x15);
  qdevice(0x20);
  if (fd != (FILE *)0x0) {
    qdevice(0xc);
  }
  uVar2 = uStack_18;
  uVar5 = 0;
caseD_b:
  while( true ) {
    while (iVar4 = qtest(), iVar4 == 0) {
      FUN_00405490(uVar5,uVar2,uVar6);
      uVar2 = 0;
      uVar5 = 0;
    }
    iVar4 = qread(asStack_22);
    if (iVar4 < 0x21) break;
    if (iVar4 == 0x210) {
      FUN_00405120();
    }
  }
  switch(iVar4) {
  case 10:
    goto caseD_a;
  default:
    goto caseD_b;
  case 0xc:
    if (asStack_22[0] != 0) {
      uVar5 = 1;
      goto caseD_b;
    }
    break;
  case 0x15:
    break;
  case 0x18:
    if (iStack_1c != 0) {
      AFseekframe(uVar6,0x3e9,0);
    }
    goto caseD_b;
  case 0x1c:
    if (asStack_22[0] != 0) {
      uVar2 = 1;
    }
    goto caseD_b;
  case 0x20:
    goto code_r0x004064f0;
  }
  if (asStack_22[0] == 0) {
code_r0x004064f0:
    if (asStack_22[0] != 0) {
      printf("Min Amp:\n");
      scanf("%f",&amin);
      printf("Max Amp:\n");
      scanf("%f",&amax);
      FUN_00405120();
    }
  }
  else {
    printf("Min Freq:\n");
    scanf("%f",&fmin);
    printf("Max Freq:\n");
    scanf("%f",&fmax);
    FUN_00405120();
  }
  goto caseD_b;
caseD_a:
  fclose(fd);
                    /* WARNING: Subroutine does not return */
  exit(0);
}

