import { useEffect, useRef } from 'react';
import Meter from '../design-system/components/Meter';
import { useMeterFrame, useVariant } from '../lib/crucible-bridge';
import type { CrucibleMeterFrame } from '../lib/juce';

interface ColumnDomRefs {
  rmsLBar: React.RefObject<HTMLDivElement | null>;
  rmsRBar: React.RefObject<HTMLDivElement | null>;
  peakLLabel: React.RefObject<HTMLSpanElement | null>;
  peakRLabel: React.RefObject<HTMLSpanElement | null>;
}

const formatDb = (peak: number): string => {
  if (peak <= 0) return '-INF';
  const db = 20 * Math.log10(peak);
  return db.toFixed(1);
};

interface MeterColumnProps {
  label: string;
  domRefs: ColumnDomRefs;
}

const MeterColumn = ({ label, domRefs }: MeterColumnProps) => (
  <div className="flex flex-col items-center gap-1 flex-1 min-h-0">
    <span className="type-label">{label}</span>
    <div className="flex flex-row gap-0.5">
      <span className="type-label">L</span>
      <span className="type-label">R</span>
    </div>
    <div className="flex flex-row gap-0.5 flex-1 min-h-0 items-stretch">
      <Meter level={0} barRef={domRefs.rmsLBar} height="100%" />
      <Meter level={0} barRef={domRefs.rmsRBar} height="100%" />
    </div>
    <div className="flex flex-col items-center">
      <span
        ref={domRefs.peakLLabel}
        className="type-value tabular-nums text-[10px] text-text-secondary text-center w-[5ch]"
      >
        -INF
      </span>
      <span
        ref={domRefs.peakRLabel}
        className="type-value tabular-nums text-[10px] text-text-secondary text-center w-[5ch]"
      >
        -INF
      </span>
    </div>
  </div>
);

const useColumnRefs = (): ColumnDomRefs => ({
  rmsLBar: useRef<HTMLDivElement | null>(null),
  rmsRBar: useRef<HTMLDivElement | null>(null),
  peakLLabel: useRef<HTMLSpanElement | null>(null),
  peakRLabel: useRef<HTMLSpanElement | null>(null),
});

const applyColumn = (
  refs: ColumnDomRefs,
  rmsL: number,
  rmsR: number,
  peakL: number,
  peakR: number,
): void => {
  if (refs.rmsLBar.current) refs.rmsLBar.current.style.height = `${(rmsL * 100).toString()}%`;
  if (refs.rmsRBar.current) refs.rmsRBar.current.style.height = `${(rmsR * 100).toString()}%`;
  if (refs.peakLLabel.current) refs.peakLLabel.current.textContent = formatDb(peakL);
  if (refs.peakRLabel.current) refs.peakRLabel.current.textContent = formatDb(peakR);
};

const MetersPanel = () => {
  const variant = useVariant();
  const showInput = variant === 'fx';

  const { frameRef } = useMeterFrame();
  const inputRefs = useColumnRefs();
  const outputRefs = useColumnRefs();

  // rAF loop reads the frameRef and mutates the DOM directly. Avoids
  // re-rendering the React tree at 30 Hz — the meter columns'
  // `Meter` primitive accepts an external `barRef` for exactly this
  // imperative path.
  useEffect(() => {
    let raf = 0;
    const tick = (): void => {
      const frame: CrucibleMeterFrame = frameRef.current;
      if (showInput) {
        applyColumn(inputRefs, frame.inL_rms, frame.inR_rms, frame.inL_peak, frame.inR_peak);
      }
      applyColumn(outputRefs, frame.outL_rms, frame.outR_rms, frame.outL_peak, frame.outR_peak);
      raf = window.requestAnimationFrame(tick);
    };
    raf = window.requestAnimationFrame(tick);
    return () => { window.cancelAnimationFrame(raf); };
  }, [frameRef, inputRefs, outputRefs, showInput]);

  return (
    <div className="bg-surface px-3 py-3 flex flex-col items-center h-full relative overflow-hidden">
      <div className="flex flex-row gap-2 flex-1 min-h-0 justify-center">
        {showInput && <MeterColumn label="IN" domRefs={inputRefs} />}
        <MeterColumn label="OUT" domRefs={outputRefs} />
      </div>
    </div>
  );
};

export default MetersPanel;
