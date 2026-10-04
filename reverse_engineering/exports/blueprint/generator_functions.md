# Blueprint bytecode symbol summary

Source: `research/bp_reflection/generator.json`

This is a structural inventory of UAssetAPI's decoded Kismet bytecode; it is not native Ghidra output and does not claim source-level control flow.

## `fullFix`

- Bytecode size: 903 bytes
- Serialized export: offset 78314, size 2351
- Field/property paths: `sine_amplitude`, `panelObj`, `targetSine_amplitude`, `sine_frequency`, `targetSine_frequency`, `sine_offset`, `targetSine_offset`, `switches_target`, `CallFunc_ByteToBits_Bit1`, `CallFunc_ByteToBits_Bit2`, `CallFunc_ByteToBits_Bit3`, `CallFunc_ByteToBits_Bit4`, `CallFunc_ByteToBits_Bit5`, `CallFunc_ByteToBits_Bit6`, `CallFunc_ByteToBits_Bit7`, `CallFunc_ByteToBits_Bit8`, `K2Node_MakeArray_Array`, `switches_states`, `Temp_int_Loop_Counter_Variable`, `Temp_int_Array_Index_Variable`, `CallFunc_Array_Length_ReturnValue`, `rotators_states`, `CallFunc_Less_IntInt_ReturnValue`, `Temp_byte_Variable`, `cycle`, `isBroken`, `turnedOn`, `CallFunc_Add_IntInt_ReturnValue`
- Virtual calls: `upd`
- Resolved stack nodes: `-191 (ByteToBits)`, `-152 (Array_Length)`, `-165 (Less_IntInt)`, `-153 (Array_Set)`, `1 (turnedOn__DelegateSignature)`, `-155 (Add_IntInt)`
- Literals: `EX_IntConst=0`, `False`, `EX_IntConst=100`, `EX_IntConst=1`
- Expression mix: `EX_LocalVariable` x33, `EX_InstanceVariable` x23, `EX_Context` x12, `EX_Let` x11, `EX_IntConst` x4, `EX_PushExecutionFlow` x3, `EX_CallMath` x3, `EX_ObjectConst` x2, `EX_FinalFunction` x2, `EX_LetBool` x2, `EX_False` x2, `EX_PopExecutionFlow` x2, `EX_SetArray` x1, `EX_PopExecutionFlowIfNot` x1, `EX_CallMulticastDelegate` x1, `EX_LocalVirtualFunction` x1, `EX_Jump` x1, `EX_Return` x1, `EX_Nothing` x1, `EX_EndOfScript` x1

## `upd`

- Bytecode size: 395 bytes
- Serialized export: offset 63778, size 551
- Field/property paths: `gamemode`, `generatorCond`, `isBroken`, `powerControl`, `panelObj`, `updated`
- Virtual calls: `updUpgrades`, `upd`, `solar`, `sendPower`, `updateAll`, `buttonsVisibility`
- Resolved stack nodes: `2 (updated__DelegateSignature)`
- Literals: (none)
- Expression mix: `EX_InstanceVariable` x13, `EX_Context` x11, `EX_LocalVirtualFunction` x7, `EX_PopExecutionFlow` x4, `EX_PushExecutionFlow` x3, `EX_JumpIfNot` x1, `EX_CallMulticastDelegate` x1, `EX_Return` x1, `EX_Nothing` x1, `EX_EndOfScript` x1

## `update`

- Bytecode size: 196 bytes
- Serialized export: offset 72103, size 506
- Field/property paths: `Temp_object_Variable`, `Temp_object_Variable_1`, `Temp_bool_Variable`, `isBroken`, `turnon`, `K2Node_Select_Default`
- Virtual calls: `Activate`, `upd`
- Resolved stack nodes: `-140 (SetSound)`
- Literals: `False`, `True`
- Expression mix: `EX_LocalVariable` x7, `EX_InstanceVariable` x3, `EX_LetObj` x2, `EX_ObjectConst` x2, `EX_Context` x2, `EX_True` x2, `EX_LetBool` x1, `EX_FinalFunction` x1, `EX_SwitchValue` x1, `EX_False` x1, `EX_VirtualFunction` x1, `EX_LocalVirtualFunction` x1, `EX_Return` x1, `EX_Nothing` x1, `EX_EndOfScript` x1

## `updUpgrades`

