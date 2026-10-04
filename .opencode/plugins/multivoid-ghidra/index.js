import { execFile } from "node:child_process"
import { Plugin } from "/home/matt/.config/opencode/node_modules/@opencode/plugin/dist/promise/index.js"

const GHIDRA =
  "/home/matt/Tools/ghidra-12.1.4/support/analyzeHeadless"

const PROJECT_DIR =
  "/home/matt/GhidraProjects"

const PROJECT_NAME =
  "VotV-headless"

const PROGRAM =
  "VotV-Win64-Shipping.exe"

const SCRIPT_DIR =
  "/home/matt/.config/opencode/ghidra"

const SCRIPT =
  "VotVQuery.java"

const BEGIN =
  "=== MULTIVOID_GHIDRA_QUERY_BEGIN ==="

const END =
  "=== MULTIVOID_GHIDRA_QUERY_END ==="

function validate(mode, target) {
  if (!["function", "address", "string"].includes(mode)) {
    throw new Error(`Unsupported ghidra_query mode: ${mode}`)
  }

  if (typeof target !== "string" || target.length === 0) {
    throw new Error("ghidra_query target must be a non-empty string")
  }

  if (target.length > 256) {
    throw new Error("ghidra_query target is too long")
  }

  if (/[\0\r\n]/.test(target)) {
    throw new Error("ghidra_query target contains forbidden control characters")
  }

  if (
    mode === "address" &&
    !/^(?:0x)?[0-9a-fA-F]{1,16}$/.test(target)
  ) {
    throw new Error(
      "Address mode requires a hexadecimal virtual address"
    )
  }
}

function runHeadless(args) {
  return new Promise((resolve, reject) => {
    execFile(
      GHIDRA,
      args,
      {
        encoding: "utf8",
        timeout: 60000,
        maxBuffer: 2 * 1024 * 1024,
      },
      (error, stdout, stderr) => {
        const output =
          `${stdout ?? ""}\n${stderr ?? ""}`

        if (error) {
          const bounded = output.slice(-16000)

          reject(
            new Error(
              `Ghidra headless query failed: ${error.message}\n${bounded}`
            )
          )

          return
        }

        resolve(output)
      }
    )
  })
}

function extractReport(text) {
  const start = text.indexOf(BEGIN)

  if (start < 0) {
    throw new Error(
      "Ghidra query completed but the BEGIN marker was not found"
    )
  }

  const finish = text.indexOf(
    END,
    start + BEGIN.length
  )

  if (finish < 0) {
    throw new Error(
      "Ghidra query completed but the END marker was not found"
    )
  }

  let report =
    text.slice(start, finish + END.length)

  /*
   * Ghidra prefixes lines printed through println() with:
   *
   *   INFO  VotVQuery.java> ... (GhidraScript)
   *
   * Raw multiline decompiler output does not necessarily have that
   * prefix, so strip it only when present.
   */
  report = report
    .split(/\r?\n/)
    .map((line) =>
      line
        .replace(/^.*?VotVQuery\.java>\s?/, "")
        .replace(/\s+\(GhidraScript\)\s*$/, "")
    )
    .join("\n")
    .trim()

  return report
}

export default Plugin.define({
  id: "multivoid-ghidra",

  async setup(ctx) {
    await ctx.tool.transform((editor) => {
      editor.add({
        name: "ghidra_query",

        description:
          "Read-only bounded query of the prepared Voices of the Void Ghidra project. " +
          "Use mode=function for an analyzed function name such as FUN_1412e0310, " +
          "mode=address for a virtual address, or mode=string for defined-string/xref lookup.",

        /*
         * Keep this false. Multivoid-Re has Code Mode/execute denied,
         * and this tool must remain directly callable.
         */
        options: {
          codemode: false,
        },

        input: {
          type: "object",

          properties: {
            mode: {
              type: "string",
              enum: [
                "function",
                "address",
                "string",
              ],
            },

            target: {
              type: "string",
              minLength: 1,
              maxLength: 256,
            },
          },

          required: [
            "mode",
            "target",
          ],

          additionalProperties: false,
        },

        async execute(input) {
          const mode = input.mode
          const target = input.target

          validate(mode, target)

          /*
           * No shell is involved.
           *
           * The model controls only these last two argv elements:
           *   mode
           *   target
           *
           * Everything capable of selecting the executable/project/script
           * is fixed here.
           */
          const output = await runHeadless([
            PROJECT_DIR,
            PROJECT_NAME,

            "-process",
            PROGRAM,

            "-readOnly",
            "-noanalysis",

            "-scriptPath",
            SCRIPT_DIR,

            "-postScript",
            SCRIPT,

            mode,
            target,
          ])

          const report = extractReport(output)

          return {
            content: report,
          }
        },
      })
    })
  },
})
