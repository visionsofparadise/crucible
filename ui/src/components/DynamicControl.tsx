import { useEffect, useRef } from 'react';
import Knob from '../design-system/components/Knob';
import Meter from '../design-system/components/Meter';
import Dropdown from '../design-system/components/Dropdown';
import ButtonSelection from '../design-system/components/ButtonSelection';
import Toggle from '../design-system/components/Toggle';
import Momentary from '../design-system/components/Momentary';
import type { CrucibleControlSchema } from '../lib/juce';
import { readBargraphSlot, useBoolSlot, useFloatSlot, useMeterFrame } from '../lib/crucible-bridge';
import { formatDb } from '../lib/db';
import { formatDisplay } from '../lib/format';

// Slot id helpers — `slot.f.012` → 12. Returns -1 if the id is empty
// (over-budget control, unbound).
const slotIndex = (slotId: string): number => {
  const match = /^slot\.[fbm]\.(\d+)$/.exec(slotId);
  const digits = match?.[1];
  if (digits === undefined) return -1;
  return Number.parseInt(digits, 10);
};

// Parse `menu{label1:value1;label2:value2}` / `radio{...}` into an
// ordered list of `{label, value}`. The metadata string preserves
// the order the user declared in the Faust source.
interface StyleOption {
  label: string;
  value: number;
}

const parseStyleOptions = (styleMeta: string | undefined): Array<StyleOption> => {
  if (!styleMeta) return [];
  const match = /^(?:menu|radio)\s*\{([^}]*)\}\s*$/.exec(styleMeta);
  const body = match?.[1];
  if (body === undefined) return [];
  return body
    .split(';')
    .map((entry) => entry.trim())
    .filter((entry) => entry.length > 0)
    .map((entry): StyleOption | null => {
      const colon = entry.indexOf(':');
      if (colon < 0) return null;
      const label = entry.slice(0, colon).trim();
      const valueText = entry.slice(colon + 1).trim();
      const value = Number.parseFloat(valueText);
      if (Number.isNaN(value)) return null;
      return { label, value };
    })
    .filter((option): option is StyleOption => option !== null);
};

// Map a raw (normalised 0..1) slot value back to one of the style
// options by nearest-value match. Used for dropdown / radio display.
const optionForSlot = (
  options: Array<StyleOption>,
  control: CrucibleControlSchema,
  normalised: number,
): StyleOption | undefined => {
  const first = options[0];
  if (first === undefined) return undefined;
  const range = control.max - control.min;
  const denormalised = control.min + normalised * range;
  let best = first;
  let bestDiff = Math.abs(denormalised - best.value);
  for (let cursor = 1; cursor < options.length; cursor++) {
    const candidate = options[cursor];
    if (candidate === undefined) continue;
    const candidateDiff = Math.abs(denormalised - candidate.value);
    if (candidateDiff < bestDiff) {
      best = candidate;
      bestDiff = candidateDiff;
    }
  }
  return best;
};

const normaliseFromValue = (control: CrucibleControlSchema, value: number): number => {
  const range = control.max - control.min;
  if (range <= 0) return 0;
  return Math.max(0, Math.min(1, (value - control.min) / range));
};

// Per-leaf component. Each renders its own primitive bound to the
// appropriate slot via `useFloatSlot` / `useBoolSlot`. Slot id parsing
// happens once at the leaf level so the bridge hooks receive numeric
// indices.

const FloatKnobControl = ({ control }: { control: CrucibleControlSchema }) => {
  const index = slotIndex(control.slotId);
  const slot = useFloatSlot(Math.max(0, index));
  const value = index < 0 ? normaliseFromValue(control, control.init) : slot.value;
  const display = formatDisplay(control, value);
  return (
    <Knob
      label={control.label}
      value={value}
      display={display}
      onChange={index < 0 ? undefined : slot.setValue}
      defaultValue={normaliseFromValue(control, control.init)}
    />
  );
};

const DropdownControl = ({
  control,
  options,
}: { control: CrucibleControlSchema; options: Array<StyleOption> }) => {
  const index = slotIndex(control.slotId);
  const slot = useFloatSlot(Math.max(0, index));
  const labels = options.map((option) => option.label);
  const active = optionForSlot(options, control, slot.value)?.label ?? labels[0] ?? '';
  return (
    <div className="flex flex-col items-center gap-2 select-none">
      <Dropdown
        value={active}
        options={labels}
        onChange={(nextLabel) => {
          const option = options.find((entry) => entry.label === nextLabel);
          if (!option || index < 0) return;
          slot.setValue(normaliseFromValue(control, option.value));
        }}
      />
      <span className="type-control-label">{control.label}</span>
    </div>
  );
};

