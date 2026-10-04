# Blueprint bytecode symbol summary

Source: `research/bp_reflection/transformerMGPanel.json`

This is a structural inventory of UAssetAPI's decoded Kismet bytecode; it is not native Ghidra output and does not claim source-level control flow.

## `clicked_switchers`

- Bytecode size: 54 bytes
- Serialized export: offset 111236, size 228
- Field/property paths: `K2Node_CustomEvent_TouchedComponent`, `TouchedComponent`, `K2Node_CustomEvent_ButtonPressed`, `ButtonPressed`
- Virtual calls: (none)
- Resolved stack nodes: `1 (ExecuteUbergraph_transformerMGPanel)`
- Literals: `EX_IntConst=4090`
- Expression mix: `EX_LetValueOnPersistentFrame` x2, `EX_LocalVariable` x2, `EX_LocalFinalFunction` x1, `EX_IntConst` x1, `EX_Return` x1, `EX_Nothing` x1, `EX_EndOfScript` x1

## `clicked_rotataors`

- Bytecode size: 54 bytes
- Serialized export: offset 111603, size 228
- Field/property paths: `K2Node_CustomEvent_TouchedComponent_1`, `TouchedComponent`, `K2Node_CustomEvent_ButtonPressed_1`, `ButtonPressed`
- Virtual calls: (none)
- Resolved stack nodes: `1 (ExecuteUbergraph_transformerMGPanel)`
- Literals: `EX_IntConst=3757`
- Expression mix: `EX_LetValueOnPersistentFrame` x2, `EX_LocalVariable` x2, `EX_LocalFinalFunction` x1, `EX_IntConst` x1, `EX_Return` x1, `EX_Nothing` x1, `EX_EndOfScript` x1

## `moveRotator`

- Bytecode size: 36 bytes
- Serialized export: offset 110958, size 139
- Field/property paths: `K2Node_CustomEvent_index`, `index`
- Virtual calls: (none)
- Resolved stack nodes: `1 (ExecuteUbergraph_transformerMGPanel)`
- Literals: `EX_IntConst=4677`
- Expression mix: `EX_LetValueOnPersistentFrame` x1, `EX_LocalVariable` x1, `EX_LocalFinalFunction` x1, `EX_IntConst` x1, `EX_Return` x1, `EX_Nothing` x1, `EX_EndOfScript` x1

## `moveSwitch`

- Bytecode size: 36 bytes
- Serialized export: offset 111097, size 139
- Field/property paths: `K2Node_CustomEvent_index_1`, `index`
- Virtual calls: (none)
- Resolved stack nodes: `1 (ExecuteUbergraph_transformerMGPanel)`
- Literals: `EX_IntConst=4365`
- Expression mix: `EX_LetValueOnPersistentFrame` x1, `EX_LocalVariable` x1, `EX_LocalFinalFunction` x1, `EX_IntConst` x1, `EX_Return` x1, `EX_Nothing` x1, `EX_EndOfScript` x1

## `setRotators`

- Bytecode size: 758 bytes
- Serialized export: offset 129582, size 1572
- Field/property paths: `CallFunc_checkColors_solved`, `isRotatorsComplete`, `CallFunc_SelectColor_ReturnValue`, `Temp_int_Loop_Counter_Variable`, `Temp_int_Array_Index_Variable`, `CallFunc_Array_Length_ReturnValue`, `buttons_rotators`, `CallFunc_Less_IntInt_ReturnValue`, `CallFunc_Array_Get_Item`, `CallFunc_getKnobRot_ReturnValue`, `rotators_states`, `CallFunc_K2_SetRelativeRotation_SweepHitResult`, `widgetInst`, `CallFunc_Add_IntInt_ReturnValue`
- Virtual calls: `checkColors`, `getKnobRot`, `updateCollision`, `printStatus`
- Resolved stack nodes: `-254 (SelectColor)`, `-265 (PrintString)`, `-203 (Array_Length)`, `-234 (Less_IntInt)`, `-201 (Array_Get)`, `-275 (K2_SetRelativeRotation)`, `-211 (Add_IntInt)`
- Literals: `EX_FloatConst='+0'`, `EX_FloatConst=1.0`, `EX_StringConst='Hello'`, `True`, `EX_FloatConst=2.0`, `EX_IntConst=0`, `False`, `EX_IntConst=1`
- Expression mix: `EX_LocalVariable` x27, `EX_FloatConst` x9, `EX_Let` x8, `EX_InstanceVariable` x7, `EX_Context` x6, `EX_LocalVirtualFunction` x4, `EX_CallMath` x4, `EX_ObjectConst` x4, `EX_FinalFunction` x4, `EX_IntConst` x3, `EX_False` x3, `EX_PushExecutionFlow` x2, `EX_LetBool` x2, `EX_StructConst` x2, `EX_Self` x2, `EX_True` x2, `EX_PopExecutionFlow` x2, `EX_StringConst` x1, `EX_JumpIfNot` x1, `EX_ArrayGetByRef` x1, `EX_Jump` x1, `EX_Return` x1, `EX_Nothing` x1, `EX_EndOfScript` x1

