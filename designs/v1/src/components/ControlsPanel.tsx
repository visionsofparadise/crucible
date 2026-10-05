import Knob from '../design-system/components/Knob';
import ButtonSelection from '../design-system/components/ButtonSelection';
import Dropdown from '../design-system/components/Dropdown';
import Meter from '../design-system/components/Meter';

const Toggle = ({ label, on }: { label: string; on: boolean }) => (
  <div className="flex flex-col items-center gap-2 select-none">
    <button
      type="button"
      aria-pressed={on}
      className={`w-10 h-10 border ${
        on ? 'border-accent-primary bg-accent-primary/15' : 'border-border bg-elevated'
      } cursor-pointer transition-colors`}
    />
    <span className="type-label">{label}</span>
  </div>
);

const Momentary = ({ label }: { label: string }) => (
  <div className="flex flex-col items-center gap-2 select-none">
    <button
      type="button"
      className="w-10 h-10 border border-border bg-elevated active:bg-accent-primary/30 active:border-accent-primary cursor-pointer transition-colors"
    />
    <span className="type-label">{label}</span>
  </div>
);

const ButtonSelectField = ({ label, options, active }: { label: string; options: Array<string>; active: string }) => (
  <div className="flex flex-col items-center gap-2 select-none">
    <ButtonSelection options={options} active={active} />
    <span className="type-label">{label}</span>
  </div>
);

const DropdownField = ({ label, value, options }: { label: string; value: string; options: Array<string> }) => (
  <div className="flex flex-col items-center gap-2 select-none">
    <Dropdown value={value} options={options} />
    <span className="type-label">{label}</span>
  </div>
);

const HBargraph = ({ label, level, peak }: { label: string; level: number; peak: string }) => (
  <div className="flex flex-col items-center gap-2 select-none w-[120px]">
    <span className="type-label tabular-nums text-center">{peak}</span>
    <Meter level={level} orientation="horizontal" width="100%" height={4} />
    <span className="type-label">{label}</span>
  </div>
);

interface GroupProps {
  name: string;
  depth?: number;
  children?: React.ReactNode;
}

const Group = ({ name, depth = 0, children }: GroupProps) => (
  <div className="flex flex-col gap-2">
    <div className="flex items-center gap-2" style={{ paddingLeft: depth * 12 }}>
      {depth > 0 && <span className="type-label text-dimmed">└</span>}
      <span className="type-label text-text-primary">{name}</span>
    </div>
    {children && (
      <div
        className="flex flex-wrap items-end gap-x-4 gap-y-3 pb-3"
        style={{ paddingLeft: depth * 12 + (depth > 0 ? 16 : 0) }}
      >
        {children}
      </div>
    )}
  </div>
);

const ControlsPanel = () => (
  <div className="bg-elevated h-[300px] max-h-[300px] overflow-y-auto scrollbar-ruler px-4 pb-3 flex flex-col gap-2">
    <Group name="SYNTH">
      <Group name="OSCILLATOR" depth={1}>
        <Knob label="FREQ" value={0.22} display="440" displayWidth={5} />
        <Knob label="DETUNE" value={0.55} display="+3¢" displayWidth={5} />
        <Knob label="GAIN" value={0.7} display="-12dB" displayWidth={5} />
        <Knob label="VOICES" value={0.4} display="4" displayWidth={5} />
        <ButtonSelectField label="WAVE" options={['SIN', 'SAW', 'SQR', 'TRI']} active="SAW" />
        <DropdownField label="ALGO" value="FM" options={['FM', 'AM', 'PM', 'PD']} />
      </Group>
      <Group name="FILTER" depth={1}>
        <Knob label="CUTOFF" value={0.42} display="1.2k" displayWidth={5} />
        <Knob label="RES" value={0.45} display="45%" displayWidth={5} />
        <Knob label="DRIVE" value={0.1} display="10%" displayWidth={5} />
        <ButtonSelectField label="TYPE" options={['LP', 'HP', 'BP', 'NT']} active="LP" />
        <Toggle label="OS" on={true} />
      </Group>
      <Group name="ENVELOPE" depth={1}>
        <Knob label="ATTACK" value={0.05} display="5ms" displayWidth={6} />
        <Knob label="DECAY" value={0.12} display="120ms" displayWidth={6} />
        <Knob label="SUSTAIN" value={0.7} display="70%" displayWidth={6} />
        <Knob label="RELEASE" value={0.25} display="250ms" displayWidth={6} />
        <Toggle label="LOOP" on={false} />
        <Momentary label="TRIG" />
      </Group>
    </Group>
    <Group name="REVERB">
      <Knob label="SIZE" value={0.6} display="60%" displayWidth={5} />
      <Knob label="DAMP" value={0.4} display="40%" displayWidth={5} />
      <Knob label="MIX" value={0.25} display="25%" displayWidth={5} />
    </Group>
    <Group name="ANALYSIS">
      <HBargraph label="RMS" level={0.55} peak="-5.2" />
      <HBargraph label="PEAK" level={0.85} peak="-1.4" />
      <HBargraph label="LUFS" level={0.4} peak="-14.0" />
    </Group>
  </div>
);

export default ControlsPanel;
