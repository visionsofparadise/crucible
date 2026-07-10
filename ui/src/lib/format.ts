import type { CrucibleControlSchema } from './juce';

// Render a control's current value (normalised 0..1) as a display
// string: denormalise to the control's range, then pick a format by
// magnitude and step. Values with magnitude >= 1000 collapse to a
// `k` suffix (1200 -> "1.2k"); integer-stepped controls round to a
// whole number; everything else shows two decimals. The control's
// `[unit:...]` metadata, when present, is appended to every branch.
export const formatDisplay = (control: CrucibleControlSchema, normalised: number): string => {
  const range = control.max - control.min;
  const denormalised = control.min + normalised * range;
  const unit = control.meta.unit;
  const suffix = unit ? ` ${unit}` : '';
  if (Math.abs(denormalised) >= 1000) {
    return `${(denormalised / 1000).toFixed(1)}k${suffix}`;
  }
  if (Number.isInteger(control.step) && control.step >= 1) {
    return `${Math.round(denormalised).toString()}${suffix}`;
  }
  return `${denormalised.toFixed(2)}${suffix}`;
};