## `setSwitches`

- Bytecode size: 2660 bytes
- Serialized export: offset 131154, size 6345
- Field/property paths: `swMatInd`, `switches_target`, `CallFunc_ByteToBits_Bit1`, `CallFunc_ByteToBits_Bit2`, `CallFunc_ByteToBits_Bit3`, `CallFunc_ByteToBits_Bit4`, `CallFunc_ByteToBits_Bit5`, `CallFunc_ByteToBits_Bit6`, `CallFunc_ByteToBits_Bit7`, `CallFunc_ByteToBits_Bit8`, `CallFunc_EqualEqual_BoolBool_ReturnValue`, `switches_states`, `CallFunc_EqualEqual_BoolBool_ReturnValue_1`, `CallFunc_EqualEqual_BoolBool_ReturnValue_2`, `CallFunc_EqualEqual_BoolBool_ReturnValue_3`, `CallFunc_EqualEqual_BoolBool_ReturnValue_4`, `CallFunc_EqualEqual_BoolBool_ReturnValue_5`, `CallFunc_EqualEqual_BoolBool_ReturnValue_6`, `CallFunc_EqualEqual_BoolBool_ReturnValue_7`, `K2Node_MakeArray_Array`, `compareArray`, `CallFunc_Array_Contains_ReturnValue`, `Temp_bool_Variable`, `CallFunc_Not_PreBool_ReturnValue`, `isSwitchesComplete`, `CallFunc_BitsToByte_Byte`, `switchesBitsToBytes`, `Temp_int_Loop_Counter_Variable`, `Temp_int_Array_Index_Variable`, `CallFunc_Array_Length_ReturnValue`, `CallFunc_Less_IntInt_ReturnValue`, `widgetInst`, `textsBits`, `CallFunc_Array_Get_Item_3`, `CallFunc_IsValid_ReturnValue`, `CallFunc_Array_Get_Item_1`, `CallFunc_Conv_BoolToInt_ReturnValue`, `CallFunc_Conv_IntToText_ReturnValue`, `CallFunc_SelectColor_ReturnValue`, `SpecifiedColor`, `K2Node_MakeStruct_SlateColor`, `ColorUseRule`, `CallFunc_getSwitchRot_ReturnValue`, `buttons_switches`, `CallFunc_Array_Get_Item_2`, `CallFunc_K2_SetRelativeRotation_SweepHitResult`, `swState`, `Temp_object_Variable`, `CallFunc_Array_Get_Item`, `Temp_object_Variable_1`, `Temp_bool_Variable_1`, `K2Node_Select_Default`, `CallFunc_Conv_ByteToFloat_ReturnValue`, `CallFunc_Divide_FloatFloat_ReturnValue`, `switchesMatch`, `dynmat_barSwitches`, `CallFunc_Add_IntInt_ReturnValue`, `CallFunc_Not_PreBool_ReturnValue_1`
- Virtual calls: `SetText`, `getSwitchRot`, `SetMaterial`, `printStatus`
- Resolved stack nodes: `-279 (ByteToBits)`, `-225 (EqualEqual_BoolBool)`, `-199 (Array_Contains)`, `-246 (Not_PreBool)`, `-278 (BitsToByte)`, `-203 (Array_Length)`, `-234 (Less_IntInt)`, `-201 (Array_Get)`, `-263 (IsValid)`, `-218 (Conv_BoolToInt)`, `-266 (Conv_IntToText)`, `-254 (SelectColor)`, `-282 (SetColorAndOpacity)`, `-275 (K2_SetRelativeRotation)`, `-219 (Conv_ByteToFloat)`, `-223 (Divide_FloatFloat)`, `-268 (SetScalarParameterValue)`, `-211 (Add_IntInt)`
- Literals: `EX_IntConst=1`, `EX_IntConst=7`, `EX_IntConst=6`, `EX_IntConst=5`, `EX_IntConst=4`, `EX_IntConst=3`, `EX_IntConst=2`, `EX_IntConst=0`, `False`, `True`, `EX_IntConst=324`, `EX_FloatConst='+0'`, `EX_FloatConst=1.0`, `EX_ByteConst=0`, `EX_FloatConst=255.0`, `EX_NameConst='alpha'`
- Expression mix: `EX_LocalVariable` x115, `EX_InstanceVariable` x39, `EX_IntConst` x24, `EX_Let` x20, `EX_CallMath` x20, `EX_Context` x20, `EX_LetBool` x17, `EX_ArrayGetByRef` x16, `EX_FinalFunction` x14, `EX_ObjectConst` x13, `EX_FloatConst` x9, `EX_False` x4, `EX_PushExecutionFlow` x3, `EX_True` x3, `EX_VirtualFunction` x2, `EX_StructConst` x2, `EX_StructMemberContext` x2, `EX_LocalVirtualFunction` x2, `EX_LetObj` x2, `EX_PopExecutionFlow` x2, `EX_Jump` x2, `EX_SetArray` x1, `EX_JumpIfNot` x1, `EX_PopExecutionFlowIfNot` x1, `EX_ByteConst` x1, `EX_SwitchValue` x1, `EX_NameConst` x1, `EX_Return` x1, `EX_Nothing` x1, `EX_EndOfScript` x1