- Bytecode size: 1278 bytes
- Serialized export: offset 73402, size 2885
- Field/property paths: `Temp_int_Variable`, `CallFunc_Array_Length_ReturnValue`, `upgradeMeshes`, `CallFunc_Less_IntInt_ReturnValue`, `CallFunc_Array_Get_Item`, `CallFunc_Add_IntInt_ReturnValue`, `Temp_int_Loop_Counter_Variable`, `Temp_int_Array_Index_Variable`, `CallFunc_Array_Length_ReturnValue_1`, `upgradeButtons`, `CallFunc_Less_IntInt_ReturnValue_1`, `CallFunc_Greater_IntInt_ReturnValue`, `upgradeLevel`, `CallFunc_MakeTransform_ReturnValue`, `CallFunc_AddComponent_ReturnValue`, `CallFunc_Array_Add_ReturnValue`, `CallFunc_Array_Get_Item_1`, `CallFunc_K2_AttachToComponent_ReturnValue`, `CallFunc_Conv_IntToFloat_ReturnValue`, `CallFunc_Divide_FloatFloat_ReturnValue`, `CallFunc_Lerp_ReturnValue`, `CallFunc_SelectFloat_ReturnValue_1`, `isBroken`, `Audio`, `CallFunc_SelectFloat_ReturnValue`, `CallFunc_Add_IntInt_ReturnValue_1`
- Virtual calls: (none)
- Resolved stack nodes: `-152 (Array_Length)`, `-165 (Less_IntInt)`, `-151 (Array_Get)`, `-138 (K2_DestroyComponent)`, `-155 (Add_IntInt)`, `-148 (Array_Clear)`, `-163 (Greater_IntInt)`, `-167 (MakeTransform)`, `-135 (AddComponent)`, `-147 (Array_Add)`, `-184 (K2_AttachToComponent)`, `-159 (Conv_IntToFloat)`, `-160 (Divide_FloatFloat)`, `-164 (Lerp)`, `-172 (SelectFloat)`, `-139 (SetPitchMultiplier)`, `-141 (SetVolumeMultiplier)`
- Literals: `EX_IntConst=0`, `EX_IntConst=1`, `EX_NameConst='NODE_AddStaticMeshComponent-0'`, `False`, `EX_NameConst='None'`, `EX_ByteConst=0`, `True`, `EX_FloatConst=6.0`, `EX_FloatConst=0.4000000059604645`, `EX_FloatConst=0.6000000238418579`, `EX_FloatConst=0.20000000298023224`, `EX_FloatConst=1.0`, `EX_FloatConst=3.0`
- Expression mix: `EX_LocalVariable` x49, `EX_Let` x17, `EX_InstanceVariable` x12, `EX_FinalFunction` x11, `EX_CallMath` x11, `EX_Context` x10, `EX_ObjectConst` x6, `EX_FloatConst` x6, `EX_IntConst` x5, `EX_LetBool` x4, `EX_ByteConst` x3, `EX_PushExecutionFlow` x2, `EX_JumpIfNot` x2, `EX_Self` x2, `EX_Jump` x2, `EX_VectorConst` x2, `EX_NameConst` x2, `EX_False` x2, `EX_PopExecutionFlow` x2, `EX_PopExecutionFlowIfNot` x1, `EX_RotationConst` x1, `EX_LetObj` x1, `EX_True` x1, `EX_Return` x1, `EX_Nothing` x1, `EX_EndOfScript` x1

## `getUpgrades`

