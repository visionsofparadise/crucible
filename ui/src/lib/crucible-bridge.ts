import { useEffect, useRef, useState } from "react";
import type {
	CrucibleCompileResult,
	CrucibleInputError,
	CrucibleMeterFrame,
	CrucibleSchemaRoot,
	CrucibleSourceChanged,
	JuceFrontend,
	JuceSliderState,
	JuceToggleState,
} from "./juce";

type CompileResultListener = (result: CrucibleCompileResult | null) => void;
type SourceChangedListener = (source: CrucibleSourceChanged) => void;
type InputErrorListener = (errors: Array<CrucibleInputError>) => void;
type MeterFrameListener = (frame: CrucibleMeterFrame) => void;

const compileResultListeners = new Set<CompileResultListener>();
const sourceChangedListeners = new Set<SourceChangedListener>();
const inputErrorListeners = new Set<InputErrorListener>();
const meterFrameListeners = new Set<MeterFrameListener>();

type ClipboardPasteListener = (text: string) => void;

const clipboardPasteListeners = new Set<ClipboardPasteListener>();

let latestCompileResult: CrucibleCompileResult | null = null;
let latestSourceChanged: CrucibleSourceChanged | null = null;
let activeInputErrors: Array<CrucibleInputError> = [];
const latestMeterFrame: CrucibleMeterFrame = {
	inL_rms: 0,
	inR_rms: 0,
	inL_peak: 0,
	inR_peak: 0,
	outL_rms: 0,
	outR_rms: 0,
	outL_peak: 0,
	outR_peak: 0,
	bargraphs: [],
};

export const isJuceHost = (): boolean => typeof window !== "undefined" && window.__JUCE__ !== undefined;

const detectVariant = (): "fx" | "instrument" => {
	if (typeof window === "undefined") return "fx";

	const injected = (window as unknown as { __crucibleVariant?: string }).__crucibleVariant;

	if (injected === "instrument") return "instrument";

	if (injected === "fx") return "fx";

	if (window.crucible?.variant === "instrument") return "instrument";

	if (window.crucible?.variant === "fx") return "fx";

	const search = new URLSearchParams(window.location.search);

	return search.get("variant") === "instrument" ? "instrument" : "fx";
};

const buildDevCompileResult = (): CrucibleCompileResult => ({
	success: true,
	crossLoad: false,
	errorMessage: "",
	numInputs: 2,
	numOutputs: 2,
	latencySamples: 0,
	svgRoot: "",
	schema: {
		name: "dev_demo",
		inputs: 2,
		outputs: 2,
		ui: {
			type: "vgroup",
			label: "DEMO",
			items: [
				{
					type: "vgroup",
					label: "FILTER",
					items: [
						{
							type: "hslider",
							label: "CUTOFF",
							address: "/dev_demo/filter/cutoff",
							slotId: "slot.f.001",
							init: 1200,
							min: 20,
							max: 20000,
							step: 1,
							meta: { unit: "Hz", scale: "log" },
						},
						{
							type: "hslider",
							label: "RES",
							address: "/dev_demo/filter/res",
							slotId: "slot.f.002",
							init: 0.5,
							min: 0,
							max: 1,
							step: 0.01,
							meta: {},
						},
						{
							type: "nentry",
							label: "POLES",
							address: "/dev_demo/filter/poles",
							slotId: "slot.f.003",
							init: 2,
							min: 1,
							max: 4,
							step: 1,
							meta: {},
						},
						{
							type: "hslider",
							label: "TYPE",
							address: "/dev_demo/filter/type",
							slotId: "slot.f.004",
							init: 0,
							min: 0,
							max: 3,
							step: 1,
							meta: { style: "radio{LP:0;HP:1;BP:2;NT:3}" },
						},
						{
							type: "hslider",
							label: "ALGO",
							address: "/dev_demo/filter/algo",
							slotId: "slot.f.005",
							init: 0,
							min: 0,
							max: 3,
							step: 1,
							meta: { style: "menu{FM:0;AM:1;PM:2;PD:3}" },
						},
						{
							type: "checkbox",
							label: "OS",
							address: "/dev_demo/filter/os",
							slotId: "slot.b.001",
							init: 0,
							min: 0,
							max: 1,
							step: 1,
							meta: {},
						},
						{
							type: "button",
							label: "RESET",
							address: "/dev_demo/filter/reset",
							slotId: "slot.b.002",
							init: 0,
							min: 0,
							max: 1,
							step: 1,
							meta: {},
						},
					],
				},
				{
					type: "vgroup",
					label: "ANALYSIS",
					items: [
						{
							type: "hbargraph",
							label: "RMS",
							address: "/dev_demo/analysis/rms",
							slotId: "slot.m.000",
							init: 0,
							min: 0,
							max: 1,
							step: 0,
							meta: {},
						},
						{
							type: "vbargraph",
							label: "PEAK",
							address: "/dev_demo/analysis/peak",
							slotId: "slot.m.001",
							init: 0,
							min: 0,
							max: 1,
							step: 0,
							meta: {},
						},
					],
				},
			],
		},
	},
});

