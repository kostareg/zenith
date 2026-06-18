import CodeMirror from "@uiw/react-codemirror";
import { HighlightStyle, StreamLanguage, syntaxHighlighting } from "@codemirror/language";
import { tags } from "@lezer/highlight";
import { Hammer, Maximize2, Pause, RotateCcw, SkipForward, StepForward, X } from "lucide-react";
import { EditorView, Decoration, ViewPlugin, type ViewUpdate } from "@codemirror/view";
import { useEffect, useMemo, useRef, useState } from "react";

import { Button } from "@/components/ui/button";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { useZenithAssembler } from "@/components/wasm/zenith-assembler-provider";
import { useZenithCompiler } from "@/components/wasm/zenith-compiler-provider";
import { useZenithEmulator } from "@/components/wasm/zenith-emulator-provider";

// todo: find a better way to import the kernel, allow users to run userspace programs
import zenithKernel from "@/../public/zenith-kernel/main.c?raw"

const REGISTER_COUNT = 32;
const RUN_FRAME_BUDGET_MS = 8;
const FRAMEBUFFER_PREVIEW_WIDTH = 320;
const FRAMEBUFFER_PREVIEW_HEIGHT = 180;
const FRAMEBUFFER_WIDTH = 1920;
const FRAMEBUFFER_HEIGHT = 1080;
const INSTRUCTION_FRAME_HISTORY_LIMIT = 90;
const INSTRUCTION_CHART_WIDTH = 280;
const INSTRUCTION_CHART_HEIGHT = 72;
const INSTRUCTION_CHART_PADDING = 6;
const EDITOR_MINIMAP_WIDTH = 52;