- Bytecode size: 888 bytes
- Serialized export: offset 76287, size 2027
- Field/property paths: `Temp_int_Variable`, `CallFunc_Array_Length_ReturnValue`, `upgradeButtons`, `CallFunc_Less_IntInt_ReturnValue`, `CallFunc_Array_Get_Item`, `CallFunc_Add_IntInt_ReturnValue`, `Temp_int_Variable_1`, `CallFunc_LessEqual_IntInt_ReturnValue`, `CallFunc_Conv_IntToFloat_ReturnValue`, `CallFunc_MakeVector_ReturnValue`, `CallFunc_Multiply_VectorFloat_ReturnValue`, `CallFunc_MakeTransform_ReturnValue`, `CallFunc_AddComponent_ReturnValue`, `CallFunc_Array_Add_ReturnValue`, `CallFunc_K2_AttachToComponent_ReturnValue`, `upgradeRoot`, `CallFunc_Add_IntInt_ReturnValue_1`
- Virtual calls: `updUpgrades`
- Resolved stack nodes: `-152 (Array_Length)`, `-165 (Less_IntInt)`, `-151 (Array_Get)`, `-138 (K2_DestroyComponent)`, `-155 (Add_IntInt)`, `-148 (Array_Clear)`, `-166 (LessEqual_IntInt)`, `-159 (Conv_IntToFloat)`, `-168 (MakeVector)`, `-169 (Multiply_VectorFloat)`, `-167 (MakeTransform)`, `-135 (AddComponent)`, `-147 (Array_Add)`, `-184 (K2_AttachToComponent)`
- Literals: `EX_IntConst=0`, `EX_IntConst=1`, `EX_IntConst=5`, `EX_FloatConst='+0'`, `EX_FloatConst=14.068180084228516`, `EX_NameConst='NODE_AddBoxComponent-0'`, `False`, `EX_NameConst='None'`, `EX_ByteConst=0`, `True`
- Expression mix: `EX_LocalVariable` x35, `EX_Let` x12, `EX_CallMath` x8, `EX_FinalFunction` x7, `EX_Context` x6, `EX_IntConst` x5, `EX_InstanceVariable` x5, `EX_ObjectConst` x4, `EX_LetBool` x3, `EX_FloatConst` x3, `EX_ByteConst` x3, `EX_PushExecutionFlow` x2, `EX_JumpIfNot` x2, `EX_Self` x2, `EX_Jump` x2, `EX_NameConst` x2, `EX_False` x2, `EX_PopExecutionFlow` x2, `EX_RotationConst` x1, `EX_VectorConst` x1, `EX_LetObj` x1, `EX_True` x1, `EX_LocalVirtualFunction` x1, `EX_Return` x1, `EX_Nothing` x1, `EX_EndOfScript` x1

## `loadData`

- Bytecode size: 834 bytes
- Serialized export: offset 64329, size 1672
- Field/property paths: `data`, `CallFunc_loadData_return`, `bools_12_1EE56AD54E30A7ED1CB618AA037E80B4`, `CallFunc_Array_Get_Item_1`, `bools_2_89CC26B14C79E8F107FE6E9010A5AFC9`, `CallFunc_Array_Get_Item_4`, `isBroken`, `ints_30_E691B1324599D17904E03BB5084DEBA0`, `CallFunc_Array_Get_Item`, `ints_5_89CC26B14C79E8F107FE6E9010A5AFC9`, `CallFunc_Array_Get_Item_6`, `index`, `CallFunc_Array_Get_Item_5`, `cycle`, `CallFunc_Array_Get_Item_2`, `upgradeLevel`, `CallFunc_Array_Get_Item_3`, `cyc`, `return`
- Virtual calls: `updUpgrades`
- Resolved stack nodes: `-130 (loadData)`, `-151 (Array_Get)`
- Literals: `EX_IntConst=0`, `EX_IntConst=1`, `EX_IntConst=2`
- Expression mix: `EX_LocalVariable` x28, `EX_Context` x10, `EX_ObjectConst` x10, `EX_FinalFunction` x10, `EX_StructMemberContext` x10, `EX_IntConst` x10, `EX_InstanceVariable` x5, `EX_LetBool` x3, `EX_Let` x3, `EX_PushExecutionFlow` x2, `EX_LocalFinalFunction` x1, `EX_LocalVirtualFunction` x1, `EX_PopExecutionFlow` x1, `EX_LocalOutVariable` x1, `EX_Return` x1, `EX_Nothing` x1, `EX_EndOfScript` x1

## `getData`

- Bytecode size: 349 bytes
- Serialized export: offset 66001, size 1322
- Field/property paths: `CallFunc_getData_data`, `K2Node_MakeArray_Array`, `index`, `cycle`, `upgradeLevel`, `K2Node_MakeArray_Array_1`, `isBroken`, `panelObj`, `isSineComplete`, `ints_5_89CC26B14C79E8F107FE6E9010A5AFC9`, `K2Node_MakeStruct_struct_mInt`, `bools_2_89CC26B14C79E8F107FE6E9010A5AFC9`, `K2Node_MakeStruct_struct_mBool`, `K2Node_MakeArray_Array_2`, `K2Node_MakeArray_Array_3`, `bools_12_1EE56AD54E30A7ED1CB618AA037E80B4`, `ints_30_E691B1324599D17904E03BB5084DEBA0`, `K2Node_SetFieldsInStruct_StructOut`, `data`
- Virtual calls: (none)
- Resolved stack nodes: `-128 (getData)`
- Literals: (none)
- Expression mix: `EX_LocalVariable` x18, `EX_InstanceVariable` x6, `EX_Let` x6, `EX_SetArray` x4, `EX_StructMemberContext` x4, `EX_LocalFinalFunction` x1, `EX_Context` x1, `EX_LocalOutVariable` x1, `EX_Return` x1, `EX_Nothing` x1, `EX_EndOfScript` x1