## `setKnobs`

- Bytecode size: 2811 bytes
- Serialized export: offset 120620, size 6677
- Field/property paths: `CallFunc_Clamp_ReturnValue_1`, `sine_frequency`, `CallFunc_Clamp_ReturnValue_2`, `sine_offset`, `CallFunc_Clamp_ReturnValue`, `sine_amplitude`, `CallFunc_Conv_IntToFloat_ReturnValue_11`, `CallFunc_Divide_FloatFloat_ReturnValue_11`, `CallFunc_Multiply_FloatFloat_ReturnValue_3`, `CallFunc_MakeRotator_ReturnValue_1`, `knobTop_offset`, `CallFunc_K2_SetRelativeRotation_SweepHitResult_1`, `CallFunc_Conv_IntToFloat_ReturnValue_10`, `CallFunc_Divide_FloatFloat_ReturnValue_10`, `CallFunc_Multiply_FloatFloat_ReturnValue_2`, `CallFunc_MakeRotator_ReturnValue`, `knobBottom_freq`, `CallFunc_K2_SetRelativeRotation_SweepHitResult`, `CallFunc_Conv_IntToFloat_ReturnValue_9`, `CallFunc_Divide_FloatFloat_ReturnValue_9`, `CallFunc_Lerp_ReturnValue`, `CallFunc_MakeVector_ReturnValue`, `slider_amp`, `CallFunc_K2_SetRelativeLocation_SweepHitResult`, `CallFunc_Subtract_IntInt_ReturnValue_2`, `targetSine_offset`, `CallFunc_Abs_Int_ReturnValue_2`, `CallFunc_Conv_IntToFloat_ReturnValue_8`, `CallFunc_Divide_FloatFloat_ReturnValue_8`, `CallFunc_Subtract_FloatFloat_ReturnValue_2`, `sineOffsetDiff`, `CallFunc_Subtract_IntInt_ReturnValue_1`, `targetSine_frequency`, `CallFunc_Abs_Int_ReturnValue_1`, `CallFunc_Conv_IntToFloat_ReturnValue_7`, `CallFunc_Divide_FloatFloat_ReturnValue_7`, `CallFunc_Subtract_FloatFloat_ReturnValue_1`, `sineFrequencyDIff`, `CallFunc_Subtract_IntInt_ReturnValue`, `targetSine_amplitude`, `CallFunc_Abs_Int_ReturnValue`, `CallFunc_Conv_IntToFloat_ReturnValue`, `CallFunc_Divide_FloatFloat_ReturnValue`, `CallFunc_Subtract_FloatFloat_ReturnValue`, `sineAmplitudeDIff`, `CallFunc_Conv_IntToFloat_ReturnValue_3`, `CallFunc_Divide_FloatFloat_ReturnValue_3`, `dynmat_sine`, `CallFunc_Conv_IntToFloat_ReturnValue_2`, `CallFunc_Divide_FloatFloat_ReturnValue_2`, `CallFunc_Conv_IntToFloat_ReturnValue_1`, `CallFunc_Divide_FloatFloat_ReturnValue_1`, `CallFunc_Conv_IntToFloat_ReturnValue_6`, `CallFunc_Divide_FloatFloat_ReturnValue_6`, `CallFunc_Conv_IntToFloat_ReturnValue_5`, `CallFunc_Divide_FloatFloat_ReturnValue_5`, `CallFunc_Conv_IntToFloat_ReturnValue_4`, `CallFunc_Divide_FloatFloat_ReturnValue_4`, `CallFunc_Multiply_FloatFloat_ReturnValue`, `CallFunc_Multiply_FloatFloat_ReturnValue_1`, `sineResult`, `dynmat_bar_sine`, `CallFunc_EqualEqual_IntInt_ReturnValue`, `CallFunc_EqualEqual_IntInt_ReturnValue_1`, `CallFunc_EqualEqual_IntInt_ReturnValue_2`, `CallFunc_BooleanAND_ReturnValue`, `CallFunc_BooleanAND_ReturnValue_1`, `isSineComplete`, `widgetInst`
- Virtual calls: `printStatus`
- Resolved stack nodes: `-217 (Clamp)`, `-222 (Conv_IntToFloat)`, `-223 (Divide_FloatFloat)`, `-242 (Multiply_FloatFloat)`, `-238 (MakeRotator)`, `-275 (K2_SetRelativeRotation)`, `-233 (Lerp)`, `-240 (MakeVector)`, `-274 (K2_SetRelativeLocation)`, `-257 (Subtract_IntInt)`, `-209 (Abs_Int)`, `-256 (Subtract_FloatFloat)`, `-268 (SetScalarParameterValue)`, `-227 (EqualEqual_IntInt)`, `-214 (BooleanAND)`
- Literals: `EX_IntConst=0`, `EX_IntConst=15`, `EX_FloatConst=16.0`, `EX_FloatConst=-360.0`, `EX_FloatConst='+0'`, `False`, `EX_FloatConst=15.0`, `EX_FloatConst=9.0`, `EX_FloatConst=1.0`, `EX_NameConst='amplitude_A'`, `EX_NameConst='offset_A'`, `EX_NameConst='frequency_A'`, `EX_NameConst='amplitude_B'`, `EX_NameConst='offset_B'`, `EX_NameConst='frequency_B'`, `EX_NameConst='alpha'`
- Expression mix: `EX_LocalVariable` x101, `EX_Let` x51, `EX_CallMath` x49, `EX_InstanceVariable` x47, `EX_FloatConst` x25, `EX_Context` x11, `EX_FinalFunction` x10, `EX_NameConst` x7, `EX_IntConst` x6, `EX_False` x6, `EX_LetBool` x6, `EX_PushExecutionFlow` x2, `EX_PopExecutionFlow` x2, `EX_LocalVirtualFunction` x1, `EX_Return` x1, `EX_Nothing` x1, `EX_EndOfScript` x1