const DEV_SOURCE = `import("stdfaust.lib");

cutoff = hslider("filter/cutoff [unit:Hz][scale:log]", 1200, 20, 20000, 1) : si.smoo;
res    = hslider("filter/res", 0.5, 0, 1, 0.01) : si.smoo;

process = fi.resonlp(cutoff, res * 10 + 0.5, 1);
`;

let devLoopStarted = false;
let devStartedAtMs = 0;

const startDevLoop = (): void => {
	if (devLoopStarted) return;

	devLoopStarted = true;
	devStartedAtMs = performance.now();

	queueMicrotask(() => {
		const compileResult = buildDevCompileResult();

		latestCompileResult = compileResult;
		latestSourceChanged = { source: DEV_SOURCE, path: "<dev>" };
		activeInputErrors = [];

		if (compileResult.schema) seedDevSlotsFromSchema(compileResult.schema);

		for (const listener of compileResultListeners) listener(compileResult);

		const sourceSnapshot = latestSourceChanged;

		for (const listener of sourceChangedListeners) listener(sourceSnapshot);

		for (const listener of inputErrorListeners) listener(activeInputErrors);
	});

	const tick = (): void => {
		const tSec = (performance.now() - devStartedAtMs) / 1000;
		const value01 = (theta: number): number => 0.5 * (Math.sin(theta) + 1);
		const peak1 = value01(tSec * 1.7);
		const peak2 = value01(tSec * 1.9 + 1.2);
		const peak3 = value01(tSec * 2.3 + 2.4);
		const peak4 = value01(tSec * 2.1 + 3.6);

		latestMeterFrame.inL_rms = peak1 * 0.6;
		latestMeterFrame.inR_rms = peak2 * 0.6;
		latestMeterFrame.inL_peak = peak1;
		latestMeterFrame.inR_peak = peak2;
		latestMeterFrame.outL_rms = peak3 * 0.6;
		latestMeterFrame.outR_rms = peak4 * 0.6;
		latestMeterFrame.outL_peak = peak3;
		latestMeterFrame.outR_peak = peak4;
		latestMeterFrame.bargraphs = [value01(tSec * 2.7), value01(tSec * 3.1 + 0.8)];

		for (const listener of meterFrameListeners) listener(latestMeterFrame);
	};

	setInterval(tick, 33);
};

const devFloatSlots = new Map<string, number>();
const devBoolSlots = new Map<string, boolean>();

type DevSlotListener = () => void;

const devSlotListeners = new Map<string, Set<DevSlotListener>>();

const seedDevSlotsFromSchema = (schema: CrucibleSchemaRoot): void => {
	const walk = (group: { items: Array<unknown> }): void => {
		for (const item of group.items) {
			const node = item as { type: string; slotId?: string; init?: number; min?: number; max?: number };

			if (node.type === "vgroup" || node.type === "hgroup" || node.type === "tgroup") {
				walk(node as unknown as { items: Array<unknown> });

				continue;
			}

			if (!node.slotId) continue;

			if (node.type === "hslider" || node.type === "vslider" || node.type === "nentry") {
				const range = (node.max ?? 1) - (node.min ?? 0);
				const normalised = range > 0 ? ((node.init ?? 0) - (node.min ?? 0)) / range : 0;

				devFloatSlots.set(node.slotId, Math.max(0, Math.min(1, normalised)));
			} else if (node.type === "button" || node.type === "checkbox") {
				devBoolSlots.set(node.slotId, (node.init ?? 0) > 0.5);
			} else if (node.type === "hbargraph" || node.type === "vbargraph") {
				devFloatSlots.set(node.slotId, 0);
			}
		}
	};

	walk(schema.ui);
};

