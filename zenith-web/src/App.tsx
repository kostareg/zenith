import CodeMirror from "@uiw/react-codemirror"
import { EditorView, Decoration } from "@codemirror/view"
import { Pause, RotateCcw, SkipForward, StepForward } from "lucide-react"
import { useEffect, useMemo, useState } from "react"

import { useZenithEmulator } from "@/components/zenith-emulator-provider"
import { Button } from "@/components/ui/button"

const REGISTER_COUNT = 32
const PROGRAM_SCALE = 1_000_000_000_000
const INSTRUCTIONS_PER_FRAME = 100

const REGISTER_NAMES = [
  "zero",
  "ra",
  "sp",
  "a0",
  "a1",
  "a2",
  "a3",
  "a4",
  "a5",
  "a6",
  "a7",
  "t0",
  "t1",
  "t2",
  "t3",
  "t4",
  "t5",
  "t6",
  "t7",
  "s0",
  "s1",
  "s2",
  "s3",
  "s4",
  "s5",
  "s6",
  "s7",
  "k0",
  "k1",
  "k2",
  "k3",
  "k4",
]

const DEFAULT_PROGRAM = `0x4E200184
0x4E203185
0x4E203185
0x00063205
0x00083285
0x00040304
0x00026404
0x00046484
0x00106502
0x0012A502
0x00145583
0x00164200
0x00046304
0x00026404
0x00046484
0x00106502
0x0012A502
0x00145583
0x00164201
0x00046304
0xFFFC801A`

type ParsedInstruction = {
  lineNumber: number
  text: string
  value: number
}

type ParseResult = {
  instructions: Array<ParsedInstruction>
  errors: Array<string>
}

function parseProgram(source: string): ParseResult {
  const instructions: Array<ParsedInstruction> = []
  const errors: Array<string> = []

  source.split(/\r?\n/).forEach((rawLine, index) => {
    const text = rawLine.trim()
    if (text.length === 0) {
      return
    }

    if (!/^(?:0x)?[0-9a-fA-F]{1,8}$/.test(text)) {
      errors.push(`Line ${index + 1}: expected one 32-bit hex value.`)
      return
    }

    const value = Number.parseInt(text.replace(/^0x/i, ""), 16)
    instructions.push({
      lineNumber: index + 1,
      text,
      value,
    })
  })

  return { instructions, errors }
}

function formatHex(value: bigint) {
  return `0x${BigInt.asUintN(64, value).toString(16).toUpperCase().padStart(16, "0")}`
}

function formatFixedPoint(value: bigint, scale: bigint) {
  const sign = value < 0n ? "-" : ""
  const magnitude = value < 0n ? -value : value
  const whole = magnitude / scale
  const fraction = magnitude % scale
  const fractionWidth = scale.toString().length - 1

  return `${sign}${whole}.${fraction.toString().padStart(fractionWidth, "0")}`
}

function decodeField1(instruction: number) {
  return (instruction >>> 7) & 0x1f
}

function decodeField2(instruction: number) {
  return (instruction >>> 12) & 0x1f
}

function signExtend(value: number, width: number) {
  const signBit = 1 << (width - 1)
  return (value & signBit) === 0 ? value : value - (1 << width)
}

function decodeImm15(instruction: number) {
  return signExtend(instruction >>> 17, 15)
}

function decodeImm20(instruction: number) {
  return signExtend(instruction >>> 12, 20)
}

function resolveNextInstructionIndex(
  instruction: number,
  currentIndex: number,
  registers: Array<bigint>
) {
  const op = instruction & 0x7f
  const pc = currentIndex * 4
  let nextPc = pc + 4

  if (op >= 0x14 && op <= 0x19) {
    const lhs = registers[decodeField1(instruction)] ?? 0n
    const rhs = registers[decodeField2(instruction)] ?? 0n
    const shouldBranch =
      (op === 0x14 && lhs === rhs) ||
      (op === 0x15 && lhs !== rhs) ||
      (op === 0x16 && lhs >= rhs) ||
      (op === 0x17 && lhs <= rhs) ||
      (op === 0x18 && lhs > rhs) ||
      (op === 0x19 && lhs < rhs)

    if (shouldBranch) {
      nextPc = pc + decodeImm15(instruction)
    }
  } else if (op === 0x1a) {
    nextPc = pc + decodeImm20(instruction)
  } else if (op === 0x1b) {
    nextPc =
      Number(registers[decodeField2(instruction)] ?? 0n) +
      decodeImm15(instruction)
  }

  return nextPc % 4 === 0 ? nextPc / 4 : -1
}