## `randomizeTargets`

- Bytecode size: 532 bytes
- Serialized export: offset 127297, size 1408
- Field/property paths: `CallFunc_RandomIntegerInRange_ReturnValue_2`, `targetSine_offset`, `CallFunc_RandomIntegerInRange_ReturnValue_1`, `targetSine_frequency`, `CallFunc_RandomIntegerInRange_ReturnValue`, `targetSine_amplitude`, `CallFunc_RandomBool_ReturnValue`, `CallFunc_RandomBool_ReturnValue_1`, `CallFunc_RandomBool_ReturnValue_2`, `CallFunc_RandomBool_ReturnValue_3`, `CallFunc_RandomBool_ReturnValue_4`, `CallFunc_RandomBool_ReturnValue_5`, `CallFunc_RandomBool_ReturnValue_6`, `CallFunc_RandomBool_ReturnValue_7`, `CallFunc_BitsToByte_Byte`, `switches_target`
- Virtual calls: `solveColorGrid`, `setRotators`, `setKnobs`, `setSwitches`
- Resolved stack nodes: `-252 (RandomIntegerInRange)`, `-251 (RandomBool)`, `-278 (BitsToByte)`
- Literals: `EX_IntConst=0`, `EX_IntConst=15`, `EX_IntConst=1`
- Expression mix: `EX_LocalVariable` x24, `EX_CallMath` x12, `EX_LetBool` x8, `EX_Let` x7, `EX_IntConst` x6, `EX_InstanceVariable` x4, `EX_LocalVirtualFunction` x4, `EX_Return` x1, `EX_Nothing` x1, `EX_EndOfScript` x1