let installedHostCallbacks = false;

const installHostCallbacks = (): void => {
	if (installedHostCallbacks || typeof window === "undefined") return;

	window.crucible ??= {};

	const api = window.crucible;

	api.onCompileResult = (result: CrucibleCompileResult): void => {
		latestCompileResult = result;

		for (const listener of compileResultListeners) listener(result);
	};

	api.onSourceChanged = (event: CrucibleSourceChanged): void => {
		latestSourceChanged = event;

		for (const listener of sourceChangedListeners) listener(event);
	};

	api.onInputError = (error: CrucibleInputError): void => {
		activeInputErrors = [...activeInputErrors.filter((existing) => existing.kind !== error.kind), error];

		for (const listener of inputErrorListeners) listener(activeInputErrors);
	};

	api.onClipboardPaste = (text: string): void => {
		for (const listener of clipboardPasteListeners) listener(text);
	};

	api.onMeterFrame = (frame: CrucibleMeterFrame): void => {
		latestMeterFrame.inL_rms = frame.inL_rms;
		latestMeterFrame.inR_rms = frame.inR_rms;
		latestMeterFrame.inL_peak = frame.inL_peak;
		latestMeterFrame.inR_peak = frame.inR_peak;
		latestMeterFrame.outL_rms = frame.outL_rms;
		latestMeterFrame.outR_rms = frame.outR_rms;
		latestMeterFrame.outL_peak = frame.outL_peak;
		latestMeterFrame.outR_peak = frame.outR_peak;
		latestMeterFrame.bargraphs = frame.bargraphs;

		for (const listener of meterFrameListeners) listener(latestMeterFrame);
	};

	installedHostCallbacks = true;
};

let uiClickListenerInstalled = false;
const installUiClickListener = (): void => {
	if (uiClickListenerInstalled || typeof window === "undefined") return;

	uiClickListenerInstalled = true;

	let lastEmitMs = 0;
	const handler = (event: Event): void => {
		const backend = window.__JUCE__?.backend;

		if (!backend) return;

		const target = event.target;

		if (
			target instanceof Element &&
			target.closest('.cm-editor, input, textarea, select, [contenteditable=""], [contenteditable="true"]') !== null
		) {
			return;
		}

		const now = event.timeStamp;

		if (now - lastEmitMs < 50) return;

		lastEmitMs = now;
		backend.emitEvent("crucible.uiClicked", {});
	};

	window.addEventListener("pointerdown", handler, { capture: true });
	window.addEventListener("mousedown", handler, { capture: true });
};

