import CodeMirror from "@uiw/react-codemirror";
import { HighlightStyle, StreamLanguage, syntaxHighlighting } from "@codemirror/language";
import { tags } from "@lezer/highlight";
import { Hammer, Pause, RotateCcw, SkipForward, StepForward } from "lucide-react";
import { EditorView, Decoration } from "@codemirror/view";
import { useEffect, useMemo, useRef, useState } from "react";

import { Button } from "@/components/ui/button";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { useZenithAssembler } from "@/components/wasm/zenith-assembler-provider";
import { useZenithEmulator } from "@/components/wasm/zenith-emulator-provider";

const REGISTER_COUNT = 32;
const RUN_FRAME_BUDGET_MS = 8;
const FRAMEBUFFER_PREVIEW_WIDTH = 320;
const FRAMEBUFFER_PREVIEW_HEIGHT = 180;
const FRAMEBUFFER_WIDTH = 1920;
const FRAMEBUFFER_HEIGHT = 1080;

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
];

const ASSEMBLY_INSTRUCTIONS = new Set([
    "add",
    "sub",
    "mul",
    "div",
    "addi",
    "muli",
    "divi",
    "and",
    "or",
    "xor",
    "not",
    "sll",
    "srl",
    "sla",
    "sra",
    "l8",
    "l16",
    "l32",
    "l64",
    "s8",
    "s16",
    "s32",
    "s64",
]);

const ASSEMBLY_CONTROL_INSTRUCTIONS = new Set(["beq", "bne", "bge", "ble", "bgt", "blt", "jal", "jalr", "j"]);

const ASSEMBLY_REGISTERS = new Set([
    ...REGISTER_NAMES,
    ...Array.from({ length: REGISTER_COUNT }, (_, index) => `r${index}`),
    ...Array.from({ length: REGISTER_COUNT }, (_, index) => `z${index}`),
]);

const C_KEYWORDS = new Set([
    "break",
    "case",
    "char",
    "const",
    "continue",
    "default",
    "do",
    "double",
    "else",
    "enum",
    "float",
    "for",
    "if",
    "int",
    "long",
    "return",
    "short",
    "signed",
    "sizeof",
    "static",
    "struct",
    "switch",
    "typedef",
    "unsigned",
    "void",
    "while",
]);

const zenithHighlightStyle = HighlightStyle.define([
    { tag: [tags.comment, tags.lineComment, tags.blockComment], color: "var(--muted-foreground)", fontStyle: "italic" },
    { tag: [tags.keyword, tags.controlKeyword], color: "var(--primary)", fontWeight: "600" },
    { tag: tags.labelName, color: "oklch(0.57 0.17 86)", fontWeight: "600" },
    { tag: tags.variableName, color: "oklch(0.52 0.15 205)" },
    { tag: [tags.number, tags.integer], color: "oklch(0.57 0.19 36)" },
    { tag: [tags.string, tags.character], color: "oklch(0.48 0.14 145)" },
    { tag: [tags.operator, tags.punctuation, tags.separator], color: "var(--muted-foreground)" },
    { tag: tags.invalid, color: "var(--destructive)" },
]);