## `randomizeValues`

- Bytecode size: 989 bytes
- Serialized export: offset 163408, size 2275
- Field/property paths: `CallFunc_RandomIntegerInRange_ReturnValue_3`, `sine_offset`, `CallFunc_RandomIntegerInRange_ReturnValue_2`, `sine_frequency`, `CallFunc_RandomIntegerInRange_ReturnValue_1`, `sine_amplitude`, `Temp_int_Loop_Counter_Variable_1`, `Temp_int_Array_Index_Variable_1`, `CallFunc_Array_Length_ReturnValue`, `rotators_states`, `CallFunc_Less_IntInt_ReturnValue`, `CallFunc_RandomIntegerInRange_ReturnValue`, `CallFunc_Conv_IntToByte_ReturnValue`, `Temp_int_Loop_Counter_Variable`, `Temp_int_Array_Index_Variable`, `CallFunc_Array_Length_ReturnValue_1`, `switches_states`, `CallFunc_Less_IntInt_ReturnValue_1`, `CallFunc_RandomBool_ReturnValue`, `CallFunc_Add_IntInt_ReturnValue_1`, `CallFunc_Add_IntInt_ReturnValue`
- Virtual calls: `setRotators`, `setKnobs`, `setSwitches`
- Resolved stack nodes: `-252 (RandomIntegerInRange)`, `-203 (Array_Length)`, `-234 (Less_IntInt)`, `-221 (Conv_IntToByte)`, `-207 (Array_Set)`, `-251 (RandomBool)`, `-211 (Add_IntInt)`
- Literals: `EX_IntConst=0`, `EX_IntConst=15`, `EX_IntConst=1`, `EX_IntConst=3`, `False`
- Expression mix: `EX_LocalVariable` x40, `EX_Let` x20, `EX_IntConst` x14, `EX_CallMath` x10, `EX_InstanceVariable` x7, `EX_Context` x4, `EX_ObjectConst` x4, `EX_FinalFunction` x4, `EX_PushExecutionFlow` x3, `EX_LetBool` x3, `EX_PopExecutionFlow` x3, `EX_LocalVirtualFunction` x3, `EX_JumpIfNot` x2, `EX_False` x2, `EX_Jump` x2, `EX_Return` x1, `EX_Nothing` x1, `EX_EndOfScript` x1

## `randomizeSines`

- Bytecode size: 407 bytes
- Serialized export: offset 165683, size 945
- Field/property paths: `CallFunc_RandomIntegerInRange_ReturnValue_5`, `targetSine_offset`, `CallFunc_RandomIntegerInRange_ReturnValue_4`, `targetSine_frequency`, `CallFunc_RandomIntegerInRange_ReturnValue_3`, `targetSine_amplitude`, `CallFunc_RandomIntegerInRange_ReturnValue_1`, `sine_offset`, `CallFunc_RandomIntegerInRange_ReturnValue`, `sine_frequency`, `CallFunc_RandomIntegerInRange_ReturnValue_2`, `sine_amplitude`
- Virtual calls: `setKnobs`
- Resolved stack nodes: `-252 (RandomIntegerInRange)`
- Literals: `EX_IntConst=0`, `EX_IntConst=15`, `EX_IntConst=1`
- Expression mix: `EX_Let` x12, `EX_LocalVariable` x12, `EX_IntConst` x12, `EX_CallMath` x6, `EX_InstanceVariable` x6, `EX_LocalVirtualFunction` x1, `EX_Return` x1, `EX_Nothing` x1, `EX_EndOfScript` x1

## `solveColorGrid`