function paintFramebufferPreview(canvas: HTMLCanvasElement, framebuffer: ArrayLike<number> | null) {
    const context = canvas.getContext("2d");
    if (!context) return;

    context.imageSmoothingEnabled = false;
    const imageData = context.createImageData(FRAMEBUFFER_PREVIEW_WIDTH, FRAMEBUFFER_PREVIEW_HEIGHT);

    if (framebuffer) {
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
}

function paintFramebufferFullSize(canvas: HTMLCanvasElement, framebuffer: ArrayLike<number> | null) {
    const context = canvas.getContext("2d");
    if (!context) return;

    context.imageSmoothingEnabled = false;
    const imageData = context.createImageData(FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT);

    if (framebuffer) {
        for (let sourceOffset = 0, destinationOffset = 0; sourceOffset < framebuffer.length; sourceOffset += 3) {
            imageData.data[destinationOffset] = framebuffer[sourceOffset] ?? 0;
            imageData.data[destinationOffset + 1] = framebuffer[sourceOffset + 1] ?? 0;
            imageData.data[destinationOffset + 2] = framebuffer[sourceOffset + 2] ?? 0;
            imageData.data[destinationOffset + 3] = 255;
            destinationOffset += 4;
        }
    } else {
        for (let i = 3; i < imageData.data.length; i += 4) {
            imageData.data[i] = 255;
        }
    }

    context.putImageData(imageData, 0, 0);
}

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

const editorMinimap = ViewPlugin.fromClass(
    class {
        private readonly view: EditorView;
        private readonly dom: HTMLDivElement;
        private readonly lineLayer: HTMLDivElement;
        private readonly viewportMarker: HTMLDivElement;
        private animationFrame: number | null = null;
        private dragging = false;

        constructor(view: EditorView) {
            this.view = view;
            this.dom = document.createElement("div");
            this.dom.className = "cm-minimap";

            this.lineLayer = document.createElement("div");
            this.lineLayer.className = "cm-minimap-lines";

            this.viewportMarker = document.createElement("div");
            this.viewportMarker.className = "cm-minimap-viewport";

            this.dom.append(this.lineLayer, this.viewportMarker);
            this.view.dom.append(this.dom);

            this.dom.addEventListener("pointerdown", this.handlePointerDown);
            this.view.scrollDOM.addEventListener("scroll", this.updateViewport, { passive: true });
            this.scheduleRender();
        }

        update(update: ViewUpdate) {
            if (update.docChanged || update.geometryChanged || update.viewportChanged) {
                this.scheduleRender();
            } else {
                this.updateViewport();
            }
        }

        destroy() {
            if (this.animationFrame !== null) {
                cancelAnimationFrame(this.animationFrame);
            }

            window.removeEventListener("pointermove", this.handlePointerMove);
            window.removeEventListener("pointerup", this.handlePointerUp);
            this.view.scrollDOM.removeEventListener("scroll", this.updateViewport);
            this.dom.removeEventListener("pointerdown", this.handlePointerDown);
            this.dom.remove();
        }

        private scheduleRender = () => {
            if (this.animationFrame !== null) return;

            this.animationFrame = requestAnimationFrame(() => {
                this.animationFrame = null;
                this.render();
            });
        };

        private render() {
            const lines = this.view.state.doc;
            const minimapHeight = Math.max(this.dom.clientHeight, 1);
            const gap = lines.lines > minimapHeight / 2 ? 0 : 1;
            const lineHeight = Math.min(3, Math.max(0.1, (minimapHeight - 12 - gap * (lines.lines - 1)) / lines.lines));
            const fragment = document.createDocumentFragment();

            this.lineLayer.textContent = "";
            this.lineLayer.style.setProperty("--cm-minimap-line-height", `${lineHeight}px`);
            this.lineLayer.style.setProperty("--cm-minimap-line-gap", `${gap}px`);

            for (let lineNumber = 1; lineNumber <= lines.lines; lineNumber += 1) {
                const line = lines.line(lineNumber).text;
                const previewLine = document.createElement("div");
                previewLine.className = line.trim() ? "cm-minimap-line" : "cm-minimap-line cm-minimap-line-empty";
                previewLine.style.width = `${Math.min(100, Math.max(8, line.trimEnd().length * 1.8))}%`;
                fragment.append(previewLine);
            }

            this.lineLayer.append(fragment);
            this.updateViewport();
        }

        private updateViewport = () => {
            const scrollHeight = Math.max(this.view.scrollDOM.scrollHeight, 1);
            const clientHeight = Math.max(this.view.scrollDOM.clientHeight, 1);
            const minimapHeight = Math.max(this.dom.clientHeight, 1);
            const viewportHeight = Math.max(18, (clientHeight / scrollHeight) * minimapHeight);
            const maxTop = Math.max(0, minimapHeight - viewportHeight);
            const maxScrollTop = Math.max(1, scrollHeight - clientHeight);
            const top = Math.min(maxTop, (this.view.scrollDOM.scrollTop / maxScrollTop) * maxTop);

            this.viewportMarker.style.height = `${Math.min(minimapHeight, viewportHeight)}px`;
            this.viewportMarker.style.transform = `translateY(${top}px)`;
        };

        private handlePointerDown = (event: PointerEvent) => {
            event.preventDefault();
            this.dragging = true;
            this.scrollToPointer(event);
            window.addEventListener("pointermove", this.handlePointerMove);
            window.addEventListener("pointerup", this.handlePointerUp, { once: true });
        };

        private handlePointerMove = (event: PointerEvent) => {
            if (!this.dragging) return;
            event.preventDefault();
            this.scrollToPointer(event);
        };

        private handlePointerUp = () => {
            this.dragging = false;
            window.removeEventListener("pointermove", this.handlePointerMove);
        };

        private scrollToPointer(event: PointerEvent) {
            const rect = this.dom.getBoundingClientRect();
            const ratio = Math.min(1, Math.max(0, (event.clientY - rect.top) / Math.max(rect.height, 1)));
            const scrollableHeight = this.view.scrollDOM.scrollHeight - this.view.scrollDOM.clientHeight;
            this.view.scrollDOM.scrollTop = ratio * Math.max(0, scrollableHeight);
        }
    }
);

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

type EditorTab = "zenith-c" | "assembly" | "machine-code";

const EDITOR_TABS: Array<{ id: EditorTab; label: string }> = [
    { id: "zenith-c", label: "Zenith C" },
    { id: "assembly", label: "Zenith Assembly" },
    { id: "machine-code", label: "Machine Code" },
];

const EDITOR_TAB_META = {
    "zenith-c": {
        title: "Zenith C",
        description: "Compile Zenith C into assembly, machine code, and the emulator.",
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

type AssembledProgram = {
    machineCode: string;
    data: Array<number>;
};

type ChartPoint = {
    x: number;
    y: number;
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

const SYSTEM_INCLUDE_BASE_URL = "zenith-libc/";
const systemIncludeCache = new Map<string, Promise<string>>();

function systemIncludeUrl(includePath: string) {
    return `${SYSTEM_INCLUDE_BASE_URL}${includePath.split("/").map(encodeURIComponent).join("/")}`;
}

function validateSystemIncludePath(includePath: string) {
    if (!/^[A-Za-z0-9_./-]+\.h$/.test(includePath) || includePath.startsWith("/") || includePath.includes("..")) {
        throw new Error(`#include <${includePath}> is not a valid Zenith libc header path`);
    }
}

function fetchSystemInclude(includePath: string) {
    validateSystemIncludePath(includePath);

    const cached = systemIncludeCache.get(includePath);
    if (cached) {
        return cached;
    }

    const request = fetch(systemIncludeUrl(includePath)).then(async (response) => {
        if (!response.ok) {
            throw new Error(`failed to load #include <${includePath}> from ${systemIncludeUrl(includePath)}`);
        }
        if (response.headers.get("content-type")?.includes("text/html")) {
            throw new Error(`#include <${includePath}> was not found in ${SYSTEM_INCLUDE_BASE_URL}`);
        }

        return response.text();
    });
    systemIncludeCache.set(includePath, request);
    return request;
}

async function preprocessZenithCSystemIncludes(source: string) {
    const includePattern = /^([ \t]*)#[ \t]*include[ \t]*<([^>\r\n]+)>[ \t]*(?:(?:\/\/.*)|(?:\/\*.*\*\/)[ \t]*)?$/gm;
    let output = "";
    let lastIndex = 0;
    let match: RegExpExecArray | null = includePattern.exec(source);

    while (match) {
        const indentation = match[1] ?? "";
        const includePath = (match[2] ?? "").trim();
        const includedSource = await fetchSystemInclude(includePath);

        output += source.slice(lastIndex, match.index);
        output += `${indentation}/* begin #include <${includePath}> */\n`;
        output += includedSource;
        if (!includedSource.endsWith("\n")) {
            output += "\n";
        }
        output += `${indentation}/* end #include <${includePath}> */`;
        lastIndex = includePattern.lastIndex;

        match = includePattern.exec(source);
    }

    return output + source.slice(lastIndex);
}

function appendInstructionFrameSample(history: Array<number>, sample: number) {
    return [...history.slice(-(INSTRUCTION_FRAME_HISTORY_LIMIT - 1)), sample];
}

function assembleProgram(
    assembler: NonNullable<ReturnType<typeof useZenithAssembler>>,
    source: string
): AssembledProgram {
    const maybeProgramAssembler = assembler as typeof assembler & {
        assembleProgram?: (source: string) => AssembledProgram;
    };

    if (typeof maybeProgramAssembler.assembleProgram === "function") {
        return maybeProgramAssembler.assembleProgram(source);
    }

    return {
        machineCode: assembler.assemble(source),
        data: [],
    };
}

function createInstructionChartPoints(samples: Array<number>): Array<ChartPoint> {
    if (samples.length === 0) return [];

    const maxSample = Math.max(1, ...samples);
    const plotWidth = INSTRUCTION_CHART_WIDTH - INSTRUCTION_CHART_PADDING * 2;
    const plotHeight = INSTRUCTION_CHART_HEIGHT - INSTRUCTION_CHART_PADDING * 2;

    return samples.map((sample, index) => {
        const x =
            samples.length === 1
                ? INSTRUCTION_CHART_WIDTH - INSTRUCTION_CHART_PADDING
                : INSTRUCTION_CHART_PADDING + (index * plotWidth) / (samples.length - 1);
        const y = INSTRUCTION_CHART_PADDING + (1 - sample / maxSample) * plotHeight;

        return { x, y };
    });
}

function createInstructionChartPath(points: Array<ChartPoint>) {
    return points.map(({ x, y }, index) => `${index === 0 ? "M" : "L"} ${x.toFixed(2)} ${y.toFixed(2)}`).join(" ");
}

function InstructionFrameChart({ samples, isRunning }: { samples: Array<number>; isRunning: boolean }) {
    const chart = useMemo(() => {
        const points = createInstructionChartPoints(samples);

        return {
            path: createInstructionChartPath(points),
            points,
        };
    }, [samples]);
    const latest = samples.at(-1) ?? 0;
    const peak = samples.length > 0 ? Math.max(...samples) : 0;
    const trough = samples.length > 0 ? Math.min(...samples) : 0;
    const average =
        samples.length > 0 ? Math.round(samples.reduce((total, sample) => total + sample, 0) / samples.length) : 0;

    return (
        <section className="mt-4 rounded-md border bg-background">
            <div className="flex items-center justify-between border-b px-3 py-2">
                <h3 className="text-xs font-semibold tracking-wide text-muted-foreground uppercase">
                    Emulator instructions/frame
                </h3>
                <span className="font-mono text-[11px] text-muted-foreground">{isRunning ? "live" : "paused"}</span>
            </div>
            <div className="p-3">
                <div className="relative h-20 rounded-md border bg-muted/20 p-2">
                    <svg
                        aria-label={`Instructions executed per frame. Latest ${latest}, peak ${peak}, trough ${trough}, average ${average}.`}
                        className="h-full w-full"
                        preserveAspectRatio="none"
                        role="img"
                        viewBox={`0 0 ${INSTRUCTION_CHART_WIDTH} ${INSTRUCTION_CHART_HEIGHT}`}
                    >
                        <line
                            stroke="var(--border)"
                            strokeWidth="1"
                            vectorEffect="non-scaling-stroke"
                            x1={INSTRUCTION_CHART_PADDING}
                            x2={INSTRUCTION_CHART_WIDTH - INSTRUCTION_CHART_PADDING}
                            y1={INSTRUCTION_CHART_PADDING}
                            y2={INSTRUCTION_CHART_PADDING}
                        />
                        <line
                            stroke="var(--border)"
                            strokeWidth="1"
                            vectorEffect="non-scaling-stroke"
                            x1={INSTRUCTION_CHART_PADDING}
                            x2={INSTRUCTION_CHART_WIDTH - INSTRUCTION_CHART_PADDING}
                            y1={INSTRUCTION_CHART_HEIGHT - INSTRUCTION_CHART_PADDING}
                            y2={INSTRUCTION_CHART_HEIGHT - INSTRUCTION_CHART_PADDING}
                        />
                        {chart.path ? (
                            <path
                                d={chart.path}
                                fill="none"
                                stroke="var(--chart-2)"
                                strokeLinecap="round"
                                strokeLinejoin="round"
                                strokeWidth="2"
                                vectorEffect="non-scaling-stroke"
                            />
                        ) : null}
                        {chart.points.length === 1 ? (
                            <circle cx={chart.points[0].x} cy={chart.points[0].y} fill="var(--chart-2)" r="2.5" />
                        ) : null}
                    </svg>
                    {samples.length === 0 ? (
                        <div className="absolute inset-0 flex items-center justify-center font-mono text-[11px] text-muted-foreground">
                            No frame samples
                        </div>
                    ) : null}
                </div>
                <div className="mt-2 grid grid-cols-4 gap-2 font-mono text-[11px]">
                    <div className="min-w-0">
                        <span className="text-muted-foreground">latest</span>
                        <span className="block truncate text-foreground">{latest.toLocaleString()}</span>
                    </div>
                    <div className="min-w-0">
                        <span className="text-muted-foreground">avg</span>
                        <span className="block truncate text-foreground">{average.toLocaleString()}</span>
                    </div>
                    <div className="min-w-0">
                        <span className="text-muted-foreground">peak</span>
                        <span className="block truncate text-foreground">{peak.toLocaleString()}</span>
                    </div>
                    <div className="min-w-0">
                        <span className="text-muted-foreground">trough</span>
                        <span className="block truncate text-foreground">{trough.toLocaleString()}</span>
                    </div>
                </div>
            </div>
        </section>
    );
}

export function App() {
    const assembler = useZenithAssembler();
    const emulator = useZenithEmulator();
    const compiler = useZenithCompiler();
    const [activeTab, setActiveTab] = useState<EditorTab>("zenith-c");
    const [zenithCSource, setZenithCSource] = useState(zenithKernel);
    const [assemblySource, setAssemblySource] = useState("");
    const [machineCode, setMachineCode] = useState("");
    const [compilerError, setCompilerError] = useState<string | null>(null);
    const [assemblyError, setAssemblyError] = useState<string | null>(null);
    const [assemblyIsLoaded, setAssemblyIsLoaded] = useState(false);
    const [loadedData, setLoadedData] = useState<Array<number>>([]);
    const [registers, setRegisters] = useState<Array<bigint>>(createZeroRegisters);
    const [instructionIndex, setInstructionIndex] = useState(0);
    const [isRunning, setIsRunning] = useState(false);
    const [lastMessage, setLastMessage] = useState("");
    const framebufferCanvasRef = useRef<HTMLCanvasElement | null>(null);
    const fullscreenFramebufferCanvasRef = useRef<HTMLCanvasElement | null>(null);
    const [framebufferRevision, setFramebufferRevision] = useState(0);
    const [fullscreenFramebufferIsOpen, setFullscreenFramebufferIsOpen] = useState(false);
    const [instructionsPerFrame, setInstructionsPerFrame] = useState<Array<number>>([]);

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
                    position: "relative",
                    height: "100%",
                    backgroundColor: "transparent",
                    color: "var(--foreground)",
                    fontSize: "13px",
                },
                "&.cm-editor": {
                    minHeight: 0,
                },
                ".cm-scroller": {
                    fontFamily: "ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace",
                    height: "100%",
                    overflow: "auto",
                    paddingRight: `${EDITOR_MINIMAP_WIDTH}px`,
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
                ".cm-minimap": {
                    position: "absolute",
                    insetBlock: 0,
                    right: 0,
                    zIndex: 5,
                    width: `${EDITOR_MINIMAP_WIDTH}px`,
                    borderLeft: "1px solid var(--border)",
                    backgroundColor: "color-mix(in oklch, var(--background) 88%, var(--muted))",
                    cursor: "pointer",
                    overflow: "hidden",
                    userSelect: "none",
                },
                ".cm-minimap-lines": {
                    boxSizing: "border-box",
                    display: "flex",
                    flexDirection: "column",
                    gap: "var(--cm-minimap-line-gap)",
                    height: "100%",
                    minHeight: "100%",
                    padding: "6px 6px",
                    pointerEvents: "none",
                },
                ".cm-minimap-line": {
                    flex: "0 0 var(--cm-minimap-line-height)",
                    maxWidth: "100%",
                    borderRadius: "999px",
                    backgroundColor: "color-mix(in oklch, var(--foreground) 28%, transparent)",
                },
                ".cm-minimap-line-empty": {
                    opacity: 0,
                },
                ".cm-minimap-viewport": {
                    position: "absolute",
                    insetInline: "4px",
                    top: 0,
                    border: "1px solid color-mix(in oklch, var(--primary) 45%, transparent)",
                    borderRadius: "4px",
                    backgroundColor: "color-mix(in oklch, var(--primary) 14%, transparent)",
                    boxShadow: "0 0 0 1px color-mix(in oklch, var(--background) 70%, transparent)",
                    pointerEvents: "none",
                },
            }),
            editorMinimap,
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

    const assemblyEditorExtensions = useMemo(() => [...baseEditorExtensions, assemblyLanguage], [baseEditorExtensions]);

    const cEditorExtensions = useMemo(() => [...baseEditorExtensions, cLikeLanguage], [baseEditorExtensions]);

    useEffect(() => {
        const canvas = framebufferCanvasRef.current;
        if (!canvas) return;

        paintFramebufferPreview(canvas, emulator?.getFramebuffer() ?? null);
    }, [emulator, framebufferRevision]);

    useEffect(() => {
        if (!fullscreenFramebufferIsOpen) return;

        const canvas = fullscreenFramebufferCanvasRef.current;
        if (!canvas) return;

        paintFramebufferFullSize(canvas, emulator?.getFramebuffer() ?? null);
    }, [emulator, framebufferRevision, fullscreenFramebufferIsOpen]);

    useEffect(() => {
        if (!fullscreenFramebufferIsOpen) return;

        const closeOnEscape = (event: KeyboardEvent) => {
            if (event.key === "Escape") {
                setFullscreenFramebufferIsOpen(false);
            }
        };

        window.addEventListener("keydown", closeOnEscape);
        return () => window.removeEventListener("keydown", closeOnEscape);
    }, [fullscreenFramebufferIsOpen]);

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
            setInstructionsPerFrame((history) => appendInstructionFrameSample(history, executed));

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
        if (emulator && loadedData.length > 0 && !emulator.loadData(loadedData)) {
            setLastMessage("Program data is too large to load without overlapping the stack.");
            return;
        }
        setRegisters(emulator?.getRegisters() ?? createZeroRegisters());
        setInstructionIndex(0);
        setFramebufferRevision((revision) => revision + 1);
        setInstructionsPerFrame([]);
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

    function loadMachineCode(program: AssembledProgram, shouldRun: boolean) {
        const compiled = program.machineCode;
        const compiledParseResult = parseProgram(compiled);
        setIsRunning(false);
        emulator?.reset();
        if (emulator && program.data.length > 0 && !emulator.loadData(program.data)) {
            throw new Error("program data is too large to load without overlapping the stack");
        }
        setRegisters(emulator?.getRegisters() ?? createZeroRegisters());
        setInstructionIndex(0);
        setMachineCode(compiled);
        setLoadedData(program.data);
        setFramebufferRevision((revision) => revision + 1);
        setInstructionsPerFrame([]);

        if (
            shouldRun &&
            emulator &&
            compiledParseResult.errors.length === 0 &&
            compiledParseResult.instructions.length > 0
        ) {
            setIsRunning(true);
        }

        return compiledParseResult;
    }

    function compileAndLoadAssembly() {
        if (!assembler) {
            setLastMessage("Assembler is still loading.");
            return;
        }

        setCompilerError(null);

        try {
            const compiled = assembleProgram(assembler, assemblySource);
            const compiledParseResult = loadMachineCode(compiled, false);
            setAssemblyError(null);
            setAssemblyIsLoaded(true);
            setActiveTab("machine-code");
            setLastMessage(
                `Compiled and loaded ${compiledParseResult.instructions.length} machine-code instruction${
                    compiledParseResult.instructions.length === 1 ? "" : "s"
                }.`
            );
        } catch (error) {
            const message = unknownErrorMessage(error);
            setCompilerError(null);
            setAssemblyError(message);
            setAssemblyIsLoaded(false);
            setIsRunning(false);
            setLastMessage(`Assembly compile failed: ${message}`);
        }
    }

    async function compileAndRunZenithC(source: string, showMachineCode = false) {
        setZenithCSource(source);
        setIsRunning(false);

        if (!compiler || !assembler) {
            setLastMessage("Compiler and assembler are still loading.");
            return;
        }

        try {
            const preprocessedSource = await preprocessZenithCSystemIncludes(source);
            const compiledAssembly = compiler.compile(preprocessedSource);
            setAssemblySource(compiledAssembly);
            const compiledMachineCode = assembleProgram(assembler, compiledAssembly);
            const compiledParseResult = loadMachineCode(compiledMachineCode, true);
            setCompilerError(null);
            setAssemblyError(null);
            setAssemblyIsLoaded(true);
            if (showMachineCode) {
                setActiveTab("machine-code");
            }
            setLastMessage(
                `Compiled Zenith C through ${compiledParseResult.instructions.length} machine-code instruction${
                    compiledParseResult.instructions.length === 1 ? "" : "s"
                }${emulator ? " and started the emulator." : "; emulator is still loading."}`
            );
        } catch (error) {
            const message = unknownErrorMessage(error);
            setCompilerError(message);
            setAssemblyError(null);
            setAssemblyIsLoaded(false);
            setIsRunning(false);
            setLastMessage(`Zenith C pipeline failed: ${message}`);
        }
    }

    function selectTab(tab: EditorTab) {
        if (tab === "zenith-c") {
            setIsRunning(false);
        }

        setActiveTab(tab);
    }

    return (
        <>
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
                    <Tabs
                        className="flex min-h-0 flex-col gap-0"
                        onValueChange={(value) => selectTab(value as EditorTab)}
                        value={activeTab}
                    >
                        <div className="flex flex-wrap items-center justify-between gap-3 border-b px-4 py-2">
                            <div className="min-w-0">
                                <TabsList className="mb-2">
                                    {EDITOR_TABS.map((tab) => (
                                        <TabsTrigger key={tab.id} value={tab.id}>
                                            {tab.label}
                                        </TabsTrigger>
                                    ))}
                                </TabsList>
                                <h2 className="text-sm font-semibold">{activeTabMeta.title}</h2>
                                <p className="text-xs text-muted-foreground">{activeTabMeta.description}</p>
                            </div>
                            <div className="flex items-center gap-2">
                                {activeTab === "zenith-c" ? (
                                    <Button
                                        disabled={!compiler || !assembler}
                                        onClick={() => void compileAndRunZenithC(zenithCSource, true)}
                                        title="Compile and load Zenith C"
                                        variant="outline"
                                    >
                                        <Hammer />
                                        Compile & Load
                                    </Button>
                                ) : null}
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
                        </div>
                        <TabsContent className="m-0 min-h-0 overflow-hidden" value="zenith-c">
                            <CodeMirror
                                basicSetup={{
                                    autocompletion: false,
                                    bracketMatching: false,
                                    foldGutter: false,
                                    highlightSelectionMatches: false,
                                }}
                                className="h-full min-h-0 [&_.cm-editor]:h-full [&_.cm-scroller]:overflow-auto"
                                extensions={cEditorExtensions}
                                height="100%"
                                onChange={(value) => {
                                    setZenithCSource(value);
                                    setCompilerError(null);
                                    setAssemblyError(null);
                                    setAssemblyIsLoaded(false);
                                    setIsRunning(false);
                                    setInstructionsPerFrame([]);
                                    setLastMessage("Zenith C changed. Compile and load before running those changes.");
                                }}
                                value={zenithCSource}
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
                                className="h-full min-h-0 [&_.cm-editor]:h-full [&_.cm-scroller]:overflow-auto"
                                extensions={assemblyEditorExtensions}
                                height="100%"
                                onChange={(value) => {
                                    setAssemblySource(value);
                                    setCompilerError(null);
                                    setAssemblyError(null);
                                    setAssemblyIsLoaded(false);
                                    setIsRunning(false);
                                    setInstructionsPerFrame([]);
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
                                className="h-full min-h-0 [&_.cm-editor]:h-full [&_.cm-scroller]:overflow-auto"
                                extensions={machineCodeEditorExtensions}
                                height="100%"
                                onChange={(value) => {
                                    setIsRunning(false);
                                    setMachineCode(value);
                                    setCompilerError(null);
                                    setAssemblyError(null);
                                    setAssemblyIsLoaded(false);
                                    setInstructionIndex(0);
                                    setInstructionsPerFrame([]);
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
                            {activeTab === "zenith-c" && !compiler ? <p>Loading compiler...</p> : null}
                            {activeTab === "zenith-c" && !assembler ? <p>Loading assembler...</p> : null}
                            {activeTab === "assembly" && !assembler ? <p>Loading assembler...</p> : null}
                            {compilerError ? (
                                <div className="text-destructive">
                                    <p>{compilerError}</p>
                                </div>
                            ) : assemblyError ? (
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
                                        <p>
                                            {assemblyIsLoaded ? "assembly output is loaded" : "assembly is not loaded"}
                                        </p>
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
                            <div className="flex items-center justify-between gap-2 border-b px-3 py-2">
                                <h3 className="text-xs font-semibold tracking-wide text-muted-foreground uppercase">
                                    Framebuffer
                                </h3>
                                <div className="flex items-center gap-2">
                                    <span className="font-mono text-[11px] text-muted-foreground">
                                        {FRAMEBUFFER_WIDTH}x{FRAMEBUFFER_HEIGHT}
                                    </span>
                                    <Button
                                        aria-label="Open fullscreen framebuffer"
                                        disabled={!emulator}
                                        onClick={() => setFullscreenFramebufferIsOpen(true)}
                                        size="icon-xs"
                                        title="Open fullscreen framebuffer"
                                        variant="ghost"
                                    >
                                        <Maximize2 />
                                    </Button>
                                </div>
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
                        <InstructionFrameChart isRunning={isRunning} samples={instructionsPerFrame} />
                    </div>
                </aside>
            </main>
            {fullscreenFramebufferIsOpen ? (
                <div
                    aria-labelledby="fullscreen-framebuffer-title"
                    aria-modal="true"
                    className="fixed inset-0 z-50 grid grid-rows-[auto_minmax(0,1fr)] bg-background/95 text-foreground backdrop-blur-sm"
                    onClick={() => setFullscreenFramebufferIsOpen(false)}
                    role="dialog"
                >
                    <header
                        className="flex items-center justify-between gap-3 border-b bg-background px-4 py-3"
                        onClick={(event) => event.stopPropagation()}
                    >
                        <div className="min-w-0">
                            <h2 className="text-sm font-semibold" id="fullscreen-framebuffer-title">
                                Framebuffer
                            </h2>
                            <p className="font-mono text-xs text-muted-foreground">
                                {FRAMEBUFFER_WIDTH}x{FRAMEBUFFER_HEIGHT} RGB888
                            </p>
                        </div>
                        <Button
                            aria-label="Close fullscreen framebuffer"
                            onClick={() => setFullscreenFramebufferIsOpen(false)}
                            size="icon"
                            title="Close fullscreen framebuffer"
                            variant="outline"
                        >
                            <X />
                        </Button>
                    </header>
                    <div
                        className="grid min-h-0 place-items-center overflow-auto bg-black p-4"
                        onClick={(event) => event.stopPropagation()}
                    >
                        <canvas
                            aria-label="Fullscreen framebuffer"
                            className="max-h-full max-w-full border border-border bg-black [image-rendering:pixelated]"
                            height={FRAMEBUFFER_HEIGHT}
                            ref={fullscreenFramebufferCanvasRef}
                            width={FRAMEBUFFER_WIDTH}
                        />
                    </div>
                </div>
            ) : null}
        </>
    );
}

export default App;
