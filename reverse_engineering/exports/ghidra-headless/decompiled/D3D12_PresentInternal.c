
undefined8 FUN_14177e0e0(longlong param_1,int param_2)

{
  longlong *plVar1;
  undefined8 uVar2;
  undefined4 uVar3;
  
  uVar3 = 0;
  if (((param_2 == 0) && (uVar3 = 0, *(char *)(param_1 + 0x54) == '\0')) &&
     (uVar3 = 0, *(char *)(param_1 + 0x5d) != '\0')) {
    uVar3 = 0x200;
  }
  uVar2 = FUN_1411783e0();
  FUN_141185f40(uVar2);
  plVar1 = *(longlong **)(param_1 + 0x60);
  if (plVar1 != (longlong *)0x0) {
                    /* WARNING: Could not recover jumptable at 0x00014177e137. Too many branches */
                    /* WARNING: Treating indirect jump as call */
    uVar2 = (**(code **)(*plVar1 + 0x40))(plVar1,param_2,uVar3);
    return uVar2;
  }
  return 0;
}

