import type { CrucibleControlSchema } from "./juce";

export const formatDisplay = (control: CrucibleControlSchema, normalised: number): string => {
	const range = control.max - control.min;
	const denormalised = control.min + normalised * range;
	const unit = control.meta.unit;
	const suffix = unit ? ` ${unit}` : "";

	if (Math.abs(denormalised) >= 1000) {
		return `${(denormalised / 1000).toFixed(1)}k${suffix}`;
	}

	if (Number.isInteger(control.step) && control.step >= 1) {
		return `${Math.round(denormalised).toString()}${suffix}`;
	}

	return `${denormalised.toFixed(2)}${suffix}`;
};