const assemblyLanguage = StreamLanguage.define({
    name: "zenith-assembly",
    token(stream) {
        if (stream.eatSpace()) return null;
        if (stream.match(/^#.*/)) return "lineComment";
        if (stream.match(/^-?(?:0x[0-9a-fA-F]+|\d+)/)) return "number";
        if (stream.match(/^\.[A-Za-z_][\w]*/)) return "keyword";
        if (stream.match(/^[A-Za-z_][\w]*/)) {
            const token = stream.current();
            if (stream.peek() === ":") return "labelName";
            if (ASSEMBLY_CONTROL_INSTRUCTIONS.has(token)) return "controlKeyword";
            if (ASSEMBLY_INSTRUCTIONS.has(token)) return "keyword";
            if (ASSEMBLY_REGISTERS.has(token)) return "variableName";
            return "variableName";
        }

        if (stream.match(/^[,:]/)) return "separator";

        stream.next();
        return "operator";
    },
});

const machineCodeLanguage = StreamLanguage.define({
    name: "zenith-machine-code",
    token(stream) {
        if (stream.eatSpace()) return null;
        if (stream.match(/^(?:0x)?[0-9a-fA-F]{1,8}/)) return "number";
        stream.match(/^\S+/);
        return "invalid";
    },
});

type CStreamState = {
    inBlockComment: boolean;
};

const cLikeLanguage = StreamLanguage.define<CStreamState>({
    name: "zenith-c",
    startState: () => ({ inBlockComment: false }),
    token(stream, state) {
        if (state.inBlockComment) {
            if (stream.skipTo("*/")) {
                stream.pos += 2;
                state.inBlockComment = false;
            } else {
                stream.skipToEnd();
            }

            return "blockComment";
        }

        if (stream.eatSpace()) return null;
        if (stream.match(/^\/\/.*/)) return "lineComment";
        if (stream.match(/^\/\*/)) {
            state.inBlockComment = true;
            return "blockComment";
        }

        if (stream.match(/^"(?:[^"\\]|\\.)*"?/)) return "string";
        if (stream.match(/^'(?:[^'\\]|\\.)*'?/)) return "character";
        if (stream.match(/^-?(?:0x[0-9a-fA-F]+|\d+(?:\.\d+)?)/)) return "number";
        if (stream.match(/^[A-Za-z_][\w]*/)) {
            return C_KEYWORDS.has(stream.current()) ? "keyword" : "variableName";
        }

        if (stream.match(/^[{}()[\],.;:]/)) return "punctuation";

        stream.next();
        return "operator";
    },
});

const DEFAULT_ASSEMBLY = `.main
  # a0 = previous fibonacci number
  # a1 = current fibonacci number
  # a4 = latest fibonacci number
  # a2/a3 = inner loop counter and limit
  # a5/a6 = outer loop counter and limit
  addi a0, zero, 0
  addi a1, zero, 1
  addi a3, zero, 16383
  addi a6, zero, 16383
  addi a5, zero, 0

outer: addi a2, zero, 0
inner: add a4, a0, a1
  add a0, a1, zero
  add a1, a4, zero
  addi a2, a2, 1
  blt a2, a3, inner
  addi a5, a5, 1
  blt a5, a6, outer
`;

const ZENITH_C_PLACEHOLDER = `// Zenith C is not implemented yet.
// Use the Zenith Assembly or Machine Code tabs for now.`;

type EditorTab = "zenith-c" | "assembly" | "machine-code";

const EDITOR_TABS: Array<{ id: EditorTab; label: string }> = [
    { id: "zenith-c", label: "Zenith C" },
    { id: "assembly", label: "Zenith Assembly" },
    { id: "machine-code", label: "Machine Code" },
];

const EDITOR_TAB_META = {
    "zenith-c": {
        title: "Zenith C",
        description: "The C frontend is not implemented yet.",
    },
    assembly: {
        title: "Zenith Assembly",
        description: "Compile assembly into the loaded machine-code program.",
    },
    "machine-code": {
        title: "Machine code",
        description: "Enter one raw 32-bit hex instruction per line.",
    },
} satisfies Record<EditorTab, { title: string; description: string }>;

type ParsedInstruction = {
    lineNumber: number;
    text: string;
    value: number;
};

type ParseResult = {
    instructions: Array<ParsedInstruction>;
    errors: Array<string>;
};

function createZeroRegisters() {
    return Array.from({ length: REGISTER_COUNT }, () => 0n);
}

function parseProgram(source: string): ParseResult {
    const instructions: Array<ParsedInstruction> = [];
    const errors: Array<string> = [];

    source.split(/\r?\n/).forEach((rawLine, index) => {
        const text = rawLine.trim();
        if (text.length === 0) {
            return;
        }

        if (!/^(?:0x)?[0-9a-fA-F]{1,8}$/.test(text)) {
            errors.push(`Line ${index + 1}: expected one 32-bit hex value.`);
            return;
        }

        const value = Number.parseInt(text.replace(/^0x/i, ""), 16);
        instructions.push({
            lineNumber: index + 1,
            text,
            value,
        });
    });

    return { instructions, errors };
}

function formatHex(value: bigint) {
    return `0x${BigInt.asUintN(64, value).toString(16).toUpperCase().padStart(16, "0")}`;
}

function decodeField1(instruction: number) {
    return (instruction >>> 7) & 0x1f;
}

function decodeField2(instruction: number) {
    return (instruction >>> 12) & 0x1f;
}

function signExtend(value: number, width: number) {
    const signBit = 1 << (width - 1);
    return (value & signBit) === 0 ? value : value - (1 << width);
}

function decodeImm15(instruction: number) {
    return signExtend(instruction >>> 17, 15);
}

function decodeImm20(instruction: number) {
    return signExtend(instruction >>> 12, 20);
}

function resolveNextInstructionIndex(instruction: number, currentIndex: number, registers: Array<bigint>) {
    const op = instruction & 0x7f;
    const pc = currentIndex * 4;
    let nextPc = pc + 4;

    if (op >= 0x14 && op <= 0x19) {
        const lhs = registers[decodeField1(instruction)] ?? 0n;
        const rhs = registers[decodeField2(instruction)] ?? 0n;
        const shouldBranch =
            (op === 0x14 && lhs === rhs) ||
            (op === 0x15 && lhs !== rhs) ||
            (op === 0x16 && lhs >= rhs) ||
            (op === 0x17 && lhs <= rhs) ||
            (op === 0x18 && lhs > rhs) ||
            (op === 0x19 && lhs < rhs);

        if (shouldBranch) {
            nextPc = pc + decodeImm15(instruction);
        }
    } else if (op === 0x1a) {
        nextPc = pc + decodeImm20(instruction);
    } else if (op === 0x1b) {
        nextPc = Number(registers[decodeField2(instruction)] ?? 0n) + decodeImm15(instruction);
    }

    return nextPc % 4 === 0 ? nextPc / 4 : -1;
}

function unknownErrorMessage(error: unknown) {
    return error instanceof Error ? error.message : String(error);
}

export function App() {
    const assembler = useZenithAssembler();
    const emulator = useZenithEmulator();
    const [activeTab, setActiveTab] = useState<EditorTab>("assembly");
    const [assemblySource, setAssemblySource] = useState(DEFAULT_ASSEMBLY);
    const [machineCode, setMachineCode] = useState("");
    const [assemblyError, setAssemblyError] = useState<string | null>(null);
    const [assemblyIsLoaded, setAssemblyIsLoaded] = useState(false);
    const [registers, setRegisters] = useState<Array<bigint>>(createZeroRegisters);
    const [instructionIndex, setInstructionIndex] = useState(0);
    const [isRunning, setIsRunning] = useState(false);
    const [lastMessage, setLastMessage] = useState("");
    const framebufferCanvasRef = useRef<HTMLCanvasElement | null>(null);
    const [framebufferRevision, setFramebufferRevision] = useState(0);

    const parseResult = useMemo(() => parseProgram(machineCode), [machineCode]);
    const activeInstruction = parseResult.instructions[instructionIndex] ?? null;
    const activeLine = activeInstruction?.lineNumber ?? null;
    const canStep =
        emulator !== null && parseResult.errors.length === 0 && instructionIndex < parseResult.instructions.length;
    const canRun = canStep || isRunning;
    const activeTabMeta = EDITOR_TAB_META[activeTab];
    const activeTabCanUseLoadedProgram = activeTab === "machine-code" || assemblyIsLoaded;
    const canUseStepControl = activeTabCanUseLoadedProgram && canStep && !isRunning;
    const canUseRunControl = isRunning || (activeTabCanUseLoadedProgram && canRun);

    const baseEditorExtensions = useMemo(
        () => [
            EditorView.lineWrapping,
            syntaxHighlighting(zenithHighlightStyle),
            EditorView.theme({
                "&": {
                    height: "100%",
                    backgroundColor: "transparent",
                    color: "var(--foreground)",
                    fontSize: "13px",
                },
                ".cm-scroller": {
                    fontFamily: "ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace",
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
                    backgroundColor: "color-mix(in oklch, var(--primary) 18%, transparent)",
                    boxShadow: "inset 3px 0 0 var(--primary)",
                },
                ".cm-focused": {
                    outline: "none",
                },
                ".cm-activeLine": {
                    backgroundColor: "color-mix(in oklch, var(--muted) 55%, transparent)",
                },
            }),
        ],
        []
    );

    const machineCodeEditorExtensions = useMemo(
        () => [
            ...baseEditorExtensions,
            machineCodeLanguage,
            EditorView.decorations.of((view) => {
                if (activeLine === null || activeLine > view.state.doc.lines) {
                    return Decoration.set([]);
                }

                const line = view.state.doc.line(activeLine);
                return Decoration.set([Decoration.line({ class: "cm-executing-line" }).range(line.from)]);
            }),
        ],
        [activeLine, baseEditorExtensions]
    );

    const assemblyEditorExtensions = useMemo(
        () => [...baseEditorExtensions, assemblyLanguage],
        [baseEditorExtensions]
    );

    const readOnlyEditorExtensions = useMemo(
        () => [...baseEditorExtensions, cLikeLanguage, EditorView.editable.of(false)],
        [baseEditorExtensions]
    );

    useEffect(() => {
        const canvas = framebufferCanvasRef.current;
        if (!canvas) return;

        const context = canvas.getContext("2d");
        if (!context) return;

        context.imageSmoothingEnabled = false;
        const imageData = context.createImageData(FRAMEBUFFER_PREVIEW_WIDTH, FRAMEBUFFER_PREVIEW_HEIGHT);

        if (emulator) {
            const framebuffer = emulator.getFramebuffer();
            for (let y = 0; y < FRAMEBUFFER_PREVIEW_HEIGHT; y += 1) {
                const sourceYStart = Math.floor((y * FRAMEBUFFER_HEIGHT) / FRAMEBUFFER_PREVIEW_HEIGHT);
                const sourceYEnd = Math.ceil(((y + 1) * FRAMEBUFFER_HEIGHT) / FRAMEBUFFER_PREVIEW_HEIGHT);
                for (let x = 0; x < FRAMEBUFFER_PREVIEW_WIDTH; x += 1) {
                    const destinationOffset = (y * FRAMEBUFFER_PREVIEW_WIDTH + x) * 4;
                    const sourceXStart = Math.floor((x * FRAMEBUFFER_WIDTH) / FRAMEBUFFER_PREVIEW_WIDTH);
                    const sourceXEnd = Math.ceil(((x + 1) * FRAMEBUFFER_WIDTH) / FRAMEBUFFER_PREVIEW_WIDTH);

                    let red = 0;
                    let green = 0;
                    let blue = 0;
                    for (let sourceY = sourceYStart; sourceY < sourceYEnd; sourceY += 1) {
                        for (let sourceX = sourceXStart; sourceX < sourceXEnd; sourceX += 1) {
                            const sourceOffset = (sourceY * FRAMEBUFFER_WIDTH + sourceX) * 3;
                            red = Math.max(red, framebuffer[sourceOffset] ?? 0);
                            green = Math.max(green, framebuffer[sourceOffset + 1] ?? 0);
                            blue = Math.max(blue, framebuffer[sourceOffset + 2] ?? 0);
                        }
                    }

                    imageData.data[destinationOffset] = red;
                    imageData.data[destinationOffset + 1] = green;
                    imageData.data[destinationOffset + 2] = blue;
                    imageData.data[destinationOffset + 3] = 255;
                }
            }
        } else {
            for (let i = 3; i < imageData.data.length; i += 4) {
                imageData.data[i] = 255;
            }
        }

        context.putImageData(imageData, 0, 0);
    }, [emulator, framebufferRevision]);

    useEffect(() => {
        if (!isRunning || !emulator || parseResult.errors.length > 0) {
            return;
        }

        let frame = 0;

        const executeFrame = () => {
            const frameDeadline = performance.now() + RUN_FRAME_BUDGET_MS;
            let nextInstructionIndex = instructionIndex;
            let nextRegisters = registers;
            let executed = 0;

            while (performance.now() < frameDeadline || executed === 0) {
                const instruction = parseResult.instructions[nextInstructionIndex];
                if (!instruction) {
                    setIsRunning(false);
                    setLastMessage("Program counter is outside the parsed instruction range.");
                    break;
                }

                const resolvedInstructionIndex = resolveNextInstructionIndex(
                    instruction.value,
                    nextInstructionIndex,
                    nextRegisters
                );
                emulator.step(instruction.value);
                nextRegisters = emulator.getRegisters();
                nextInstructionIndex = resolvedInstructionIndex;
                executed += 1;
            }

            setRegisters(nextRegisters);
            setInstructionIndex(nextInstructionIndex);
            setFramebufferRevision((revision) => revision + 1);

            if (executed > 0) {
                setLastMessage(
                    `Running continuously; executed ${executed} instruction${executed === 1 ? "" : "s"} this frame.`
                );
            }
        };

        frame = window.requestAnimationFrame(executeFrame);

        return () => window.cancelAnimationFrame(frame);
    }, [emulator, instructionIndex, isRunning, parseResult, registers]);

    function resetProgram() {
        setIsRunning(false);
        emulator?.reset();
        setRegisters(emulator?.getRegisters() ?? createZeroRegisters());
        setInstructionIndex(0);
        setFramebufferRevision((revision) => revision + 1);
        setLastMessage("Program counter reset to the first parsed machine-code line.");
    }

    function stepProgram() {
        if (!emulator) {
            setLastMessage("Emulator is still loading.");
            return;
        }

        if (parseResult.errors.length > 0) {
            setLastMessage("Fix the machine-code input before stepping.");
            return;
        }

        const instruction = parseResult.instructions[instructionIndex];
        if (!instruction) {
            setIsRunning(false);
            setLastMessage("Program counter is outside the parsed instruction range.");
            return;
        }

        const nextInstructionIndex = resolveNextInstructionIndex(instruction.value, instructionIndex, registers);
        emulator.step(instruction.value);
        setRegisters(emulator.getRegisters());
        setInstructionIndex(nextInstructionIndex);
        setFramebufferRevision((revision) => revision + 1);
        setLastMessage(`Executed line ${instruction.lineNumber}: ${instruction.text}`);
    }

    function toggleRunProgram() {
        if (isRunning) {
            setIsRunning(false);
            setLastMessage("Paused continuous execution.");
            return;
        }

        if (!activeTabCanUseLoadedProgram) {
            setLastMessage("Compile and load the assembly before running it.");
            return;
        }

        if (!canStep) {
            stepProgram();
            return;
        }

        setIsRunning(true);
        setLastMessage("Running continuously.");
    }

    function compileAndLoadAssembly() {
        if (!assembler) {
            setLastMessage("Assembler is still loading.");
            return;
        }

        try {
            const compiled = assembler.assemble(assemblySource);
            const compiledParseResult = parseProgram(compiled);
            setAssemblyError(null);
            setAssemblyIsLoaded(true);
            setIsRunning(false);
            emulator?.reset();
            setRegisters(emulator?.getRegisters() ?? createZeroRegisters());
            setInstructionIndex(0);
            setMachineCode(compiled);
            setFramebufferRevision((revision) => revision + 1);
            setActiveTab("machine-code");
            setLastMessage(
                `Compiled and loaded ${compiledParseResult.instructions.length} machine-code instruction${
                    compiledParseResult.instructions.length === 1 ? "" : "s"
                }.`
            );
        } catch (error) {
            const message = unknownErrorMessage(error);
            setAssemblyError(message);
            setAssemblyIsLoaded(false);
            setIsRunning(false);
            setLastMessage(`Assembly compile failed: ${message}`);
        }
    }

    function selectTab(tab: EditorTab) {
        if (tab === "zenith-c") {
            setIsRunning(false);
        }

        setActiveTab(tab);
    }

    return (
        <main className="grid h-svh grid-cols-[18rem_minmax(0,1fr)_22rem] overflow-hidden bg-background text-foreground">
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
                                    {REGISTER_NAMES[index]}
                                    <span className="ml-1 text-muted-foreground">z{index} </span>
                                </span>
                                <span className="truncate text-right text-muted-foreground" title={formatHex(value)}>
                                    {formatHex(value)}
                                </span>
                            </div>
                        ))}
                    </div>
                </div>
            </aside>

            <section className="grid min-h-0 grid-rows-[minmax(0,1fr)_15rem]">
                <Tabs
                    className="grid min-h-0 grid-rows-[auto_minmax(0,1fr)] gap-0"
                    onValueChange={(value) => selectTab(value as EditorTab)}
                    value={activeTab}
                >
                    <div className="flex flex-wrap items-center justify-between gap-3 border-b px-4 py-2">
                        <div className="min-w-0">
                            <TabsList className="mb-2">
                                {EDITOR_TABS.map((tab) => (
                                    <TabsTrigger
                                        key={tab.id}
                                        value={tab.id}
                                    >
                                        {tab.label}
                                    </TabsTrigger>
                                ))}
                            </TabsList>
                            <h2 className="text-sm font-semibold">{activeTabMeta.title}</h2>
                            <p className="text-xs text-muted-foreground">{activeTabMeta.description}</p>
                        </div>
                        {activeTab !== "zenith-c" ? (
                            <div className="flex items-center gap-2">
                                {activeTab === "assembly" ? (
                                    <Button
                                        disabled={!assembler}
                                        onClick={compileAndLoadAssembly}
                                        title="Compile and load assembly"
                                        variant="outline"
                                    >
                                        <Hammer />
                                        Compile & Load
                                    </Button>
                                ) : null}
                                <Button
                                    aria-label="Reset program"
                                    disabled={!emulator || !activeTabCanUseLoadedProgram}
                                    onClick={resetProgram}
                                    size="icon"
                                    title="Reset program"
                                    variant="outline"
                                >
                                    <RotateCcw />
                                </Button>
                                <Button
                                    aria-label="Step instruction"
                                    disabled={!canUseStepControl}
                                    onClick={stepProgram}
                                    size="icon"
                                    title="Step instruction"
                                    variant="secondary"
                                >
                                    <StepForward />
                                </Button>
                                <Button
                                    aria-label={isRunning ? "Pause continuous execution" : "Run continuously"}
                                    disabled={!canUseRunControl}
                                    onClick={toggleRunProgram}
                                    size="icon"
                                    title={isRunning ? "Pause continuous execution" : "Run continuously"}
                                >
                                    {isRunning ? <Pause /> : <SkipForward />}
                                </Button>
                            </div>
                        ) : null}
                    </div>
                    <TabsContent className="m-0 min-h-0 overflow-hidden" value="zenith-c">
                        <CodeMirror
                            basicSetup={{
                                autocompletion: false,
                                bracketMatching: false,
                                foldGutter: false,
                                highlightSelectionMatches: false,
                            }}
                            extensions={readOnlyEditorExtensions}
                            height="100%"
                            value={ZENITH_C_PLACEHOLDER}
                        />
                    </TabsContent>
                    <TabsContent className="m-0 min-h-0 overflow-hidden" value="assembly">
                        <CodeMirror
                            basicSetup={{
                                autocompletion: false,
                                bracketMatching: false,
                                foldGutter: false,
                                highlightSelectionMatches: false,
                            }}
                            extensions={assemblyEditorExtensions}
                            height="100%"
                            onChange={(value) => {
                                setAssemblySource(value);
                                setAssemblyError(null);
                                setAssemblyIsLoaded(false);
                                setIsRunning(false);
                                setLastMessage("Assembly changed. Compile and load before running those changes.");
                            }}
                            value={assemblySource}
                        />
                    </TabsContent>
                    <TabsContent className="m-0 min-h-0 overflow-hidden" value="machine-code">
                        <CodeMirror
                            basicSetup={{
                                autocompletion: false,
                                bracketMatching: false,
                                foldGutter: false,
                                highlightSelectionMatches: false,
                            }}
                            extensions={machineCodeEditorExtensions}
                            height="100%"
                            onChange={(value) => {
                                setIsRunning(false);
                                setMachineCode(value);
                                setAssemblyIsLoaded(false);
                                setInstructionIndex(0);
                                setLastMessage("Machine code changed. Reset or step from the first parsed line.");
                            }}
                            value={machineCode}
                        />
                    </TabsContent>
                </Tabs>

                <footer className="grid min-h-0 grid-rows-[auto_minmax(0,1fr)] border-t bg-muted/20">
                    <div className="flex items-center justify-between border-b px-4 py-2">
                        <h2 className="text-sm font-semibold">Output</h2>
                        <span className="font-mono text-xs text-muted-foreground">
                            {parseResult.instructions.length} instruction
                            {parseResult.instructions.length === 1 ? "" : "s"} loaded
                        </span>
                    </div>
                    <div className="min-h-0 overflow-auto p-4 font-mono text-xs leading-6">
                        {!emulator ? <p>Loading emulator...</p> : null}
                        {activeTab === "assembly" && !assembler ? <p>Loading assembler...</p> : null}
                        {assemblyError ? (
                            <div className="text-destructive">
                                <p>{assemblyError}</p>
                            </div>
                        ) : parseResult.errors.length > 0 ? (
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
                                {activeTab === "assembly" ? (
                                    <p>{assemblyIsLoaded ? "assembly output is loaded" : "assembly is not loaded"}</p>
                                ) : null}
                            </>
                        )}
                    </div>
                </footer>
            </section>

            <aside className="grid min-h-0 grid-rows-[auto_minmax(0,1fr)] border-l bg-muted/20">
                <header className="border-b px-4 py-3">
                    <h2 className="text-sm font-semibold">Peripherals</h2>
                    <p className="text-xs text-muted-foreground">Framebuffer and future MMIO devices</p>
                </header>
                <div className="min-h-0 overflow-auto p-4">
                    <section className="rounded-lg border bg-background">
                        <div className="flex items-center justify-between border-b px-3 py-2">
                            <h3 className="text-xs font-semibold uppercase tracking-wide text-muted-foreground">
                                Framebuffer
                            </h3>
                            <span className="font-mono text-[11px] text-muted-foreground">
                                {FRAMEBUFFER_WIDTH}x{FRAMEBUFFER_HEIGHT}
                            </span>
                        </div>
                        <div className="p-3">
                            <canvas
                                aria-label="Framebuffer preview"
                                className="aspect-video w-full border bg-black [image-rendering:pixelated]"
                                height={FRAMEBUFFER_PREVIEW_HEIGHT}
                                ref={framebufferCanvasRef}
                                width={FRAMEBUFFER_PREVIEW_WIDTH}
                            />
                            <div className="mt-3 grid grid-cols-[auto_minmax(0,1fr)] gap-x-3 gap-y-1 font-mono text-[11px] text-muted-foreground">
                                <span>base</span>
                                <span className="truncate text-right">0xFFFF_FFFF_FFA1_1400</span>
                                <span>format</span>
                                <span className="text-right">RGB888</span>
                            </div>
                        </div>
                    </section>
                </div>
            </aside>
        </main>
    );
}

export default App;
