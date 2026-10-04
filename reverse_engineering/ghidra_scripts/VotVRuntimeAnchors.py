# VotVRuntimeAnchors.py
# @category Multivoid
# @menupath Tools.Multivoid.Report Runtime Anchors
#
# Read-only report that maps RVAs observed in multivoid.log into the current
# Ghidra program. Run only after GUI auto-analysis completes.


ANCHORS = (
    ("FName::ToString", 0x127D870),
    ("ProcessEvent", 0x1465930),
    ("FD3D11Viewport::PresentChecked", 0x16F4BA0),
    ("FD3D11Viewport::Resize", 0x1703750),
    ("FD3D12Viewport::PresentInternal", 0x177E0E0),
    ("FD3D12Viewport::ResizeInternal", 0x177E8B0),
    ("UGameplayStatics::OpenLevel", 0x2B530B0),
    ("GUObjectArray", 0x4D8F910),
)


def containing_function(address):
    function = currentProgram.getFunctionManager().getFunctionContaining(address)
    return function.getName(True) if function else "<no analyzed function>"


image_base = currentProgram.getImageBase()
print("== VotV runtime anchors (read-only) ==")
print("program: %s" % currentProgram.getName())
print("image base: %s" % image_base)
for name, rva in ANCHORS:
    address = image_base.add(rva)
    block = currentProgram.getMemory().getBlock(address)
    block_name = block.getName() if block else "<unmapped>"
    print("%-38s RVA 0x%08X  %s  %-10s  %s" % (
        name, rva, address, block_name, containing_function(address)))

print("\nThese addresses came from the 2026-09-23 b65004 menu run.")
print("Verify the executable SHA-256 before treating them as stable.")