- Bytecode size: 3364 bytes
- Serialized export: offset 139085, size 7881
- Field/property paths: `coltemo`, `defaultColors`, `top_5_949BEC1F42A60C862E0138A8F3B50EA8`, `K2Node_MakeStruct_struct_generatorRotator`, `right_6_A1B76A824B597885F4388F8AD5974994`, `bottom_8_D4BF7E4642599DFD9452518EFF271B63`, `left_10_39AA94C0481FF6F715446B910D4435D7`, `defaultCol`, `rotators_colorGrid`, `CallFunc_RandomIntegerInRange_ReturnValue`, `CallFunc_Conv_IntToByte_ReturnValue`, `CallFunc_rotateColor_OutputPin`, `K2Node_MakeArray_Array_7`, `CallFunc_removeColors_colorsOut_7`, `CallFunc_assignRandomColors_out_7`, `K2Node_MakeArray_Array_3`, `CallFunc_removeColors_colorsOut_3`, `CallFunc_assignRandomColors_out_3`, `K2Node_MakeArray_Array_1`, `CallFunc_removeColors_colorsOut_1`, `CallFunc_assignRandomColors_out_1`, `Temp_int_Loop_Counter_Variable`, `Temp_int_Array_Index_Variable`, `CallFunc_Array_Length_ReturnValue`, `buttons_rotators`, `CallFunc_Less_IntInt_ReturnValue`, `CallFunc_Array_Get_Item`, `CallFunc_Array_Get_Item_1`, `CallFunc_byteToColor_mat_3`, `CallFunc_byteToColor_mat_2`, `CallFunc_byteToColor_mat_1`, `CallFunc_byteToColor_mat`, `CallFunc_Add_IntInt_ReturnValue`, `K2Node_MakeArray_Array_6`, `CallFunc_removeColors_colorsOut_6`, `CallFunc_assignRandomColors_out_6`, `K2Node_MakeArray_Array_5`, `CallFunc_removeColors_colorsOut_5`, `CallFunc_assignRandomColors_out_5`, `K2Node_MakeArray_Array_4`, `CallFunc_removeColors_colorsOut_4`, `CallFunc_assignRandomColors_out_4`, `K2Node_MakeArray_Array_2`, `CallFunc_removeColors_colorsOut_2`, `CallFunc_assignRandomColors_out_2`, `K2Node_MakeArray_Array`, `CallFunc_removeColors_colorsOut`, `CallFunc_assignRandomColors_out`
- Virtual calls: `rotateColor`, `removeColors`, `assignRandomColors`, `byteToColor`, `SetMaterial`
- Resolved stack nodes: `-208 (Array_Shuffle)`, `-206 (Array_Resize)`, `-207 (Array_Set)`, `-252 (RandomIntegerInRange)`, `-221 (Conv_IntToByte)`, `-203 (Array_Length)`, `-234 (Less_IntInt)`, `-201 (Array_Get)`, `-211 (Add_IntInt)`
- Literals: `EX_IntConst=4`, `EX_IntConst=0`, `EX_IntConst=1`, `EX_IntConst=2`, `EX_IntConst=3`, `False`, `EX_IntConst=5`, `EX_IntConst=7`, `EX_IntConst=6`, `EX_IntConst=8`
- Expression mix: `EX_LocalVariable` x103, `EX_IntConst` x61, `EX_InstanceVariable` x59, `EX_ArrayGetByRef` x50, `EX_StructMemberContext` x44, `EX_Let` x27, `EX_LocalVirtualFunction` x21, `EX_Context` x16, `EX_ObjectConst` x12, `EX_FinalFunction` x12, `EX_PushExecutionFlow` x10, `EX_PopExecutionFlow` x9, `EX_SetArray` x8, `EX_CallMath` x4, `EX_VirtualFunction` x4, `EX_False` x1, `EX_LetBool` x1, `EX_PopExecutionFlowIfNot` x1, `EX_Jump` x1, `EX_Return` x1, `EX_Nothing` x1, `EX_EndOfScript` x1

## `checkColors`

