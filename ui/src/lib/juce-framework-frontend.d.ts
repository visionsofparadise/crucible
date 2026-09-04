// Ambient typing for the vendored `juce-framework-frontend` package.
// The shipped JS has no own .d.ts; we declare the surfaces the
// crucible-bridge module consumes via dynamic `import()`. Mirrors the
// runtime shape documented in JUCE 8's
// `juce_gui_extra/native/javascript/index.js`.

declare module "juce-framework-frontend" {
	export interface SliderState {
		getNormalisedValue: () => number;
		setNormalisedValue: (next: number) => void;
		sliderDragStarted: () => void;
		sliderDragEnded: () => void;
		valueChangedEvent: {
			addListener: (callback: () => void) => number;
			removeListener: (id: number) => void;
		};
	}
	export interface ToggleState {
		getValue: () => boolean;
		setValue: (next: boolean) => void;
		valueChangedEvent: {
			addListener: (callback: () => void) => number;
			removeListener: (id: number) => void;
		};
	}
	export interface ComboBoxState {
		getChoiceIndex: () => number;
		setChoiceIndex: (next: number) => void;
		valueChangedEvent: {
			addListener: (callback: () => void) => number;
			removeListener: (id: number) => void;
		};
	}
	export const getSliderState: (name: string) => SliderState;
	export const getToggleState: (name: string) => ToggleState;
	export const getComboBoxState: (name: string) => ComboBoxState;
}