const RadioControl = ({
  control,
  options,
}: { control: CrucibleControlSchema; options: Array<StyleOption> }) => {
  const index = slotIndex(control.slotId);
  const slot = useFloatSlot(Math.max(0, index));
  const labels = options.map((option) => option.label);
  const active = optionForSlot(options, control, slot.value)?.label ?? labels[0] ?? '';
  return (
    <div className="flex flex-col items-center gap-2 select-none">
      <ButtonSelection
        options={labels}
        active={active}
        onSelect={(nextLabel) => {
          const option = options.find((entry) => entry.label === nextLabel);
          if (!option || index < 0) return;
          slot.setValue(normaliseFromValue(control, option.value));
        }}
      />
      <span className="type-control-label">{control.label}</span>
    </div>
  );
};

const ToggleControl = ({ control }: { control: CrucibleControlSchema }) => {
  const index = slotIndex(control.slotId);
  const slot = useBoolSlot(Math.max(0, index));
  return (
    <Toggle
      label={control.label}
      on={slot.value}
      onChange={index < 0 ? undefined : slot.setValue}
    />
  );
};

const MomentaryControl = ({ control }: { control: CrucibleControlSchema }) => {
  const index = slotIndex(control.slotId);
  const slot = useBoolSlot(Math.max(0, index));
  return (
    <Momentary
      label={control.label}
      onChange={index < 0 ? undefined : slot.setValue}
    />
  );
};

// Bargraph readings are pushed at 30 Hz via the meter frame; subscribe
// ref-style and drive the meter's display via DOM mutation so the
// component does not re-render on every tick.
const HBargraphControl = ({ control }: { control: CrucibleControlSchema }) => {
  const { frameRef } = useMeterFrame();
  const barRef = useRef<HTMLDivElement | null>(null);
  const peakLabelRef = useRef<HTMLSpanElement | null>(null);

  useEffect(() => {
    let raf = 0;
    const tick = (): void => {
      const value = readBargraphSlot(frameRef.current, control.slotId);
      if (barRef.current) {
        barRef.current.style.width = `${(value * 100).toString()}%`;
      }
      if (peakLabelRef.current) {
        peakLabelRef.current.textContent = control.meta.unit === 'dB'
          ? formatDb(value)
          : value.toFixed(2);
      }
      raf = window.requestAnimationFrame(tick);
    };
    raf = window.requestAnimationFrame(tick);
    return () => { window.cancelAnimationFrame(raf); };
  }, [control.slotId, control.meta.unit, frameRef]);

  return (
    <div className="flex flex-col items-center gap-2 select-none w-[150px]">
      <span
        ref={peakLabelRef}
        className="text-center tabular-nums"
        style={{ fontFamily: 'var(--font-lcd)', fontWeight: 600, fontSize: 11, color: 'var(--color-lcd-text)', textShadow: '0 0 5px rgba(154,144,136,0.45)' }}
      >
        --
      </span>
      <Meter level={0} barRef={barRef} orientation="horizontal" width="100%" height={7} />
      <span className="type-control-label">{control.label}</span>
    </div>
  );
};

const VBargraphControl = ({ control }: { control: CrucibleControlSchema }) => {
  const { frameRef } = useMeterFrame();
  const barRef = useRef<HTMLDivElement | null>(null);

  useEffect(() => {
    let raf = 0;
    const tick = (): void => {
      const value = readBargraphSlot(frameRef.current, control.slotId);
      if (barRef.current) {
        barRef.current.style.height = `${(value * 100).toString()}%`;
      }
      raf = window.requestAnimationFrame(tick);
    };
    raf = window.requestAnimationFrame(tick);
    return () => { window.cancelAnimationFrame(raf); };
  }, [control.slotId, frameRef]);

  return (
    <div className="flex flex-col items-center gap-2 select-none">
      <Meter level={0} barRef={barRef} orientation="vertical" width={7} height={80} />
      <span className="type-control-label">{control.label}</span>
    </div>
  );
};

interface DynamicControlProps {
  control: CrucibleControlSchema;
}

// Type-dispatch the renderer. `[style:menu{...}]` and `[style:radio{...}]`
// override `hslider`/`vslider`/`nentry`'s default knob.
const DynamicControl = ({ control }: DynamicControlProps) => {
  const styleMeta = control.meta.style;
  const styleOptions = parseStyleOptions(styleMeta);

  if (control.type === 'hslider' || control.type === 'vslider' || control.type === 'nentry') {
    if (styleMeta?.startsWith('menu') && styleOptions.length > 0) {
      return <DropdownControl control={control} options={styleOptions} />;
    }
    if (styleMeta?.startsWith('radio') && styleOptions.length > 0) {
      return <RadioControl control={control} options={styleOptions} />;
    }
    return <FloatKnobControl control={control} />;
  }
  if (control.type === 'checkbox') return <ToggleControl control={control} />;
  if (control.type === 'button')   return <MomentaryControl control={control} />;
  if (control.type === 'hbargraph') return <HBargraphControl control={control} />;
  return <VBargraphControl control={control} />;
};

export default DynamicControl;
