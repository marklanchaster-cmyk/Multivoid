
/* WARNING: Function: __security_check_cookie replaced with injection: security_check_cookie */

char FUN_1416f4ba0(longlong param_1,int param_2)

{
  longlong *plVar1;
  uint uVar2;
  char cVar3;
  int iVar4;
  undefined8 uVar5;
  undefined1 auStack_c8 [32];
  undefined8 local_a8;
  int local_98 [2];
  int local_90 [2];
  longlong *local_88 [2];
  undefined1 local_78 [80];
  ulonglong local_28;
  
  local_28 = DAT_144c773c0 ^ (ulonglong)auStack_c8;
  cVar3 = '\x01';
  plVar1 = *(longlong **)(param_1 + 0xb0);
  iVar4 = 0;
  local_98[0] = param_2;
  if (plVar1 != (longlong *)0x0) {
    cVar3 = (**(code **)(*plVar1 + 0x28))(plVar1,local_98);
    if (cVar3 == '\0') goto LAB_1416f4c93;
  }
  plVar1 = *(longlong **)(param_1 + 0x70);
  iVar4 = 0;
  if (plVar1 != (longlong *)0x0) {
    local_88[0] = (longlong *)0x0;
    (**(code **)(*plVar1 + 0x58))(plVar1,local_90,local_88);
    if ((local_90[0] != 0) == (bool)*(char *)(param_1 + 0x68)) {
      uVar5 = 0;
      uVar2 = DAT_1449391ec ^ 0x800;
      if (*(byte *)(param_1 + 0x69) == ((byte)(DAT_1449391ec >> 0xb) & 1)) {
        uVar2 = DAT_1449391ec;
      }
      if ((((uVar2 >> 0xb & 1) != 0) && (local_98[0] == 0)) &&
         (uVar5 = 0, *(char *)(param_1 + 0x68) == '\0')) {
        uVar5 = 0x200;
      }
      iVar4 = (**(code **)(**(longlong **)(param_1 + 0x70) + 0x40))
                        (*(longlong **)(param_1 + 0x70),local_98[0],uVar5);
    }
    else {
      LOCK();
      *(undefined4 *)(param_1 + 0x5c) = 3;
      UNLOCK();
      iVar4 = 0;
    }
    if (local_88[0] != (longlong *)0x0) {
      (**(code **)(*local_88[0] + 0x10))();
    }
  }
  if (*(longlong **)(param_1 + 0xb0) != (longlong *)0x0) {
    (**(code **)(**(longlong **)(param_1 + 0xb0) + 0x30))();
  }
LAB_1416f4c93:
  uVar5 = FUN_1411783e0();
  FUN_141185f40(uVar5);
  if (iVar4 < 0) {
    *(int *)(param_1 + 0x58) = *(int *)(param_1 + 0x58) + 1;
    (**(code **)(**(longlong **)(param_1 + 0x70) + 0x60))(*(longlong **)(param_1 + 0x70),local_78);
    if (*(uint *)(param_1 + 0x58) < 0xb) {
      LOCK();
      *(undefined4 *)(param_1 + 0x5c) = 3;
      UNLOCK();
    }
    else {
      local_a8 = *(undefined8 *)(*(longlong *)(param_1 + 0x18) + 0x170);
      FUN_141706b30(iVar4,"Result",
                    "D:/Build/++UE4/Sync/Engine/Source/Runtime/Windows/D3D11RHI/Private/D3D11Viewport.cpp"
                    ,0x1bb);
    }
  }
  else {
    *(undefined4 *)(param_1 + 0x58) = 0;
  }
  uVar5 = FUN_1411783e0();
  FUN_141185f40(uVar5);
  plVar1 = *(longlong **)(*(longlong *)(param_1 + 0x18) + 0x158);
  (**(code **)(*plVar1 + 0x108))(plVar1,0,0,0);
  return cVar3;
}

