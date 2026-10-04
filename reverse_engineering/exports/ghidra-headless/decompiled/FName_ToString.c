
longlong * FUN_14127d870(uint *param_1,longlong *param_2)

{
  uint uVar1;
  uint uVar2;
  int iVar3;
  ushort *puVar4;
  undefined *puVar5;
  uint uVar6;
  longlong local_28;
  int local_20;
  uint local_1c;
  
  uVar6 = param_1[1];
  uVar2 = *param_1 >> 0x10;
  uVar1 = *param_1 & 0xffff;
  if (uVar6 == 0) {
    if (DAT_144d37c24 == '\0') {
      puVar5 = (undefined *)FUN_141270dd0(&DAT_144d535c0);
      DAT_144d37c24 = '\x01';
    }
    else {
      puVar5 = &DAT_144d535c0;
    }
    FUN_141277050((ulonglong)(uVar1 * 2) + *(longlong *)(puVar5 + (ulonglong)uVar2 * 8 + 0x10),
                  param_2);
    return param_2;
  }
  local_28 = 0;
  local_1c = 0;
  if (DAT_144d37c24 == '\0') {
    puVar5 = (undefined *)FUN_141270dd0(&DAT_144d535c0);
    uVar6 = param_1[1];
    DAT_144d37c24 = '\x01';
  }
  else {
    puVar5 = &DAT_144d535c0;
  }
  puVar4 = (ushort *)((ulonglong)(uVar1 * 2) + *(longlong *)(puVar5 + (ulonglong)uVar2 * 8 + 0x10));
  uVar1 = (uint)(*puVar4 >> 6);
  local_20 = 0;
  if (uVar6 == 0) {
    if (local_1c != uVar1) {
      FUN_1407f3e20(&local_28,uVar1);
    }
    FUN_141274fe0(puVar4,&local_28);
  }
  else {
    if (local_1c != uVar1 + 6) {
      FUN_1407f3e20(&local_28,uVar1 + 6);
    }
    FUN_141274fe0(puVar4,&local_28);
    iVar3 = local_20 + -1;
    if (local_20 < 1) {
      iVar3 = 0;
    }
    local_20 = (local_20 < 1) + 1 + local_20;
    if ((int)local_1c < local_20) {
      FUN_1407f3920(&local_28);
    }
    *(undefined2 *)(local_28 + (longlong)iVar3 * 2) = 0x5f;
    *(undefined2 *)(local_28 + 2 + (longlong)iVar3 * 2) = 0;
    FUN_14112fea0(&local_28,param_1[1] - 1);
  }
  *param_2 = local_28;
  *(int *)(param_2 + 1) = local_20;
  *(uint *)((longlong)param_2 + 0xc) = local_1c;
  return param_2;
}

