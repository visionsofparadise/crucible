interface JuceListenerHandle {
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

interface JuceComboBoxState {
	getChoiceIndex: () => number;
	setChoiceIndex: (next: number) => void;
	valueChangedEvent: JuceListenerHandle;
}

export interface JuceFrontend {
	getSliderState: (name: string) => JuceSliderState;
	getToggleState: (name: string) => JuceToggleState;
	getComboBoxState: (name: string) => JuceComboBoxState;
}

interface JuceBackend {
	emitEvent: (eventId: string, payload: unknown) => void;
}

interface JuceRoot {
	backend?: JuceBackend;
}

interface CrucibleControlMeta {
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

interface CrucibleWindowAPI {
	variant?: "fx" | "instrument";
	onCompileResult?: (result: CrucibleCompileResult) => void;
	onSourceChanged?: (event: CrucibleSourceChanged) => void;
	onInputError?: (error: CrucibleInputError) => void;
	onMeterFrame?: (frame: CrucibleMeterFrame) => void;
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
