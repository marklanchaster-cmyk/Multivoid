
undefined8 * FUN_1412e0310(longlong param_1,int param_2)

{
  uint uVar1;
  undefined8 uVar2;
  undefined8 *puVar3;
  longlong lVar4;
  ulonglong uVar5;
  ulonglong uVar6;
  int iVar7;
  int *piVar8;
  longlong lVar9;
  undefined8 *puVar10;
  short *psVar11;
  short *psVar12;
  longlong local_res18;
  longlong local_res20;
  ulonglong local_68;
  uint local_60;
  int local_5c;
  longlong local_58;
  uint local_50;
  int local_4c;
  longlong local_48 [2];
  
  puVar10 = (undefined8 *)
            ((longlong)param_2 * 0x38 + *(longlong *)(*(longlong *)(param_1 + 0x50) + 8));
  if ((puVar10[4] == 0) && (*(char *)((longlong)puVar10 + 0x35) == '\0')) {
    uVar1 = *(uint *)(puVar10 + 1);
    *(undefined1 *)((longlong)puVar10 + 0x35) = 1;
    if (uVar1 == 0) {
      lVar4 = *(longlong *)(param_1 + 0x50);
      uVar2 = FUN_141437e60();
      puVar3 = (undefined8 *)FUN_1412ee910(lVar4 + 0x2e8,&local_res18,puVar10);
      uVar2 = FUN_14146e460(uVar2,0,*puVar3,1,0,0,0);
      puVar10[4] = uVar2;
    }
    else if (((int)uVar1 < 0) &&
            (lVar4 = FUN_1412e0310(param_1,~uVar1), *(longlong *)(lVar4 + 0x20) != 0)) {
      uVar2 = FUN_141481ef0();
      uVar6 = 0;
      lVar4 = FUN_14146e460(uVar2,*(undefined8 *)(lVar4 + 0x20),*puVar10,0,1,0,0);
      puVar10[4] = lVar4;
      if (lVar4 != 0) {
        local_res20 = *(longlong *)((longlong)puVar10 + 0x14);
        local_res18 = *(longlong *)(*(longlong *)(lVar4 + 0x10) + 0x18);
        if (local_res18 != local_res20) {
          piVar8 = (int *)(*(longlong *)
                            ((longlong)ThreadLocalStoragePointer + (ulonglong)_tls_index * 8) +
                          0x108);
          if ((*piVar8 < DAT_144d8c830) && (FUN_143953688(&DAT_144d8c830), DAT_144d8c830 == -1)) {
            FUN_1412709f0(&DAT_144d8c828,"BlueprintGeneratedClass",1);
            _Init_thread_footer(&DAT_144d8c830);
          }
          if ((*piVar8 < DAT_144d8c840) && (FUN_143953688(&DAT_144d8c840), DAT_144d8c840 == -1)) {
            FUN_1412709f0(&DAT_144d8c838,"DynamicClass",1);
            _Init_thread_footer(&DAT_144d8c840);
          }
          if ((*piVar8 < DAT_144d8c850) && (FUN_143953688(&DAT_144d8c850), DAT_144d8c850 == -1)) {
            FUN_1412709f0(&DAT_144d8c848,"Function",1);
            _Init_thread_footer(&DAT_144d8c850);
          }
          if ((*piVar8 < DAT_144d8c860) && (FUN_143953688(&DAT_144d8c860), DAT_144d8c860 == -1)) {
            FUN_1412709f0(&DAT_144d8c858,"DelegateFunction",1);
            _Init_thread_footer(&DAT_144d8c860);
          }
          if (((local_res20 != DAT_144d8c828) || (local_res18 != DAT_144d8c838)) &&
             ((local_res20 != DAT_144d8c848 || (local_res18 != DAT_144d8c858)))) {
            puVar3 = (undefined8 *)FUN_14127d870(&local_res18,&local_68);
            psVar11 = &DAT_143a18228;
            if (*(int *)(puVar3 + 1) == 0) {
              psVar12 = &DAT_143a18228;
            }
            else {
              psVar12 = (short *)*puVar3;
            }
            local_58 = 0;
            local_50 = 0;
            local_4c = 0;
            lVar4 = -1;
            if ((psVar12 != (short *)0x0) && (*psVar12 != 0)) {
              lVar9 = -1;
              do {
                lVar9 = lVar9 + 1;
              } while (psVar12[lVar9] != 0);
              iVar7 = (int)lVar9 + 1;
              uVar5 = uVar6;
              if (0 < iVar7) {
                FUN_1407f3e20(&local_58,iVar7);
                uVar5 = (ulonglong)local_50;
              }
              local_50 = (int)uVar5 + iVar7;
              if (local_4c < (int)local_50) {
                FUN_1407f3920(&local_58);
              }
              FUN_141156950(local_58,psVar12,(longlong)iVar7 * 2);
            }
            if (local_68 != 0) {
              FUN_1411780a0();
            }
            puVar3 = (undefined8 *)FUN_14127d870(&local_res20,local_48);
            if (*(int *)(puVar3 + 1) == 0) {
              psVar12 = &DAT_143a18228;
            }
            else {
              psVar12 = (short *)*puVar3;
            }
            local_68 = 0;
            local_60 = 0;
            local_5c = 0;
            uVar5 = uVar6;
            if ((psVar12 != (short *)0x0) && (*psVar12 != 0)) {
              lVar9 = -1;
              do {
                lVar9 = lVar9 + 1;
              } while (psVar12[lVar9] != 0);
              iVar7 = (int)lVar9 + 1;
              if (0 < iVar7) {
                FUN_1407f3e20(&local_68,iVar7);
                uVar5 = (ulonglong)local_60;
              }
              local_60 = iVar7 + (int)uVar5;
              if (local_5c < (int)local_60) {
                FUN_1407f3920(&local_68);
              }
              uVar5 = local_68;
              FUN_141156950(local_68,psVar12,(longlong)iVar7 * 2);
            }
            if (local_48[0] != 0) {
              FUN_1411780a0();
            }
            puVar3 = (undefined8 *)FUN_14127d870(param_1 + 0x14,local_48);
            if (*(int *)(puVar3 + 1) != 0) {
              psVar11 = (short *)*puVar3;
            }
            local_68 = 0;
            local_60 = 0;
            local_5c = 0;
            if ((psVar11 != (short *)0x0) && (*psVar11 != 0)) {
              do {
                lVar4 = lVar4 + 1;
              } while (psVar11[lVar4] != 0);
              iVar7 = (int)lVar4 + 1;
              if (0 < iVar7) {
                FUN_1407f3e20(&local_68,iVar7);
                uVar6 = (ulonglong)local_60;
              }
              local_60 = (int)uVar6 + iVar7;
              if (local_5c < (int)local_60) {
                FUN_1407f3920(&local_68);
              }
              uVar6 = local_68;
              FUN_141156950(local_68,psVar11,(longlong)iVar7 * 2);
            }
            if (local_48[0] != 0) {
              FUN_1411780a0();
            }
            if (uVar6 != 0) {
              FUN_1411780a0(uVar6);
            }
            if (uVar5 != 0) {
              FUN_1411780a0(uVar5);
            }
            if (local_58 != 0) {
              FUN_1411780a0(local_58);
            }
          }
        }
      }
    }
    if (puVar10[4] != 0) {
      FUN_1412d3360(param_1);
    }
  }
  return puVar10;
}

