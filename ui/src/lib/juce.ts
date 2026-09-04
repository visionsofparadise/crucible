// Type declarations for the JUCE 8 WebBrowserComponent message channel and
// the per-plugin `window.crucible` namespace populated by C++.
//
// Two surfaces:
//
// 1. `window.__JUCE__` — injected by JUCE before any user script runs.
//    Present iff the page is hosted inside a `WebBrowserComponent`. Its
//    presence is the runtime signal we use to pick between the real
//    bridge and the dev-mode fallback.
//
// 2. `window.crucible` — created by C++ at startup with the variant
//    (`"fx"` | `"instrument"`). C++ then assigns callback slots
//    (`onCompileResult`, `onSourceChanged`, `onInputError`,
//    `onMeterFrame`) via `evaluateJavascript`. The crucible-bridge
//    module installs the callbacks; consumers never touch
//    `window.crucible` directly.
//
// 3. `juce-framework-frontend` package — vendored copy of JUCE 8's
//    JS-side relay helpers. Exposes typed accessors to the
//    `WebSliderRelay` / `WebToggleButtonRelay` / `WebComboBoxRelay`
//    objects that C++ registers via `withOptionsFrom`. The package is
//    loaded dynamically only inside a JUCE host (its top-level script
//    accesses `window.__JUCE__` and would throw under `npm run start`).

export interface JuceListenerHandle {
	addListener: (callback: () => void) => number;
	removeListener: (id: number) => void;
}

export interface JuceSliderState {
	getNormalisedValue: () => number;
	setNormalisedValue: (next: number) => void;
	sliderDragStarted: () => void;
	sliderDragEnded: () => void;
	valueChangedEvent: JuceListenerHandle;
}

export interface JuceToggleState {
	getValue: () => boolean;
	setValue: (next: boolean) => void;
	valueChangedEvent: JuceListenerHandle;
}

export interface JuceComboBoxState {
	getChoiceIndex: () => number;
	setChoiceIndex: (next: number) => void;
	valueChangedEvent: JuceListenerHandle;
}

export interface JuceFrontend {
	getSliderState: (name: string) => JuceSliderState;
	getToggleState: (name: string) => JuceToggleState;
	getComboBoxState: (name: string) => JuceComboBoxState;
}

export interface JuceBackend {
	emitEvent: (eventId: string, payload: unknown) => void;
}

export interface JuceRoot {
	backend?: JuceBackend;
}

// Compile-result payload shape — must mirror
// `WebviewBridge.cpp::compileResultToVar`. Schema is null when the
// compile failed; otherwise the annotated tree
// (see `Schema.cpp::serializeForWebview`).
export interface CrucibleControlMeta {
	[key: string]: string;
}

export interface CrucibleControlSchema {
	type: "hslider" | "vslider" | "nentry" | "button" | "checkbox" | "hbargraph" | "vbargraph";
	label: string;
	address: string;
	slotId: string;
	init: number;
	min: number;
	max: number;
	step: number;
	meta: CrucibleControlMeta;
}

export interface CrucibleGroupSchema {
	type: "vgroup" | "hgroup" | "tgroup";
	label: string;
	items: Array<CrucibleSchemaNode>;
}

export type CrucibleSchemaNode = CrucibleControlSchema | CrucibleGroupSchema;

export interface CrucibleSchemaRoot {
	name: string;
	inputs: number;
	outputs: number;
	ui: CrucibleGroupSchema;
}

export interface CrucibleCompileResult {
	success: boolean;
	crossLoad: boolean;
	errorMessage: string;
	errorLine?: number;
	errorCol?: number;
	numInputs: number;
	numOutputs: number;
	latencySamples: number;
	svgRoot: string;
	schema?: CrucibleSchemaRoot;
}

export interface CrucibleSourceChanged {
	source: string;
	path: string;
}

export interface CrucibleInputError {
	kind: "fileMissing" | "fileReadFailed" | "fileWriteFailed" | "crossLoad" | "compileError";
	message: string;
	errorLine?: number;
	errorCol?: number;
	numInputs?: number;
	numOutputs?: number;
}

export interface CrucibleMeterFrame {
	inL_rms: number;
	inR_rms: number;
	inL_peak: number;
	inR_peak: number;
	outL_rms: number;
	outR_rms: number;
	outL_peak: number;
	outR_peak: number;
	bargraphs: Array<number>;
}

// Callback slots populated by C++ via `evaluateJavascript`. The bridge
// module installs these on first import; consumers subscribe via the
// `useCompileResult` etc. hooks.
export interface CrucibleWindowAPI {
	variant?: "fx" | "instrument";
	onCompileResult?: (result: CrucibleCompileResult) => void;
	onSourceChanged?: (event: CrucibleSourceChanged) => void;
	onInputError?: (error: CrucibleInputError) => void;
	onMeterFrame?: (frame: CrucibleMeterFrame) => void;
	// C++ pushes the system-clipboard text here in response to a
	// `crucible.pasteRequest`; the editor overwrites its buffer with it.
	onClipboardPaste?: (text: string) => void;
}

declare global {
	interface Window {
		__JUCE__?: JuceRoot;
		juceFrontend?: JuceFrontend;
		crucible?: CrucibleWindowAPI;
	}
}

export {};