const codeToVk: Record<string, number> = {
	KeyA: 0x41,
	KeyB: 0x42,
	KeyC: 0x43,
	KeyD: 0x44,
	KeyE: 0x45,
	KeyF: 0x46,
	KeyG: 0x47,
	KeyH: 0x48,
	KeyI: 0x49,
	KeyJ: 0x4a,
	KeyK: 0x4b,
	KeyL: 0x4c,
	KeyM: 0x4d,
	KeyN: 0x4e,
	KeyO: 0x4f,
	KeyP: 0x50,
	KeyQ: 0x51,
	KeyR: 0x52,
	KeyS: 0x53,
	KeyT: 0x54,
	KeyU: 0x55,
	KeyV: 0x56,
	KeyW: 0x57,
	KeyX: 0x58,
	KeyY: 0x59,
	KeyZ: 0x5a,
	Digit0: 0x30,
	Digit1: 0x31,
	Digit2: 0x32,
	Digit3: 0x33,
	Digit4: 0x34,
	Digit5: 0x35,
	Digit6: 0x36,
	Digit7: 0x37,
	Digit8: 0x38,
	Digit9: 0x39,
	F1: 0x70,
	F2: 0x71,
	F3: 0x72,
	F4: 0x73,
	F5: 0x74,
	F6: 0x75,
	F7: 0x76,
	F8: 0x77,
	F9: 0x78,
	F10: 0x79,
	F11: 0x7a,
	F12: 0x7b,
	ArrowLeft: 0x25,
	ArrowUp: 0x26,
	ArrowRight: 0x27,
	ArrowDown: 0x28,
	Space: 0x20,
	Enter: 0x0d,
	NumpadEnter: 0x0d,
	Escape: 0x1b,
	Tab: 0x09,
	Backspace: 0x08,
	Delete: 0x2e,
	Insert: 0x2d,
	Home: 0x24,
	End: 0x23,
	PageUp: 0x21,
	PageDown: 0x22,
	ShiftLeft: 0xa0,
	ShiftRight: 0xa1,
	ControlLeft: 0xa2,
	ControlRight: 0xa3,
	AltLeft: 0xa4,
	AltRight: 0xa5,
	MetaLeft: 0x5b,
	MetaRight: 0x5c,
	Minus: 0xbd,
	Equal: 0xbb,
	BracketLeft: 0xdb,
	BracketRight: 0xdd,
	Semicolon: 0xba,
	Quote: 0xde,
	Backquote: 0xc0,
	Comma: 0xbc,
	Period: 0xbe,
	Slash: 0xbf,
	Backslash: 0xdc,
	Numpad0: 0x60,
	Numpad1: 0x61,
	Numpad2: 0x62,
	Numpad3: 0x63,
	Numpad4: 0x64,
	Numpad5: 0x65,
	Numpad6: 0x66,
	Numpad7: 0x67,
	Numpad8: 0x68,
	Numpad9: 0x69,
	NumpadMultiply: 0x6a,
	NumpadAdd: 0x6b,
	NumpadSubtract: 0x6d,
	NumpadDecimal: 0x6e,
	NumpadDivide: 0x6f,
};

const extendedKeys = new Set<string>([
	"ArrowLeft",
	"ArrowUp",
	"ArrowRight",
	"ArrowDown",
	"Insert",
	"Delete",
	"Home",
	"End",
	"PageUp",
	"PageDown",
	"NumpadEnter",
	"NumpadDivide",
	"ShiftRight",
	"ControlRight",
	"AltRight",
	"MetaRight",
]);

const isInEditor = (target: EventTarget | null): boolean => {
	if (!(target instanceof Element)) return false;

	if (target.closest("input, textarea, select")) return true;

	if (target.closest('[contenteditable=""], [contenteditable="true"]')) return true;

	if (target.closest(".cm-content")) return true;

	return false;
};

const isClipboardShortcut = (event: KeyboardEvent): boolean => {
	if (!(event.ctrlKey || event.metaKey)) return false;

	if (event.altKey) return false;

	const key = event.key.toLowerCase();

	return key === "c" || key === "v" || key === "x" || key === "a" || key === "z" || key === "y";
};

let keyForwarderInstalled = false;
const installKeyForwarder = (): void => {
	if (keyForwarderInstalled || typeof window === "undefined") return;

	keyForwarderInstalled = true;

	const handler = (event: KeyboardEvent): void => {
		const backend = window.__JUCE__?.backend;

		if (!backend) return;

		if (isInEditor(event.target ?? document.activeElement)) return;

		if (isClipboardShortcut(event)) return;

		const vk = codeToVk[event.code];

		if (vk === undefined || vk === 0) {
			return;
		}

		event.preventDefault();
		event.stopPropagation();

		backend.emitEvent("crucible.forwardKey", {
			vkCode: vk,
			type: event.type === "keyup" ? "up" : "down",
			repeat: event.repeat,
			alt: event.altKey,
			ctrl: event.ctrlKey,
			shift: event.shiftKey,
			meta: event.metaKey,
			extended: extendedKeys.has(event.code),
		});
	};

	window.addEventListener("keydown", handler, { capture: true });
	window.addEventListener("keyup", handler, { capture: true });
};

export const useCompileResult = (): CrucibleCompileResult | null => {
	const [result, setResult] = useState<CrucibleCompileResult | null>(latestCompileResult);

	useEffect(() => {
		const listener: CompileResultListener = (next) => {
			setResult(next);
		};

		compileResultListeners.add(listener);

		if (latestCompileResult !== null) setResult(latestCompileResult);

		return () => {
			compileResultListeners.delete(listener);
		};
	}, []);

	return result;
};

