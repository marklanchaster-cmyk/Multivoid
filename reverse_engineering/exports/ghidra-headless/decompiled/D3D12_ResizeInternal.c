
void FUN_14177e8b0(longlong param_1)

{
  int iVar1;
  int iVar2;
  undefined4 uVar3;
  undefined4 uVar4;
  undefined4 uVar5;
  longlong lVar6;
  undefined4 uVar7;
  longlong lVar8;
  longlong lVar9;
  ulonglong uVar10;
  longlong *plVar11;
  longlong lVar12;
  longlong *plVar13;
  int iVar14;
  ulonglong uVar15;
  uint uVar16;
  uint uVar17;
  longlong *local_res8;
  undefined4 local_res10;
  longlong local_res18;
  undefined8 in_stack_ffffffffffffff78;
  undefined4 uVar21;
  undefined8 uVar19;
  undefined8 uVar20;
  undefined8 *in_stack_ffffffffffffff80;
  undefined8 *puVar22;
  undefined8 in_stack_ffffffffffffff88;
  undefined8 in_stack_ffffffffffffff90;
  undefined4 uVar23;
  ulonglong local_68;
  uint local_60;
  int local_5c;
  longlong local_58;
  int local_50;
  int local_4c;
  undefined8 local_48;
  undefined8 local_40;
  ulonglong uVar18;
  
  uVar3 = (undefined4)((ulonglong)in_stack_ffffffffffffff88 >> 0x20);
  uVar23 = (undefined4)((ulonglong)in_stack_ffffffffffffff90 >> 0x20);
  lVar12 = *(longlong *)(param_1 + 0x18);
  local_res18 = lVar12;
  FUN_141767db0(param_1,3);
  uVar21 = (undefined4)((ulonglong)in_stack_ffffffffffffff78 >> 0x20);
  uVar7 = 0x802;
  if (*(char *)(param_1 + 0x5d) == '\0') {
    uVar7 = 2;
  }
  local_res10 = uVar7;
  if (DAT_144956e14 < 2) {
    plVar13 = *(longlong **)(param_1 + 0x60);
    if (plVar13 != (longlong *)0x0) {
      lVar6 = *plVar13;
      uVar4 = FUN_1416efce0(*(undefined4 *)(param_1 + 0x58));
      in_stack_ffffffffffffff80 =
           (undefined8 *)CONCAT44((int)((ulonglong)in_stack_ffffffffffffff80 >> 0x20),uVar7);
      uVar19 = CONCAT44(uVar21,uVar4);
      iVar2 = (**(code **)(lVar6 + 0x68))
                        (plVar13,*(undefined4 *)(param_1 + 0x90),*(undefined4 *)(param_1 + 0x4c),
                         *(undefined4 *)(param_1 + 0x50),uVar19,in_stack_ffffffffffffff80);
      uVar21 = (undefined4)((ulonglong)uVar19 >> 0x20);
      if (iVar2 < 0) {
        uVar4 = *(undefined4 *)(param_1 + 0x58);
        uVar5 = FUN_1416efce0(uVar4);
        FUN_14113cac0(&local_48,L"Num=%d, Size=(%d,%d), PF=%d, DXGIFormat=0x%x, Flags=0x%x",
                      *(undefined4 *)(param_1 + 0x90),*(undefined4 *)(param_1 + 0x4c),
                      CONCAT44(uVar21,*(undefined4 *)(param_1 + 0x50)),uVar4,CONCAT44(uVar3,uVar5),
                      CONCAT44(uVar23,uVar7));
        in_stack_ffffffffffffff80 = &local_48;
        uVar19 = *(undefined8 *)(lVar12 + 0x18);
        FUN_14177a2e0(iVar2,
                      "SwapChain1->ResizeBuffers(NumBackBuffers, SizeX, SizeY, GetRenderTargetFormat(PixelFormat), SwapChainFlags)"
                      ,
                      "D:/Build/++UE4/Sync/Engine/Source/Runtime/D3D12RHI/Private/Windows/WindowsD3D12Viewport.cpp"
                      ,0x178,uVar19,in_stack_ffffffffffffff80);
        uVar21 = (undefined4)((ulonglong)uVar19 >> 0x20);
      }
    }
    uVar19 = *(undefined8 *)(lVar12 + 0x988);
    uVar17 = 0;
    if (*(int *)(param_1 + 0x90) != 0) {
      do {
        uVar20 = *(undefined8 *)(param_1 + 0x60);
        lVar12 = *(longlong *)(param_1 + 0x80);
        in_stack_ffffffffffffff80 =
             (undefined8 *)CONCAT44((int)((ulonglong)in_stack_ffffffffffffff80 >> 0x20),uVar17);
        local_res8 = (longlong *)0x0;
        lVar9 = FUN_14176d970(uVar19,*(undefined4 *)(param_1 + 0x58),*(undefined4 *)(param_1 + 0x4c)
                              ,*(undefined4 *)(param_1 + 0x50),uVar20,in_stack_ffffffffffffff80,
                              &local_res8);
        uVar21 = (undefined4)((ulonglong)uVar20 >> 0x20);
        lVar6 = *(longlong *)(lVar12 + (longlong)(int)uVar17 * 8);
        *(longlong *)(lVar12 + (longlong)(int)uVar17 * 8) = lVar9;
        if (lVar9 != 0) {
          (**(code **)(*(longlong *)(lVar9 + 0x68) + 8))(lVar9 + 0x68);
        }
        if (lVar6 != 0) {
          (**(code **)(*(longlong *)(lVar6 + 0x68) + 0x10))(lVar6 + 0x68);
        }
        uVar17 = uVar17 + 1;
      } while (uVar17 < *(uint *)(param_1 + 0x90));
    }
  }
  else {
    plVar13 = (longlong *)(param_1 + 0xe8);
    uVar15 = 0;
    local_58 = 0;
    local_res8 = (longlong *)((ulonglong)local_res8 & 0xffffffff00000000);
    local_4c = 0;
    local_68 = 0;
    local_5c = 0;
    *(undefined4 *)(param_1 + 0xf0) = 0;
    if (*(int *)(param_1 + 0xf4) != *(int *)(param_1 + 0x90)) {
      FUN_1408538f0(plVar13);
    }
    uVar17 = 0;
    uVar10 = uVar15;
    uVar18 = uVar15;
    if (*(int *)(param_1 + 0x90) != 0) {
      do {
        iVar2 = *(int *)(param_1 + 0xb0);
        if (iVar2 < 0) {
          iVar2 = (int)(uVar18 % (ulonglong)DAT_144956e18);
        }
        iVar14 = *(int *)(param_1 + 0xf0);
        iVar1 = iVar14 + 1;
        *(int *)(param_1 + 0xf0) = iVar1;
        if (*(int *)(param_1 + 0xf4) < iVar1) {
          FUN_1407f35f0(plVar13);
        }
        uVar16 = (int)uVar18 + 1;
        uVar18 = (ulonglong)uVar16;
        *(int *)(*plVar13 + (longlong)iVar14 * 4) = iVar2;
        uVar17 = *(uint *)(param_1 + 0x90);
        uVar10 = local_68;
      } while (uVar16 < uVar17);
    }
    uVar21 = (undefined4)((ulonglong)in_stack_ffffffffffffff78 >> 0x20);
    if (uVar17 != 0) {
      uVar18 = uVar15;
      iVar2 = (int)local_res8;
      do {
        lVar12 = *(longlong *)
                  (local_res18 + 0x988 +
                  (ulonglong)*(uint *)(*plVar13 + (longlong)(int)uVar18 * 4) * 8);
        lVar6 = FUN_141720e30(lVar12,0);
        lVar9 = (longlong)iVar2;
        local_res8 = *(longlong **)(lVar6 + 0x28);
        local_50 = iVar2 + 1;
        if (local_4c < local_50) {
          FUN_1407f3670(&local_58,iVar2);
        }
        iVar2 = local_50;
        *(longlong **)(local_58 + lVar9 * 8) = local_res8;
        if (DAT_144e82b3c == 0) {
          uVar7 = *(undefined4 *)(lVar12 + 0x10);
        }
        else {
          uVar7 = 1;
        }
        iVar14 = (int)uVar15;
        local_60 = iVar14 + 1;
        if (local_5c < (int)local_60) {
          FUN_1407f35f0(&local_68);
        }
        uVar21 = (undefined4)((ulonglong)in_stack_ffffffffffffff78 >> 0x20);
        uVar15 = (ulonglong)local_60;
        uVar17 = (int)uVar18 + 1;
        uVar18 = (ulonglong)uVar17;
        *(undefined4 *)(local_68 + (longlong)iVar14 * 4) = uVar7;
        uVar10 = local_68;
      } while (uVar17 < *(uint *)(param_1 + 0x90));
    }
    uVar17 = 0;
    puVar22 = *(undefined8 **)(param_1 + 0x60);
    if (puVar22 != (undefined8 *)0x0) {
      local_res8 = (longlong *)0x0;
      iVar2 = (**(code **)*puVar22)(puVar22,&DAT_143cabd98,&local_res8);
      uVar7 = (undefined4)((ulonglong)in_stack_ffffffffffffff80 >> 0x20);
      if (iVar2 < 0) {
        puVar22 = &local_48;
        local_48 = 0;
        local_40 = 0;
        uVar21 = 0;
        FUN_14177a2e0(iVar2,
                      "SwapChain1->QueryInterface(IID_PPV_ARGS(SwapChain3.GetInitReference()))",
                      "D:/Build/++UE4/Sync/Engine/Source/Runtime/D3D12RHI/Private/Windows/WindowsD3D12Viewport.cpp"
                      ,0x161,0,puVar22);
        uVar7 = (undefined4)((ulonglong)puVar22 >> 0x20);
      }
      lVar12 = *local_res8;
      plVar11 = local_res8;
      uVar3 = FUN_1416efce0(*(undefined4 *)(param_1 + 0x58));
      in_stack_ffffffffffffff80 = (undefined8 *)CONCAT44(uVar7,local_res10);
      uVar19 = CONCAT44(uVar21,uVar3);
      iVar2 = (**(code **)(lVar12 + 0x138))
                        (plVar11,*(undefined4 *)(param_1 + 0x90),*(undefined4 *)(param_1 + 0x4c),
                         *(undefined4 *)(param_1 + 0x50),uVar19,in_stack_ffffffffffffff80,uVar10,
                         local_58);
      uVar21 = (undefined4)((ulonglong)uVar19 >> 0x20);
      if (iVar2 < 0) {
        in_stack_ffffffffffffff80 = &local_48;
        local_48 = 0;
        local_40 = 0;
        uVar19 = *(undefined8 *)(local_res18 + 0x18);
        FUN_14177a2e0(iVar2,
                      "SwapChain3->ResizeBuffers1(NumBackBuffers, SizeX, SizeY, GetRenderTargetFormat(PixelFormat), SwapChainFlags, NodeMasks.GetData(), (IUnknown**)CommandQueues.GetData())"
                      ,
                      "D:/Build/++UE4/Sync/Engine/Source/Runtime/D3D12RHI/Private/Windows/WindowsD3D12Viewport.cpp"
                      ,0x162,uVar19,in_stack_ffffffffffffff80);
        uVar21 = (undefined4)((ulonglong)uVar19 >> 0x20);
      }
      if (local_res8 != (longlong *)0x0) {
        (**(code **)(*local_res8 + 0x10))();
      }
    }
    if (*(int *)(param_1 + 0x90) != 0) {
      do {
        lVar12 = *(longlong *)(param_1 + 0x80);
        lVar8 = (longlong)(int)uVar17;
        in_stack_ffffffffffffff80 =
             (undefined8 *)CONCAT44((int)((ulonglong)in_stack_ffffffffffffff80 >> 0x20),uVar17);
        local_res8 = (longlong *)0x0;
        uVar19 = *(undefined8 *)(param_1 + 0x60);
        lVar9 = FUN_14176d970(*(undefined8 *)
                               (local_res18 + 0x988 + (ulonglong)*(uint *)(*plVar13 + lVar8 * 4) * 8
                               ),*(undefined4 *)(param_1 + 0x58),*(undefined4 *)(param_1 + 0x4c),
                              *(undefined4 *)(param_1 + 0x50),uVar19,in_stack_ffffffffffffff80,
                              &local_res8);
        uVar21 = (undefined4)((ulonglong)uVar19 >> 0x20);
        lVar6 = *(longlong *)(lVar12 + lVar8 * 8);
        *(longlong *)(lVar12 + lVar8 * 8) = lVar9;
        if (lVar9 != 0) {
          (**(code **)(*(longlong *)(lVar9 + 0x68) + 8))(lVar9 + 0x68);
        }
        if (lVar6 != 0) {
          (**(code **)(*(longlong *)(lVar6 + 0x68) + 0x10))(lVar6 + 0x68);
        }
        uVar17 = uVar17 + 1;
        uVar10 = local_68;
      } while (uVar17 < *(uint *)(param_1 + 0x90));
    }
    if (uVar10 != 0) {
      FUN_1411780a0(uVar10);
    }
    if (local_58 != 0) {
      FUN_1411780a0(local_58);
    }
  }
  lVar6 = local_res18;
  lVar8 = 0;
  *(undefined4 *)(param_1 + 0xa0) = 0;
  *(undefined8 *)(param_1 + 0xa8) = **(undefined8 **)(param_1 + 0x80);
  *(undefined8 *)(param_1 + 0x110) = **(undefined8 **)(param_1 + 0xf8);
  uVar19 = CONCAT44(uVar21,*(undefined4 *)(param_1 + 0x50));
  lVar9 = FUN_141769280(param_1,local_res18,*(undefined4 *)(param_1 + 0x58),
                        *(undefined4 *)(param_1 + 0x4c),uVar19,
                        (ulonglong)in_stack_ffffffffffffff80 & 0xffffffffffffff00);
  uVar7 = (undefined4)((ulonglong)uVar19 >> 0x20);
  lVar12 = *(longlong *)(param_1 + 0x98);
  *(longlong *)(param_1 + 0x98) = lVar9;
  if (lVar9 != 0) {
    (**(code **)(*(longlong *)(lVar9 + 0x68) + 8))(lVar9 + 0x68);
  }
  if (lVar12 != 0) {
    (**(code **)(*(longlong *)(lVar12 + 0x68) + 0x10))(lVar12 + 0x68);
  }
  if (*(longlong *)(param_1 + 0x110) != 0) {
    lVar8 = FUN_141769280(param_1,lVar6,*(undefined4 *)(param_1 + 0x58),
                          *(undefined4 *)(param_1 + 0x4c),
                          CONCAT44(uVar7,*(undefined4 *)(param_1 + 0x50)),1);
  }
  lVar12 = *(longlong *)(param_1 + 0x108);
  *(longlong *)(param_1 + 0x108) = lVar8;
  if (lVar8 != 0) {
    (**(code **)(*(longlong *)(lVar8 + 0x68) + 8))(lVar8 + 0x68);
  }
  if (lVar12 != 0) {
    (**(code **)(*(longlong *)(lVar12 + 0x68) + 0x10))(lVar12 + 0x68);
  }
  return;
}

