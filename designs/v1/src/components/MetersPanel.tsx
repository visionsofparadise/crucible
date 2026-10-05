import Meter from '../design-system/components/Meter';

const formatDb = (peak: number) => {
  if (peak <= 0) return '-INF';
  const db = 20 * Math.log10(peak);
  return db.toFixed(1);
};

interface ColumnProps {
  label: string;
  peakL: number;
  peakR: number;
  rmsL: number;
  rmsR: number;
}

const MeterColumn = ({ label, peakL, peakR, rmsL, rmsR }: ColumnProps) => (
  <div className="flex flex-col items-center gap-1 flex-1 min-h-0">
    <span className="type-label">{label}</span>
    <div className="flex flex-row gap-0.5">
      <span className="type-label">L</span>
      <span className="type-label">R</span>
    </div>
    <div className="flex flex-row gap-0.5 flex-1 min-h-0 items-stretch">
      <Meter level={rmsL} height="100%" />
      <Meter level={rmsR} height="100%" />
    </div>
    <div className="flex flex-col items-center">
      <span className="type-value tabular-nums text-[10px] text-text-secondary text-center w-[5ch]">
        {formatDb(peakL)}
      </span>
      <span className="type-value tabular-nums text-[10px] text-text-secondary text-center w-[5ch]">
        {formatDb(peakR)}
      </span>
    </div>
  </div>
);

interface MetersPanelProps {
  showInput?: boolean;
}

const MetersPanel = ({ showInput = false }: MetersPanelProps) => (
  <div className="bg-surface px-3 py-3 flex flex-col items-center h-full relative overflow-hidden">
    <div className="flex flex-row gap-2 flex-1 min-h-0 justify-center">
      {showInput && <MeterColumn label="IN" peakL={0.7} peakR={0.65} rmsL={0.4} rmsR={0.38} />}
      <MeterColumn label="OUT" peakL={0.85} peakR={0.78} rmsL={0.55} rmsR={0.5} />
    </div>
  </div>
);

export default MetersPanel;