export const useSourceChanged = (): CrucibleSourceChanged | null => {
	const [event, setEvent] = useState<CrucibleSourceChanged | null>(latestSourceChanged);

	useEffect(() => {
		const listener: SourceChangedListener = (next) => {
			setEvent(next);
		};

		sourceChangedListeners.add(listener);

		if (latestSourceChanged !== null) setEvent(latestSourceChanged);

		return () => {
			sourceChangedListeners.delete(listener);
		};
	}, []);

	return event;
};

export const useInputErrors = (): Array<CrucibleInputError> => {
	const [errors, setErrors] = useState<Array<CrucibleInputError>>(activeInputErrors);

	useEffect(() => {
		const listener: InputErrorListener = (next) => {
			setErrors(next);
		};

		inputErrorListeners.add(listener);
		setErrors(activeInputErrors);

		const compileListener: CompileResultListener = (result) => {
			if (result?.success === true && activeInputErrors.length > 0) {
				activeInputErrors = [];

				for (const inner of inputErrorListeners) inner(activeInputErrors);
			}
		};

		compileResultListeners.add(compileListener);

		return () => {
			inputErrorListeners.delete(listener);
			compileResultListeners.delete(compileListener);
		};
	}, []);

	return errors;
};

export interface MeterFrameSubscription {
	frameRef: React.RefObject<CrucibleMeterFrame>;
}

export const useMeterFrame = (): MeterFrameSubscription => {
	const frameRef = useRef<CrucibleMeterFrame>(latestMeterFrame);

	useEffect(() => {
		const listener: MeterFrameListener = (frame) => {
			frameRef.current = frame;
		};

		meterFrameListeners.add(listener);

		return () => {
			meterFrameListeners.delete(listener);
		};
	}, []);

	return { frameRef };
};

export const useVariant = (): "fx" | "instrument" => {
	const [variant] = useState<"fx" | "instrument">(detectVariant);

	return variant;
};

let frontendPromise: Promise<JuceFrontend> | null = null;
const loadFrontend = async (): Promise<JuceFrontend> => {
	if (frontendPromise) return frontendPromise;

	frontendPromise = import("juce-framework-frontend");

	return frontendPromise;
};

const formatFloatSlotId = (index: number): string => `slot.f.${index.toString().padStart(3, "0")}`;
const formatBoolSlotId = (index: number): string => `slot.b.${index.toString().padStart(3, "0")}`;

const notifyDevSlotListeners = (slotId: string): void => {
	const listeners = devSlotListeners.get(slotId);

	if (!listeners) return;

	for (const listener of listeners) listener();
};

const subscribeDevSlot = (slotId: string, listener: DevSlotListener): (() => void) => {
	let listeners = devSlotListeners.get(slotId);

	if (!listeners) {
		listeners = new Set();
		devSlotListeners.set(slotId, listeners);
	}

	listeners.add(listener);

	return () => {
		const ls = devSlotListeners.get(slotId);

		if (!ls) return;

		ls.delete(listener);
	};
};

export interface FloatSlotBinding {
	value: number;
	setValue: (next: number) => void;
}

export const useFloatSlot = (index: number): FloatSlotBinding => {
	const slotId = formatFloatSlotId(index);
	const inHost = isJuceHost();

	const [value, setValueLocal] = useState<number>(() => devFloatSlots.get(slotId) ?? 0);

	useEffect(() => {
		if (inHost) {
			let cancelled = false;
			let listenerId: number | null = null;
			let sliderState: JuceSliderState | null = null;

			void loadFrontend().then((frontend) => {
				if (cancelled) return;

				sliderState = frontend.getSliderState(slotId);
				setValueLocal(sliderState.getNormalisedValue());
				listenerId = sliderState.valueChangedEvent.addListener(() => {
					if (sliderState) setValueLocal(sliderState.getNormalisedValue());
				});
			});

			return () => {
				cancelled = true;

				if (sliderState && listenerId !== null) {
					sliderState.valueChangedEvent.removeListener(listenerId);
				}
			};
		}

		setValueLocal(devFloatSlots.get(slotId) ?? 0);

		return subscribeDevSlot(slotId, () => {
			setValueLocal(devFloatSlots.get(slotId) ?? 0);
		});
	}, [slotId, inHost]);

	const setValue = (next: number): void => {
		const clamped = Math.max(0, Math.min(1, next));

		setValueLocal(clamped);

		if (inHost) {
			void loadFrontend().then((frontend) => {
				const sliderState = frontend.getSliderState(slotId);

				sliderState.sliderDragStarted();
				sliderState.setNormalisedValue(clamped);
				sliderState.sliderDragEnded();
			});

			return;
		}

		devFloatSlots.set(slotId, clamped);
		notifyDevSlotListeners(slotId);
	};

	return { value, setValue };
};