- Bytecode size: 2923 bytes
- Serialized export: offset 155968, size 7363
- Field/property paths: `isSolved`, `corr`, `bulb`, `Temp_int_Loop_Counter_Variable`, `Temp_int_Array_Index_Variable`, `CallFunc_Array_Length_ReturnValue_1`, `rotators_colorGrid`, `CallFunc_Less_IntInt_ReturnValue_3`, `ind`, `CallFunc_Divide_IntInt_ReturnValue`, `CallFunc_Percent_IntInt_ReturnValue`, `X`, `K2Node_MakeStruct_IntPoint_4`, `Y`, `indexToGrid`, `CallFunc_Array_Get_Item_3`, `rotators_states`, `CallFunc_rotateColor_OutputPin_1`, `col`, `K2Node_MakeStruct_IntPoint_3`, `offset`, `dir`, `CallFunc_Add_IntPointIntPoint_ReturnValue`, `offsetIndex`, `CallFunc_Less_IntInt_ReturnValue_1`, `CallFunc_Greater_IntInt_ReturnValue`, `CallFunc_Less_IntInt_ReturnValue_2`, `CallFunc_Greater_IntInt_ReturnValue_1`, `CallFunc_BooleanOR_ReturnValue`, `CallFunc_BooleanOR_ReturnValue_1`, `CallFunc_BooleanOR_ReturnValue_2`, `Temp_int_Variable`, `CallFunc_Array_Length_ReturnValue`, `buttons_rotators`, `CallFunc_Less_IntInt_ReturnValue`, `CallFunc_Array_Get_Item`, `CallFunc_Add_IntInt_ReturnValue`, `solved`, `CallFunc_Add_IntInt_ReturnValue_1`, `K2Node_MakeStruct_IntPoint_2`, `K2Node_MakeStruct_IntPoint_1`, `K2Node_MakeStruct_IntPoint`, `Temp_object_Variable`, `Temp_object_Variable_1`, `CallFunc_Array_Get_Item_1`, `Temp_bool_Variable`, `K2Node_Select_Default`, `CallFunc_Multiply_IntInt_ReturnValue`, `CallFunc_Add_IntInt_ReturnValue_2`, `CallFunc_Array_Get_Item_2`, `CallFunc_rotateColor_OutputPin`, `K2Node_SwitchInteger_CmpSuccess`, `CallFunc_EqualEqual_ByteByte_ReturnValue_2`, `left_10_39AA94C0481FF6F715446B910D4435D7`, `right_6_A1B76A824B597885F4388F8AD5974994`, `CallFunc_EqualEqual_ByteByte_ReturnValue_3`, `bottom_8_D4BF7E4642599DFD9452518EFF271B63`, `top_5_949BEC1F42A60C862E0138A8F3B50EA8`, `CallFunc_EqualEqual_ByteByte_ReturnValue`, `CallFunc_EqualEqual_ByteByte_ReturnValue_1`, `CallFunc_BooleanAND_ReturnValue_1`, `CallFunc_BooleanAND_ReturnValue`
- Virtual calls: `rotateColor`, `SetMaterial`
- Resolved stack nodes: `-203 (Array_Length)`, `-234 (Less_IntInt)`, `-224 (Divide_IntInt)`, `-250 (Percent_IntInt)`, `-201 (Array_Get)`, `-212 (Add_IntPointIntPoint)`, `-230 (Greater_IntInt)`, `-215 (BooleanOR)`, `-211 (Add_IntInt)`, `-243 (Multiply_IntInt)`, `-248 (NotEqual_IntInt)`, `-226 (EqualEqual_ByteByte)`, `-214 (BooleanAND)`
- Literals: `True`, `False`, `EX_IntConst=0`, `EX_IntConst=3`, `EX_IntConst=-1`, `EX_IntConst=2`, `EX_IntConst=5`, `EX_IntConst=1`
- Expression mix: `EX_LocalVariable` x157, `EX_Let` x37, `EX_LetBool` x31, `EX_IntConst` x30, `EX_CallMath` x26, `EX_StructMemberContext` x24, `EX_ObjectConst` x9, `EX_PushExecutionFlow` x8, `EX_Context` x8, `EX_InstanceVariable` x8, `EX_PopExecutionFlow` x8, `EX_FinalFunction` x6, `EX_JumpIfNot` x6, `EX_Jump` x6, `EX_True` x3, `EX_False` x3, `EX_LocalVirtualFunction` x2, `EX_ArrayGetByRef` x2, `EX_PopExecutionFlowIfNot` x2, `EX_VirtualFunction` x2, `EX_LetObj` x2, `EX_LocalOutVariable` x1, `EX_SwitchValue` x1, `EX_Return` x1, `EX_Nothing` x1, `EX_EndOfScript` x1

