import { tool } from "/home/matt/.config/opencode/node_modules/@opencode-ai/plugin/dist/index.js"

export default tool({
  description: "Diagnostic probe for the Multivoid Ghidra query interface.",

  args: {
    mode: tool.schema.enum(["function", "address", "string"]),
    target: tool.schema.string(),
  },

  async execute(args) {
    return `GHIDRA_QUERY_PROBE_OK mode=${args.mode} target=${args.target}`
  },
})
