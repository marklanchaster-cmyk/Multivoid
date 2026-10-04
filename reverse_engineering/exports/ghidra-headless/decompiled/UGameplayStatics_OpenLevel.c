
void FUN_142b530b0(undefined8 param_1,undefined8 param_2,byte param_3,longlong *param_4)

{
  void *_Src;
  char cVar1;
  longlong lVar2;
  int iVar3;
  void *pvVar4;
  undefined2 *puVar5;
  int iVar6;
  void *pvVar7;
  int iVar8;
  void *pvVar9;
  int iVar10;
  undefined8 local_res10;
  void *local_e8;
  undefined8 local_e0;
  int local_d8;
  undefined2 *local_d0;
  int local_c8;
  longlong local_c0;
  void *local_b8;
  int local_b0;
  longlong local_a8;
  undefined1 local_98 [40];
  undefined1 local_70 [72];
  
  local_res10 = param_2;
  lVar2 = FUN_142f31680(DAT_144ed3968,param_1,1);
  local_c0 = lVar2;
  if (lVar2 != 0) {
    local_d8 = (param_3 ^ 1) * 2;
    local_a8 = FUN_142f31630(DAT_144ed3968,lVar2);
    FUN_14127d870(&local_res10,&local_d0);
    pvVar4 = (void *)0x0;
    iVar3 = (int)param_4[1] + -1;
    if ((int)param_4[1] == 0) {
      iVar3 = 0;
    }
    if (0 < iVar3) {
      local_e8 = (void *)0x0;
      local_e0 = 0;
      FUN_1407f3e20(&local_e8,2);
      iVar6 = (int)local_e0 + 2;
      local_e0 = CONCAT44(local_e0._4_4_,iVar6);
      iVar3 = local_e0._4_4_;
      if (local_e0._4_4_ < iVar6) {
        FUN_1407f3920(&local_e8);
        iVar3 = local_e0._4_4_;
        iVar6 = (int)local_e0;
      }
      pvVar9 = local_e8;
      FUN_141156950(local_e8,&DAT_143c04ec8,4);
      iVar8 = (int)param_4[1];
      if (iVar6 < 2) {
        _Src = (void *)*param_4;
        local_b8 = (void *)0x0;
        pvVar7 = pvVar4;
        local_b0 = iVar8;
        if (iVar8 != 0) {
          FUN_1407f3570(&local_b8,iVar8,0);
          pvVar7 = local_b8;
          memcpy(local_b8,_Src,(longlong)iVar8 * 2);
          iVar8 = local_b0;
        }
      }
      else {
        iVar10 = iVar8 + -1;
        if (iVar8 == 0) {
          iVar10 = 0;
        }
        if ((iVar6 != 0) || (iVar8 = 1, iVar10 == 0)) {
          iVar8 = 0;
        }
        local_e0 = CONCAT44(iVar3,iVar6);
        local_e8 = pvVar9;
        if (iVar3 < iVar10 + iVar8 + iVar6) {
          FUN_1407f3e20(&local_e8);
        }
        FUN_14112fe00(&local_e8,*param_4,iVar10);
        pvVar7 = local_e8;
        iVar8 = (int)local_e0;
        local_e8 = (void *)0x0;
        local_e0 = 0;
        pvVar9 = pvVar4;
      }
      if (iVar8 != 0) {
        pvVar4 = (void *)(ulonglong)(iVar8 - 1);
      }
      FUN_14112fe00(&local_d0,pvVar7,pvVar4);
      if (pvVar7 != (void *)0x0) {
        FUN_1411780a0(pvVar7);
      }
      lVar2 = local_c0;
      if (pvVar9 != (void *)0x0) {
        FUN_1411780a0(pvVar9);
        lVar2 = local_c0;
      }
    }
    iVar3 = local_d8;
    puVar5 = &DAT_143a18228;
    if (local_c8 != 0) {
      puVar5 = local_d0;
    }
    FUN_142f4aaf0(local_98,local_a8 + 0xd0,puVar5,local_d8);
    cVar1 = FUN_142f65400(local_98);
    if (cVar1 != '\0') {
      FUN_142f3b800(DAT_144ed3968,local_70);
    }
    puVar5 = &DAT_143a18228;
    if (local_c8 != 0) {
      puVar5 = local_d0;
    }
    FUN_142f41080(DAT_144ed3968,lVar2,puVar5,iVar3);
    FUN_14106d260(local_98);
    if (local_d0 != (undefined2 *)0x0) {
      FUN_1411780a0();
    }
  }
  if (*param_4 != 0) {
    FUN_1411780a0();
  }
  return;
}