## `updateAll`

- Bytecode size: 45 bytes
- Serialized export: offset 166868, size 77
- Field/property paths: (none)
- Virtual calls: `setKnobs`, `setRotators`, `setSwitches`
- Resolved stack nodes: (none)
- Literals: (none)
- Expression mix: `EX_LocalVirtualFunction` x3, `EX_Return` x1, `EX_Nothing` x1, `EX_EndOfScript` x1

## `initiate`

- Bytecode size: 1168 bytes
- Serialized export: offset 166945, size 2330
- Field/property paths: `initiated`, `CallFunc_GetMaterialInstance_ReturnValue`, `Widget`, `StaticMesh`, `CallFunc_GetMaterialInstance_ReturnValue_1`, `CallFunc_GetWidget_ReturnValue`, `K2Node_DynamicCast_AsUiwindow_Transformer_Screens`, `K2Node_DynamicCast_bSuccess`, `widgetInst`, `transformer`, `CallFunc_GetDynamicMaterial_ReturnValue_2`, `image_sine`, `dynmat_sine`, `CallFunc_GetDynamicMaterial_ReturnValue_1`, `image_bar_sine`, `dynmat_bar_sine`, `CallFunc_GetDynamicMaterial_ReturnValue`, `image_bar_switches`, `dynmat_barSwitches`, `difficulty`, `difficulty_2_63EA144B4BB6BCD72D6A29A0A3A8FDEA`, `gamemode`, `gameInstance`, `gameRules`, `K2Node_MakeArray_Array`, `defaultColors`, `K2Node_MakeArray_Array_1`, `knobBottom_freq`, `knobTop_offset`, `slider_amp`, `boxSlider`, `buttonsHoverOver`, `buttons_rotators`, `buttons_switches`
- Virtual calls: `SetMaterial`, `GetWidget`, `printStatus`, `init`, `initValues`, `addHint`
- Resolved stack nodes: `-283 (GetMaterialInstance)`, `-281 (GetDynamicMaterial)`, `-197 (Array_Append)`
- Literals: `EX_IntConst=1`, `True`, `EX_ByteConst=1`, `EX_ByteConst=2`, `EX_ByteConst=3`, `EX_ByteConst=4`, `EX_StringConst='Error initializing transformer panel'`, `EX_StringConst='3731CE9B4A024F08475662B2A6C62392'`, `EX_StringConst=''`, `False`
- Expression mix: `EX_InstanceVariable` x35, `EX_LocalVariable` x21, `EX_Context` x19, `EX_LetObj` x11, `EX_ByteConst` x9, `EX_FinalFunction` x7, `EX_PopExecutionFlow` x5, `EX_Let` x5, `EX_LocalVirtualFunction` x4, `EX_PushExecutionFlow` x3, `EX_VirtualFunction` x3, `EX_ObjectConst` x3, `EX_StringConst` x3, `EX_JumpIfNot` x2, `EX_IntConst` x2, `EX_Self` x2, `EX_SetArray` x2, `EX_LetBool` x1, `EX_True` x1, `EX_DynamicCast` x1, `EX_PrimitiveCast` x1, `EX_StructMemberContext` x1, `EX_TextConst` x1, `EX_False` x1, `EX_Return` x1, `EX_Nothing` x1, `EX_EndOfScript` x1

## `initValues`

- Bytecode size: 45 bytes
- Serialized export: offset 163331, size 77
- Field/property paths: (none)
- Virtual calls: `setRotators`, `setKnobs`, `setSwitches`
- Resolved stack nodes: (none)
- Literals: (none)
- Expression mix: `EX_LocalVirtualFunction` x3, `EX_Return` x1, `EX_Nothing` x1, `EX_EndOfScript` x1

## `releaseSlider`

- Bytecode size: 18 bytes
- Serialized export: offset 104008, size 58
- Field/property paths: (none)
- Virtual calls: (none)
- Resolved stack nodes: `1 (ExecuteUbergraph_transformerMGPanel)`
- Literals: `EX_IntConst=5186`
- Expression mix: `EX_LocalFinalFunction` x1, `EX_IntConst` x1, `EX_Return` x1, `EX_Nothing` x1, `EX_EndOfScript` x1