export function App() {
  const { emulator, error, isLoading } = useZenithEmulator()
  const [source, setSource] = useState(DEFAULT_PROGRAM)
  const [registers, setRegisters] = useState<Array<bigint>>(
    Array.from({ length: REGISTER_COUNT }, () => 0n)
  )
  const [instructionIndex, setInstructionIndex] = useState(0)
  const [isRunning, setIsRunning] = useState(false)
  const [lastMessage, setLastMessage] = useState(
    "Loaded fixed-point Nilakantha pi convergence program."
  )

  const parseResult = useMemo(() => parseProgram(source), [source])
  const activeInstruction = parseResult.instructions[instructionIndex] ?? null
  const activeLine = activeInstruction?.lineNumber ?? null
  const piEstimateRaw = registers[4] ?? 0n
  const piEstimate = Number(piEstimateRaw) / PROGRAM_SCALE
  const piEstimateText = formatFixedPoint(piEstimateRaw, BigInt(PROGRAM_SCALE))
  const piDelta = Math.abs(Math.PI - piEstimate)
  const canStep =
    emulator !== null &&
    parseResult.errors.length === 0 &&
    instructionIndex < parseResult.instructions.length
  const canRun = canStep || isRunning

  const editorExtensions = useMemo(
    () => [
      EditorView.lineWrapping,
      EditorView.theme({
        "&": {
          height: "100%",
          backgroundColor: "transparent",
          color: "var(--foreground)",
          fontSize: "13px",
        },
        ".cm-scroller": {
          fontFamily:
            "ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace",
        },
        ".cm-content": {
          caretColor: "var(--foreground)",
          padding: "12px 0",
        },
        ".cm-gutters": {
          backgroundColor: "color-mix(in oklch, var(--muted) 40%, transparent)",
          borderColor: "var(--border)",
          color: "var(--muted-foreground)",
        },
        ".cm-line": {
          padding: "0 12px",
        },
        ".cm-executing-line": {
          backgroundColor:
            "color-mix(in oklch, var(--primary) 18%, transparent)",
          boxShadow: "inset 3px 0 0 var(--primary)",
        },
        ".cm-focused": {
          outline: "none",
        },
        ".cm-activeLine": {
          backgroundColor: "color-mix(in oklch, var(--muted) 55%, transparent)",
        },
      }),
      EditorView.decorations.of((view) => {
        if (activeLine === null || activeLine > view.state.doc.lines) {
          return Decoration.set([])
        }

        const line = view.state.doc.line(activeLine)
        return Decoration.set([
          Decoration.line({ class: "cm-executing-line" }).range(line.from),
        ])
      }),
    ],
    [activeLine]
  )

  useEffect(() => {
    if (!isRunning || !emulator || parseResult.errors.length > 0) {
      return
    }

    let frame = 0

    const executeFrame = () => {
      let nextInstructionIndex = instructionIndex
      let nextRegisters = registers
      let executed = 0

      while (executed < INSTRUCTIONS_PER_FRAME) {
        const instruction = parseResult.instructions[nextInstructionIndex]
        if (!instruction) {
          setIsRunning(false)
          setLastMessage(
            "Program counter is outside the parsed instruction range."
          )
          break
        }

        const resolvedInstructionIndex = resolveNextInstructionIndex(
          instruction.value,
          nextInstructionIndex,
          nextRegisters
        )
        emulator.step(instruction.value)
        nextRegisters = emulator.getRegisters()
        nextInstructionIndex = resolvedInstructionIndex
        executed += 1
      }

      setRegisters(nextRegisters)
      setInstructionIndex(nextInstructionIndex)

      if (executed > 0) {
        setLastMessage(
          `Running continuously; executed ${executed} more instruction${executed === 1 ? "" : "s"}.`
        )
      }
    }

    frame = window.requestAnimationFrame(executeFrame)

    return () => window.cancelAnimationFrame(frame)
  }, [emulator, instructionIndex, isRunning, parseResult, registers])

  function resetProgram() {
    setIsRunning(false)
    emulator?.reset()
    setRegisters(
      emulator?.getRegisters() ??
        Array.from({ length: REGISTER_COUNT }, () => 0n)
    )
    setInstructionIndex(0)
    setLastMessage("Program counter reset to the first parsed hex line.")
  }

  function stepProgram() {
    if (!emulator) {
      setLastMessage("Emulator is still loading.")
      return
    }

    if (parseResult.errors.length > 0) {
      setLastMessage("Fix the hex input before stepping.")
      return
    }

    const instruction = parseResult.instructions[instructionIndex]
    if (!instruction) {
      setIsRunning(false)
      setLastMessage("Program counter is outside the parsed instruction range.")
      return
    }

    const nextInstructionIndex = resolveNextInstructionIndex(
      instruction.value,
      instructionIndex,
      registers
    )
    emulator.step(instruction.value)
    setRegisters(emulator.getRegisters())
    setInstructionIndex(nextInstructionIndex)
    setLastMessage(
      `Executed line ${instruction.lineNumber}: ${instruction.text}`
    )
  }

  function toggleRunProgram() {
    if (isRunning) {
      setIsRunning(false)
      setLastMessage("Paused continuous execution.")
      return
    }

    if (!canStep) {
      stepProgram()
      return
    }

    setIsRunning(true)
    setLastMessage("Running continuously.")
  }

  return (
    <main className="grid h-svh grid-cols-[18rem_minmax(0,1fr)] overflow-hidden bg-background text-foreground">
      <aside className="flex min-h-0 flex-col border-r bg-muted/30">
        <header className="border-b px-4 py-3">
          <h1 className="text-sm font-semibold">Zenith Web</h1>
          <p className="text-xs text-muted-foreground">CPU registers</p>
        </header>
        <div className="min-h-0 flex-1 overflow-auto px-3 py-2">
          <div className="grid gap-1">
            {registers.map((value, index) => (
              <div
                className="grid grid-cols-[3.5rem_minmax(0,1fr)] items-baseline gap-2 rounded-md px-2 py-1 font-mono text-xs hover:bg-muted"
                key={index}
              >
                <span className="font-semibold text-foreground">
                  z{index}
                  <span className="ml-1 text-muted-foreground">
                    {REGISTER_NAMES[index]}
                  </span>
                </span>
                <span
                  className="truncate text-right text-muted-foreground"
                  title={formatHex(value)}
                >
                  {formatHex(value)}
                </span>
              </div>
            ))}
          </div>
        </div>
      </aside>

      <section className="grid min-h-0 grid-rows-[minmax(0,1fr)_15rem]">
        <div className="grid min-h-0 grid-rows-[auto_minmax(0,1fr)]">
          <div className="flex items-center justify-between border-b px-4 py-2">
            <div>
              <h2 className="text-sm font-semibold">Assembly editor</h2>
              <p className="text-xs text-muted-foreground">
                Enter one raw 32-bit hex instruction per line.
              </p>
            </div>
            <div className="flex items-center gap-2">
              <Button
                aria-label="Reset program"
                disabled={!emulator}
                onClick={resetProgram}
                size="icon"
                title="Reset program"
                variant="outline"
              >
                <RotateCcw />
              </Button>
              <Button
                aria-label="Step instruction"
                disabled={!canStep || isRunning}
                onClick={stepProgram}
                size="icon"
                title="Step instruction"
                variant="secondary"
              >
                <StepForward />
              </Button>
              <Button
                aria-label={
                  isRunning ? "Pause continuous execution" : "Run continuously"
                }
                disabled={!canRun}
                onClick={toggleRunProgram}
                size="icon"
                title={
                  isRunning ? "Pause continuous execution" : "Run continuously"
                }
              >
                {isRunning ? <Pause /> : <SkipForward />}
              </Button>
            </div>
          </div>
          <div className="min-h-0 overflow-hidden">
            <CodeMirror
              basicSetup={{
                autocompletion: false,
                bracketMatching: false,
                foldGutter: false,
                highlightSelectionMatches: false,
              }}
              extensions={editorExtensions}
              height="100%"
              onChange={(value) => {
                setIsRunning(false)
                setSource(value)
                setInstructionIndex(0)
                setLastMessage(
                  "Source changed. Reset or step from the first parsed line."
                )
              }}
              value={source}
            />
          </div>
        </div>

        <footer className="grid min-h-0 grid-rows-[auto_minmax(0,1fr)] border-t bg-muted/20">
          <div className="flex items-center justify-between border-b px-4 py-2">
            <h2 className="text-sm font-semibold">Compiler output</h2>
            <span className="font-mono text-xs text-muted-foreground">
              {parseResult.instructions.length} instruction
              {parseResult.instructions.length === 1 ? "" : "s"}
            </span>
          </div>
          <div className="min-h-0 overflow-auto p-4 font-mono text-xs leading-6">
            {isLoading ? <p>Loading emulator...</p> : null}
            {error ? (
              <p className="text-destructive">
                Failed to load emulator: {error.message}
              </p>
            ) : null}
            {parseResult.errors.length > 0 ? (
              <div className="text-destructive">
                {parseResult.errors.map((parseError) => (
                  <p key={parseError}>{parseError}</p>
                ))}
              </div>
            ) : (
              <>
                <p>{lastMessage}</p>
                <p>
                  next:{" "}
                  {activeInstruction
                    ? `line ${activeInstruction.lineNumber} (${activeInstruction.text})`
                    : "outside parsed program"}
                </p>
                <p>
                  pi estimate: {piEstimateText} scaled in z4, delta{" "}
                  {piDelta.toFixed(6)}
                </p>
                <p>z3 scale=1e12, z4 pi estimate, z5 numerator, z6 current n</p>
              </>
            )}
          </div>
        </footer>
      </section>
    </main>
  )
}

export default App
