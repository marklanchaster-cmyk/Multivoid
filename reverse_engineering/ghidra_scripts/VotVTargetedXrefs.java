// VotVTargetedXrefs.java
// @category Multivoid
// @description Follow inbound references from the few relevant native string anchors.

import java.io.BufferedWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayDeque;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Set;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class VotVTargetedXrefs extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "ad478218ec5513cc4c1682937db3214cbf2694d1dcd0583eb423447c049dd3ae";

    private static final Map<String, Long> TARGETS = new LinkedHashMap<>();
    static {
        TARGETS.put("IsBroken_string", 0x1441f5068L);
        TARGETS.put("UFunction_string", 0x143c2d7c8L);
        TARGETS.put("BlueprintGeneratedClass_string", 0x143c2ac90L);
    }

    private static class Node {
        final String root;
        final Address address;
        final int depth;
        Node(String root, Address address, int depth) {
            this.root = root;
            this.address = address;
            this.depth = depth;
        }
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            throw new IllegalArgumentException("usage: VotVTargetedXrefs.java <output-directory>");
        }
        String actualSha = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha)) {
            throw new IllegalStateException(
                "refusing unexpected executable SHA-256: " + actualSha);
        }
        Path root = Paths.get(args[0]).toAbsolutePath().normalize();
        Path decompRoot = root.resolve("targeted_decompiled");
        Files.createDirectories(decompRoot);

        Set<Address> decompiled = new HashSet<>();
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try (BufferedWriter out = Files.newBufferedWriter(
                root.resolve("targeted_xref_graph.tsv"), StandardCharsets.UTF_8)) {
            out.write("root\tdepth\tto_address\tfrom_address\tref_type\towner_entry\towner_function\n");
            for (Map.Entry<String, Long> target : TARGETS.entrySet()) {
                ArrayDeque<Node> queue = new ArrayDeque<>();
                Set<Address> visited = new HashSet<>();
                queue.add(new Node(target.getKey(), toAddr(target.getValue()), 0));
                while (!queue.isEmpty()) {
                    monitor.checkCancelled();
                    Node node = queue.removeFirst();
                    if (!visited.add(node.address)) continue;
                    ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(node.address);
                    int count = 0;
                    while (refs.hasNext() && count++ < 2000) {
                        Reference ref = refs.next();
                        Address from = ref.getFromAddress();
                        Function owner = currentProgram.getFunctionManager().getFunctionContaining(from);
                        out.write(clean(node.root) + "\t" + node.depth + "\t" + node.address + "\t" +
                            from + "\t" + clean(ref.getReferenceType().toString()) + "\t" +
                            (owner == null ? "" : owner.getEntryPoint()) + "\t" +
                            (owner == null ? "<no-function>" : clean(owner.getName(true))) + "\n");
                        if (owner != null && decompiled.add(owner.getEntryPoint())) {
                            DecompileResults result = decompiler.decompileFunction(owner, 180, monitor);
                            String body = result.decompileCompleted() && result.getDecompiledFunction() != null
                                ? result.getDecompiledFunction().getC()
                                : "/* decompilation failed: " + result.getErrorMessage() + " */\n";
                            Files.writeString(decompRoot.resolve(owner.getEntryPoint() + ".c"),
                                body, StandardCharsets.UTF_8);
                        }
                        // Data references often pass through a registration/name table before
                        // reaching code. Follow those non-function sources for two levels.
                        if (owner == null && node.depth < 2) {
                            queue.addLast(new Node(node.root, from, node.depth + 1));
                        }
                    }
                }
            }
        } finally {
            decompiler.dispose();
        }
        println("VotVTargetedXrefs: wrote " + root);
    }

    private static String clean(String value) {
        return value.replace("\t", "\\t").replace("\r", "\\r").replace("\n", "\\n");
    }
}
