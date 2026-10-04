
/* WARNING: Function: __security_check_cookie replaced with injection: security_check_cookie */

void FUN_141703750(longlong param_1,int param_2,int param_3,char param_4,undefined4 param_5)

{
  longlong *plVar1;
  uint uVar2;
  undefined4 uVar3;
  int iVar4;
  longlong lVar5;
  longlong lVar6;
  longlong *plVar7;
  int iVar8;
  ulonglong uVar9;
  int iVar10;
  undefined1 auStack_c8 [32];
  int *local_a8;
  int *local_a0;
  undefined8 local_98;
  int local_88;
  int local_84;
  undefined4 local_80;
  char local_7c;
  int local_78;
  int local_74;
  undefined4 local_70;
  undefined1 local_6c;
  undefined4 local_68;
  undefined4 local_64;
  undefined8 local_60;
  undefined4 local_58;
  undefined8 local_54;
  ulonglong local_48;
  
  local_48 = DAT_144c773c0 ^ (ulonglong)auStack_c8;
  FUN_1417044a0(*(undefined8 *)(param_1 + 0x18),0,0,0);
  FUN_1416e4c00(*(undefined8 *)(param_1 + 0x18));
  (**(code **)(**(longlong **)(*(longlong *)(param_1 + 0x18) + 0x158) + 0x378))();
  if (*(longlong **)(param_1 + 0xb0) != (longlong *)0x0) {
    (**(code **)(**(longlong **)(param_1 + 0xb0) + 8))();
  }
  lVar6 = *(longlong *)(param_1 + 0x78);
  plVar7 = (longlong *)0x0;
  if (lVar6 != 0) {
    plVar1 = *(longlong **)(lVar6 + 0x90);
    if (plVar1 != (longlong *)0x0) {
      (**(code **)(*plVar1 + 8))(plVar1);
      (**(code **)(*plVar1 + 0x10))();
      lVar6 = *(longlong *)(param_1 + 0x78);
    }
    uVar2 = -(uint)(*(char *)(lVar6 + 0xb0) != '\0');
    if (uVar2 < *(uint *)(lVar6 + 0xa8)) {
      plVar7 = *(longlong **)(*(longlong *)(lVar6 + 0xa0) + (longlong)(int)uVar2 * 8);
    }
    if (plVar7 != (longlong *)0x0) {
      (**(code **)(*plVar7 + 8))(plVar7);
      (**(code **)(*plVar7 + 0x10))(plVar7);
      lVar6 = *(longlong *)(param_1 + 0x78);
    }
    plVar7 = *(longlong **)(lVar6 + 0x98);
    if (plVar7 != (longlong *)0x0) {
      (**(code **)(*plVar7 + 8))(plVar7);
      (**(code **)(*plVar7 + 0x10))(plVar7);
    }
  }
  lVar6 = *(longlong *)(param_1 + 0x78);
  *(undefined8 *)(param_1 + 0x78) = 0;
  if (lVar6 != 0) {
    (**(code **)(*(longlong *)(lVar6 + 0x68) + 0x10))(lVar6 + 0x68);
  }
  uVar2 = FUN_1416ef3a0(*(undefined8 *)(param_1 + 0x18),param_5);
  iVar4 = *(int *)(param_1 + 0x60);
  uVar9 = (ulonglong)uVar2;
  iVar8 = *(int *)(param_1 + 0x4c);
  iVar10 = *(int *)(param_1 + 0x50);
  local_78 = iVar8;
  local_74 = iVar10;
  local_70 = FUN_1416efce0(iVar4);
  local_6c = *(undefined1 *)(param_1 + 0x68);
  local_88 = param_2;
  local_84 = param_3;
  local_80 = FUN_1416efce0(uVar9 & 0xffffffff);
  local_7c = param_4;
  if (((iVar8 != param_2) || (iVar10 != param_3)) || (iVar4 != (int)uVar9)) {
    *(int *)(param_1 + 0x4c) = param_2;
    *(int *)(param_1 + 0x50) = param_3;
    *(int *)(param_1 + 0x60) = (int)uVar9;
    if (*(char *)(param_1 + 0x89) != '\0') {
      uVar2 = DAT_1449391ec ^ 0x800;
      if (*(byte *)(param_1 + 0x69) == ((byte)(DAT_1449391ec >> 0xb) & 1)) {
        uVar2 = DAT_1449391ec;
      }
      uVar3 = FUN_1416efce0(uVar9 & 0xffffffff);
      local_a0 = (int *)CONCAT44(local_a0._4_4_,uVar2);
      local_a8 = (int *)CONCAT44(local_a8._4_4_,uVar3);
      iVar4 = (**(code **)(**(longlong **)(param_1 + 0x70) + 0x68))
                        (*(longlong **)(param_1 + 0x70),0,param_2,param_3);
      if (iVar4 < 0) {
        local_98 = *(undefined8 *)(*(longlong *)(param_1 + 0x18) + 0x170);
        local_a0 = &local_88;
        local_a8 = &local_78;
        FUN_1417068e0(iVar4,
                      "SwapChain->ResizeBuffers(0, SizeX, SizeY, RenderTargetFormat, SwapChainFlags)"
                      ,
                      "D:/Build/++UE4/Sync/Engine/Source/Runtime/Windows/D3D11RHI/Private/D3D11Viewport.cpp"
                      ,0x12a);
      }
      if (param_4 != '\0') {
        local_68 = *(undefined4 *)(param_1 + 0x4c);
        local_64 = *(undefined4 *)(param_1 + 0x50);
        local_60 = 0;
        local_58 = FUN_1416efce0(*(undefined4 *)(param_1 + 0x60));
        local_54 = 0;
        iVar4 = (**(code **)(**(longlong **)(param_1 + 0x70) + 0x70))
                          (*(longlong **)(param_1 + 0x70),&local_68);
        if (iVar4 < 0) {
          FUN_1417035c0(param_1,1);
          local_a0 = (int *)CONCAT44(local_a0._4_4_,uVar2);
          local_a8 = (int *)CONCAT44(local_a8._4_4_,uVar3);
          iVar4 = (**(code **)(**(longlong **)(param_1 + 0x70) + 0x68))
                            (*(longlong **)(param_1 + 0x70),0,*(undefined4 *)(param_1 + 0x4c),
                             *(undefined4 *)(param_1 + 0x50));
          if (iVar4 < 0) {
            local_98 = *(undefined8 *)(*(longlong *)(param_1 + 0x18) + 0x170);
            local_a0 = &local_88;
            local_a8 = &local_78;
            FUN_1417068e0(iVar4,
                          "SwapChain->ResizeBuffers(0, SizeX, SizeY, RenderTargetFormat, SwapChainFlags)"
                          ,
                          "D:/Build/++UE4/Sync/Engine/Source/Runtime/Windows/D3D11RHI/Private/D3D11Viewport.cpp"
                          ,0x133);
          }
        }
      }
    }
  }
  if (*(char *)(param_1 + 0x68) != param_4) {
    *(char *)(param_1 + 0x68) = param_4;
    LOCK();
    *(undefined4 *)(param_1 + 0x5c) = 1;
    UNLOCK();
    if (*(char *)(param_1 + 0x89) != '\0') {
      FUN_1417035c0(param_1,1);
      uVar3 = FUN_1416efce0(*(undefined4 *)(param_1 + 0x60));
      uVar2 = DAT_1449391ec ^ 0x800;
      if (*(byte *)(param_1 + 0x69) == ((byte)(DAT_1449391ec >> 0xb) & 1)) {
        uVar2 = DAT_1449391ec;
      }
      local_a0 = (int *)CONCAT44(local_a0._4_4_,uVar2);
      local_a8 = (int *)CONCAT44(local_a8._4_4_,uVar3);
      iVar4 = (**(code **)(**(longlong **)(param_1 + 0x70) + 0x68))
                        (*(longlong **)(param_1 + 0x70),0,*(undefined4 *)(param_1 + 0x4c),
                         *(undefined4 *)(param_1 + 0x50));
      if (iVar4 < 0) {
        local_98 = *(undefined8 *)(*(longlong *)(param_1 + 0x18) + 0x170);
        local_a0 = &local_88;
        local_a8 = &local_78;
        FUN_1417068e0(iVar4,
                      "SwapChain->ResizeBuffers(0, SizeX, SizeY, RenderTargetFormat, GetSwapChainFlags())"
                      ,
                      "D:/Build/++UE4/Sync/Engine/Source/Runtime/Windows/D3D11RHI/Private/D3D11Viewport.cpp"
                      ,0x144);
      }
    }
  }
  if ((*(int *)(param_1 + 0x60) == DAT_144956e38) && (*(char *)(param_1 + 0x68) != '\0')) {
    (**(code **)(**(longlong **)(param_1 + 0x18) + 0x748))();
  }
  else {
    (**(code **)(**(longlong **)(param_1 + 0x18) + 0x750))();
  }
  local_a8 = *(int **)(param_1 + 0x70);
  *(undefined4 *)(param_1 + 100) = 0x21;
  lVar5 = FUN_1416f0090(*(undefined8 *)(param_1 + 0x18),*(undefined4 *)(param_1 + 0x60),
                        *(undefined4 *)(param_1 + 0x4c),*(undefined4 *)(param_1 + 0x50));
  lVar6 = *(longlong *)(param_1 + 0x78);
  *(longlong *)(param_1 + 0x78) = lVar5;
  if (lVar5 != 0) {
    (**(code **)(*(longlong *)(lVar5 + 0x68) + 8))(lVar5 + 0x68);
  }
  if (lVar6 != 0) {
    (**(code **)(*(longlong *)(lVar6 + 0x68) + 0x10))(lVar6 + 0x68);
  }
  return;
}