export interface BoolSlotBinding {
	value: boolean;
	setValue: (next: boolean) => void;
}

export const useBoolSlot = (index: number): BoolSlotBinding => {
	const slotId = formatBoolSlotId(index);
	const inHost = isJuceHost();

	const [value, setValueLocal] = useState<boolean>(() => devBoolSlots.get(slotId) ?? false);

	useEffect(() => {
		if (inHost) {
			let cancelled = false;
			let listenerId: number | null = null;
			let toggleState: JuceToggleState | null = null;

			void loadFrontend().then((frontend) => {
				if (cancelled) return;

				toggleState = frontend.getToggleState(slotId);
				setValueLocal(toggleState.getValue());
				listenerId = toggleState.valueChangedEvent.addListener(() => {
					if (toggleState) setValueLocal(toggleState.getValue());
				});
			});

			return () => {
				cancelled = true;

				if (toggleState && listenerId !== null) {
					toggleState.valueChangedEvent.removeListener(listenerId);
				}
			};
		}

		setValueLocal(devBoolSlots.get(slotId) ?? false);

		return subscribeDevSlot(slotId, () => {
			setValueLocal(devBoolSlots.get(slotId) ?? false);
		});
	}, [slotId, inHost]);

	const setValue = (next: boolean): void => {
		setValueLocal(next);

		if (inHost) {
			void loadFrontend().then((frontend) => {
				frontend.getToggleState(slotId).setValue(next);
			});

			return;
		}

		devBoolSlots.set(slotId, next);
		notifyDevSlotListeners(slotId);
	};

	return { value, setValue };
};

export const readBargraphSlot = (frame: CrucibleMeterFrame, slotId: string): number => {
	const match = /^slot\.m\.(\d+)$/.exec(slotId);
	const digits = match?.[1];

	if (digits === undefined) return 0;

	const index = Number.parseInt(digits, 10);

	return frame.bargraphs[index] ?? 0;
};

const emitEvent = (eventId: string, payload: unknown): void => {
	if (typeof window === "undefined") return;

	const backend = window.__JUCE__?.backend;

	if (!backend) return;

	backend.emitEvent(eventId, payload);
};

export const requestOpen = (): void => {
	emitEvent("crucible.openRequest", {});
};

export const requestSaveAs = (buffer: string): void => {
	emitEvent("crucible.saveAsRequest", { buffer });
};

export const requestCopyContext = (): void => {
	emitEvent("crucible.copyContextRequest", {});
};

export const requestCopyToClipboard = (text: string): void => {
	emitEvent("crucible.copyToClipboard", { text });
};

export const requestPaste = (): void => {
	emitEvent("crucible.pasteRequest", {});
};

export const subscribeClipboardPaste = (listener: (text: string) => void): (() => void) => {
	clipboardPasteListeners.add(listener);

	return () => {
		clipboardPasteListeners.delete(listener);
	};
};

export const notifyEditorBufferChange = (buffer: string): void => {
	if (!isJuceHost()) {
		latestSourceChanged = { source: buffer, path: latestSourceChanged?.path ?? "<dev>" };
	}

	emitEvent("crucible.editorBufferChanged", { buffer });
};

export const requestCompileBuffer = (buffer: string): void => {
	emitEvent("crucible.compileBufferRequest", { buffer });
};

export const requestReplayState = (): void => {
	emitEvent("crucible.replayStateRequest", {});
};

export const notifyDiagramNavigate = (svgRel: string): void => {
	emitEvent("crucible.diagramNavigate", { path: svgRel });
};

export const notifyUiClicked = (): void => {
	emitEvent("crucible.uiClicked", {});
};

if (typeof window !== "undefined") {
	if (isJuceHost()) {
		installHostCallbacks();
		installUiClickListener();
		installKeyForwarder();
		requestReplayState();
	} else {
		startDevLoop();
	}
}
