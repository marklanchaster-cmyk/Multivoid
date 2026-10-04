
/* WARNING: Function: __security_check_cookie replaced with injection: security_check_cookie */
/* WARNING: Function: __chkstk replaced with injection: alloca_probe */

void FUN_141465930(longlong *param_1,longlong param_2,undefined4 *param_3)

{
  ushort uVar1;
  code *pcVar2;
  uint uVar3;
  undefined1 *puVar4;
  ulonglong uVar5;
  undefined1 *puVar6;
  uint uVar7;
  longlong *plVar8;
  undefined1 *puVar9;
  undefined1 *puVar10;
  undefined1 *puVar11;
  int iVar12;
  longlong *plVar13;
  longlong lVar14;
  undefined1 *puVar15;
  undefined1 local_128 [32];
  undefined8 local_108;
  undefined4 local_f8 [2];
  undefined1 auStack_f0 [8];
  undefined **local_e8;
  undefined2 local_e0;
  longlong local_d8;
  longlong *local_d0;
  undefined8 local_c8;
  undefined1 *local_c0;
  undefined8 local_b8;
  undefined8 uStack_b0;
  longlong local_88;
  undefined4 local_80;
  undefined4 local_7c;
  undefined8 local_78;
  longlong lStack_70;
  longlong local_68;
  undefined8 local_60;
  undefined1 local_58;
  ulonglong local_48;
  
  puVar9 = local_128;
  local_48 = DAT_144c773c0 ^ (ulonglong)local_f8;
  iVar12 = *(int *)((longlong)param_1 + 0xc);
  puVar15 = (undefined1 *)0x0;
  puVar4 = puVar15;
  if (iVar12 < DAT_144d8f934) {
    uVar7 = iVar12 >> 0x1f & 0xffff;
    uVar3 = iVar12 + uVar7;
    puVar4 = (undefined1 *)
             (*(longlong *)(DAT_144d8f920 + (longlong)((int)uVar3 >> 0x10) * 8) +
             (longlong)(int)((uVar3 & 0xffff) - uVar7) * 0x18);
  }
  puVar11 = local_128;
  if ((*(uint *)(puVar4 + 8) >> 0x1d & 1) == 0) {
    if ((*(uint *)(param_2 + 0xb0) & 0x400) == 0) {
      uVar3 = *(uint *)(param_2 + 0x68);
    }
    else {
      uVar3 = (**(code **)(*param_1 + 0x228))(param_1,param_2,0);
      if ((uVar3 & 1) != 0) {
        local_108 = 0;
        (**(code **)(*param_1 + 0x230))(param_1,param_2,param_3,0);
      }
      uVar3 = uVar3 & 2;
    }
    puVar11 = local_128;
    if (uVar3 != 0) {
      if (*(longlong *)(param_2 + 200) != 0) {
        local_f8[0] = *(undefined4 *)(param_2 + 0xd0);
        param_2 = *(longlong *)(param_2 + 200);
        param_3 = local_f8;
      }
      puVar4 = puVar15;
      if ((*(uint *)(param_2 + 0xb0) & 0x8000) != 0) {
        puVar4 = (undefined1 *)
                 (**(code **)(**(longlong **)(param_2 + 0x20) + 0x338))
                           (*(longlong **)(param_2 + 0x20),param_1,param_2);
      }
      puVar11 = local_128;
      puVar6 = puVar4;
      if (puVar4 == (undefined1 *)0x0) {
        iVar12 = *(int *)(param_2 + 0x58);
        puVar6 = puVar15;
        if (iVar12 != 0) {
          uVar5 = (longlong)(iVar12 + 0xf) + 0xf;
          if (uVar5 <= (ulonglong)(longlong)(iVar12 + 0xf)) {
            uVar5 = 0xffffffffffffff0;
          }
          puVar9 = local_128 + -(uVar5 & 0xfffffffffffffff0);
          puVar6 = auStack_f0 + -(uVar5 & 0xfffffffffffffff0);
        }
        uVar1 = *(ushort *)(param_2 + 0xb6);
        *(undefined8 *)(puVar9 + -8) = 0x141465aa8;
        memset(puVar6 + uVar1,0,(longlong)(int)(iVar12 - (uint)uVar1));
        puVar11 = puVar9;
      }
      uVar1 = *(ushort *)(param_2 + 0xb6);
      *(undefined8 *)(puVar11 + -8) = 0x141465abb;
      memcpy(puVar6,param_3,(ulonglong)uVar1);
      lVar14 = *(longlong *)(param_2 + 0x50);
      local_e8 = &PTR_FUN_143c55d78;
      local_c8 = *(undefined8 *)(param_2 + 0x60);
      local_e0 = 0x100;
      local_b8 = 0;
      uStack_b0 = 0;
      local_88 = 0;
      local_80 = 0;
      local_7c = 8;
      local_78 = 0;
      lStack_70 = 0;
      local_68 = lVar14;
      local_60 = 0;
      local_58 = 0;
      if (((*(uint *)(param_2 + 0xb0) & 0x400000) != 0) &&
         (plVar13 = &lStack_70, puVar10 = puVar11, lVar14 != 0)) {
        do {
          puVar11 = puVar10;
          if (-1 < (char)*(ulonglong *)(lVar14 + 0x40)) break;
          if ((*(ulonglong *)(lVar14 + 0x40) >> 8 & 1) != 0) {
            puVar11 = puVar10 + -0x30;
            plVar8 = (longlong *)((ulonglong)(puVar10 + 0xf) & 0xfffffffffffffff0);
            plVar8[1] = (longlong)*(int *)(lVar14 + 0x4c) + (longlong)param_3;
            *plVar8 = lVar14;
            if (*plVar13 == 0) {
              *plVar13 = (longlong)plVar8;
            }
            else {
              *(longlong **)(*plVar13 + 0x10) = plVar8;
              plVar13 = (longlong *)(*plVar13 + 0x10);
              puVar11 = puVar10 + -0x30;
            }
          }
          lVar14 = *(longlong *)(lVar14 + 0x20);
          puVar10 = puVar11;
        } while (lVar14 != 0);
        if (*plVar13 != 0) {
          *(undefined8 *)(*plVar13 + 0x10) = 0;
        }
      }
      local_d8 = param_2;
      local_d0 = param_1;
      local_c0 = puVar6;
      if (puVar4 == (undefined1 *)0x0) {
        for (plVar13 = *(longlong **)(param_2 + 0xc0); plVar13 != (longlong *)0x0;
            plVar13 = (longlong *)plVar13[4]) {
          if ((*(uint *)(plVar13 + 8) >> 9 & 1) == 0) {
            pcVar2 = *(code **)(*plVar13 + 0xf8);
            *(undefined8 *)(puVar11 + -8) = 0x141465bdc;
            (*pcVar2)(plVar13);
          }
          else {
            iVar12 = *(int *)((longlong)plVar13 + 0x3c);
            lVar14 = plVar13[7];
            puVar6 = local_c0 + *(int *)((longlong)plVar13 + 0x4c);
            *(undefined8 *)(puVar11 + -8) = 0x141465bc7;
            memset(puVar6,0,(longlong)(iVar12 * (int)lVar14));
          }
        }
      }
      if (*(ushort *)(param_2 + 0xb8) != 0xffff) {
        puVar15 = (undefined1 *)((ulonglong)*(ushort *)(param_2 + 0xb8) + (longlong)param_3);
      }
      *(undefined8 *)(puVar11 + -8) = 0x141465c15;
      FUN_141302dc0(param_2,param_1,&local_e8,puVar15);
      if (puVar4 == (undefined1 *)0x0) {
        for (plVar13 = *(longlong **)(param_2 + 0x80); plVar13 != (longlong *)0x0;
            plVar13 = (longlong *)plVar13[0xd]) {
          iVar12 = *(int *)((longlong)plVar13 + 0x3c) * (int)plVar13[7];
          lVar14 = (longlong)*(int *)((longlong)plVar13 + 0x4c);
          if ((int)(uint)*(ushort *)(param_2 + 0xb6) < *(int *)((longlong)plVar13 + 0x4c) + iVar12)
          {
            if ((~(byte)((ulonglong)plVar13[8] >> 0x24) & 1) != 0) {
              puVar4 = local_c0 + lVar14;
              pcVar2 = *(code **)(*plVar13 + 0xf0);
              *(undefined8 *)(puVar11 + -8) = 0x141465c6e;
              (*pcVar2)(plVar13,puVar4);
            }
          }
          else if ((~(byte)((ulonglong)plVar13[8] >> 8) & 1) != 0) {
            puVar4 = local_c0 + lVar14;
            *(undefined8 *)(puVar11 + -8) = 0x141465c8e;
            memcpy((void *)((longlong)param_3 + lVar14),puVar4,(longlong)iVar12);
          }
        }
      }
      local_e8 = &PTR_FUN_143c55d78;
      if (local_88 != 0) {
        *(undefined8 *)(puVar11 + -8) = 0x141465cb0;
        FUN_1411780a0();
      }
    }
  }
  *(undefined8 *)(puVar11 + -8) = 0x141465cbf;
  return;
}

