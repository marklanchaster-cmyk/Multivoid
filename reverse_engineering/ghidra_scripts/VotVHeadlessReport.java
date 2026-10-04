// VotVHeadlessReport.java
// @category Multivoid
// @description Read-only transformer/power discovery, xrefs, and anchor decompilation.

import java.io.BufferedWriter;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.StringDataInstance;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.util.DefinedDataIterator;

public class VotVHeadlessReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "ad478218ec5513cc4c1682937db3214cbf2694d1dcd0583eb423447c049dd3ae";

    private static final String[] TERMS = {
        "transformerMGPanel", "transformer", "generator_C", "generator",
        "powerControl", "breaker", "prop_transformerUpgrade", "upg_transofrmer",
        "fullFix", "isBroken", "cycle", "turnedOn", "updated",
        "switches_states", "switches_target", "rotators_states",
        "clicked_switchers", "clicked_rotataors", "moveRotator", "moveSwitch",
        "setRotators", "setSwitches", "setKnobs", "randomizeTargets",
        "randomizeSines", "solveColorGrid", "checkColors", "attemptIgnite",
        "powerChanged", "press_coord", "press_downl", "press_play",
        "press_calc", "press_light", "ProcessEvent", "GUObjectArray",
        "FName::ToString", "UFunction", "BlueprintGeneratedClass"
    };

    private static final Map<String, Long> ANCHORS = new LinkedHashMap<>();
    static {
        ANCHORS.put("FName_ToString", 0x127d870L);
        ANCHORS.put("ProcessEvent", 0x1465930L);
        ANCHORS.put("D3D11_PresentChecked", 0x16f4ba0L);
        ANCHORS.put("D3D11_Resize", 0x1703750L);
        ANCHORS.put("D3D12_PresentInternal", 0x177e0e0L);
        ANCHORS.put("D3D12_ResizeInternal", 0x177e8b0L);
        ANCHORS.put("UGameplayStatics_OpenLevel", 0x2b530b0L);
        ANCHORS.put("GUObjectArray", 0x4d8f910L);
    }

    private Path outputRoot;
    private final List<String> summary = new ArrayList<>();

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            throw new IllegalArgumentException("usage: VotVHeadlessReport.java <output-directory>");
        }
        outputRoot = Paths.get(args[0]).toAbsolutePath().normalize();
        Files.createDirectories(outputRoot);
        Files.createDirectories(outputRoot.resolve("decompiled"));

        String actualSha = currentProgram.getExecutableSHA256();
        summary.add("# VotV headless transformer/power report");
        summary.add("");
        summary.add("- Program: `" + currentProgram.getName() + "`");
        summary.add("- Executable: `" + currentProgram.getExecutablePath() + "`");
        summary.add("- SHA-256: `" + actualSha + "`");
        summary.add("- Image base: `" + currentProgram.getImageBase() + "`");
        summary.add("- Language: `" + currentProgram.getLanguageID() + "`");
        summary.add("- Compiler: `" + currentProgram.getCompilerSpec().getCompilerSpecID() + "`");
        summary.add("");
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha)) {
            throw new IllegalStateException("unexpected executable SHA-256: " + actualSha);
        }

        int definedHits = reportDefinedStrings();
        int rawHits = reportRawTerms();
        reportAnchors();

        summary.add("## Discovery totals");
        summary.add("");
        summary.add("- Matching defined strings: " + definedHits);
        summary.add("- Raw ASCII/UTF-16LE term hits: " + rawHits);
        summary.add("");
        summary.add("Blueprint-specific zero hits are expected: cooked Blueprint logic lives in ");
        summary.add("`.uasset/.uexp`, while this program contains the native UE4 runtime.");
        Files.write(outputRoot.resolve("summary.md"), summary, StandardCharsets.UTF_8);
        println("VotVHeadlessReport: wrote " + outputRoot);
    }

    private int reportDefinedStrings() throws Exception {
        Path path = outputRoot.resolve("defined_string_hits.tsv");
        int hits = 0;
        try (BufferedWriter out = Files.newBufferedWriter(path, StandardCharsets.UTF_8)) {
            out.write("term\taddress\tvalue\txref_count\txrefs\n");
            for (Data data : DefinedDataIterator.byDataInstance(currentProgram, Data::hasStringValue)) {
                monitor.checkCancelled();
                StringDataInstance instance = StringDataInstance.getStringDataInstance(data);
                String value = instance.getStringValue();
                if (value == null) continue;
                String folded = value.toLowerCase(Locale.ROOT);
                for (String term : TERMS) {
                    if (!folded.contains(term.toLowerCase(Locale.ROOT))) continue;
                    List<String> refs = refsTo(data.getAddress(), 100);
                    out.write(tsv(term, data.getAddress().toString(), value,
                        Integer.toString(refs.size()), String.join(";", refs)));
                    out.newLine();
                    hits++;
                }
            }
        }
        return hits;
    }

    private int reportRawTerms() throws Exception {
        Path path = outputRoot.resolve("raw_term_hits.tsv");
        Memory memory = currentProgram.getMemory();
        int total = 0;
        try (BufferedWriter out = Files.newBufferedWriter(path, StandardCharsets.UTF_8)) {
            out.write("term\tencoding\taddress\tblock\txref_count\txrefs\n");
            for (String term : TERMS) {
                for (String encoding : Arrays.asList("ASCII", "UTF-16LE")) {
                    byte[] needle = term.getBytes(
                        encoding.equals("ASCII") ? StandardCharsets.US_ASCII : StandardCharsets.UTF_16LE);
                    Set<Address> seen = new LinkedHashSet<>();
                    for (MemoryBlock block : memory.getBlocks()) {
                        if (!block.isInitialized()) continue;
                        Address cursor = block.getStart();
                        int perTermEncoding = 0;
                        while (cursor.compareTo(block.getEnd()) <= 0 && perTermEncoding < 500) {
                            monitor.checkCancelled();
                            Address hit = memory.findBytes(cursor, block.getEnd(), needle, null, true, monitor);
                            if (hit == null) break;
                            if (seen.add(hit)) {
                                List<String> refs = refsTo(hit, 100);
                                out.write(tsv(term, encoding, hit.toString(), block.getName(),
                                    Integer.toString(refs.size()), String.join(";", refs)));
                                out.newLine();
                                total++;
                                perTermEncoding++;
                            }
                            cursor = hit.add(1);
                        }
                    }
                }
            }
        }
        return total;
    }

    private void reportAnchors() throws Exception {
        Path path = outputRoot.resolve("anchor_xrefs.tsv");
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try (BufferedWriter out = Files.newBufferedWriter(path, StandardCharsets.UTF_8)) {
            out.write("name\trva\taddress\tblock\tfunction_entry\tfunction\txref_count\txrefs\n");
            for (Map.Entry<String, Long> item : ANCHORS.entrySet()) {
                monitor.checkCancelled();
                String name = item.getKey();
                long rva = item.getValue();
                Address address = currentProgram.getImageBase().add(rva);
                MemoryBlock block = currentProgram.getMemory().getBlock(address);
                Function function = currentProgram.getFunctionManager().getFunctionContaining(address);
                List<String> refs = refsTo(address, 5000);
                out.write(tsv(name, String.format("0x%x", rva), address.toString(),
                    block == null ? "<unmapped>" : block.getName(),
                    function == null ? "" : function.getEntryPoint().toString(),
                    function == null ? "<none>" : function.getName(true),
                    Integer.toString(refs.size()), String.join(";", refs)));
                out.newLine();
                if (function != null && block != null && block.isExecute()) {
                    DecompileResults result = decompiler.decompileFunction(function, 180, monitor);
                    Path decompPath = outputRoot.resolve("decompiled").resolve(name + ".c");
                    String text;
                    if (result.decompileCompleted() && result.getDecompiledFunction() != null) {
                        text = result.getDecompiledFunction().getC();
                    } else {
                        text = "/* decompilation failed: " + result.getErrorMessage() + " */\n";
                    }
                    Files.writeString(decompPath, text, StandardCharsets.UTF_8);
                }
            }
        } finally {
            decompiler.dispose();
        }
    }

    private List<String> refsTo(Address address, int limit) {
        List<String> rows = new ArrayList<>();
        ReferenceIterator iterator = currentProgram.getReferenceManager().getReferencesTo(address);
        while (iterator.hasNext() && rows.size() < limit) {
            Reference ref = iterator.next();
            Function owner = currentProgram.getFunctionManager().getFunctionContaining(ref.getFromAddress());
            rows.add(ref.getFromAddress() + "|" + ref.getReferenceType() + "|" +
                (owner == null ? "<no-function>" : owner.getName(true)));
        }
        return rows;
    }

    private static String tsv(String... values) {
        List<String> escaped = new ArrayList<>();
        for (String value : values) {
            escaped.add(value.replace("\\", "\\\\").replace("\t", "\\t")
                .replace("\r", "\\r").replace("\n", "\\n"));
        }
        return String.join("\t", escaped);
    }
}
