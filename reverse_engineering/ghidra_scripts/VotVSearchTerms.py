# VotVSearchTerms.py
# @category Multivoid
# @menupath Tools.Multivoid.Report VotV Search Terms
#
# Read-only PyGhidra/Ghidra Script Manager report. Run only after the active
# GUI auto-analysis has completed. The script prints matching symbols,
# functions, and already-defined strings; it does not rename, label, or save.

from ghidra.program.util import DefinedDataIterator


TERMS = (
    # Electrical-transformer Blueprint vocabulary. Most of these names live in
    # cooked assets rather than the native EXE, so zero hits are meaningful.
    "transformermgpanel", "generator_c", "prop_transformerupgrade",
    "fullfix", "updupgrades", "getupgrades", "isbroken", "cycle",
    "turnedon", "updated", "switches_states", "switches_target",
    "rotators_states", "clicked_switchers", "clicked_rotataors",
    "moverotator", "moveswitch", "setrotators", "setswitches",
    "setknobs", "randomizetargets", "randomizesines", "solvecolorgrid",
    "attemptignite", "releaseslider", "upg_transofrmer",
    # Native UE4 anchors and reflection surfaces.
    "processevent", "fname", "guobjectarray", "ufunction",
    "blueprintgeneratedclass", "multicastdelegate",
)


def matching(value):
    text = str(value)
    folded = text.lower()
    return text if any(term in folded for term in TERMS) else None


def report_symbols():
    print("\n== matching symbols ==")
    count = 0
    symbols = currentProgram.getSymbolTable().getAllSymbols(True)
    for symbol in symbols:
        hit = matching(symbol.getName(True))
        if hit:
            print("%s  %s  %s" % (symbol.getAddress(), symbol.getSymbolType(), hit))
            count += 1
    print("symbol hits: %d" % count)


def report_functions():
    print("\n== matching functions ==")
    count = 0
    functions = currentProgram.getFunctionManager().getFunctions(True)
    for function in functions:
        hit = matching(function.getName(True))
        if hit:
            print("%s  %s" % (function.getEntryPoint(), hit))
            count += 1
    print("function hits: %d" % count)


def report_defined_strings():
    print("\n== matching defined strings ==")
    count = 0
    for data in DefinedDataIterator.definedStrings(currentProgram):
        value = data.getDefaultValueRepresentation()
        hit = matching(value)
        if hit:
            print("%s  %s" % (data.getAddress(), hit))
            count += 1
    print("defined-string hits: %d" % count)


print("== VotV read-only search report ==")
print("program: %s" % currentProgram.getName())
print("terms: %d" % len(TERMS))
report_symbols()
report_functions()
report_defined_strings()
print("\nNOTE: transformerMGPanel_C and generator_C are Blueprint assets. Their")
print("logic is in cooked .uasset/.uexp bytecode, not necessarily in this EXE.")
