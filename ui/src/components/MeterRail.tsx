import { useEffect, useRef } from 'react';
import { GlassScreenLayers, RAIL_SPECULAR, SCREEN_BG_SHADOW } from '../design-system/chrome';
import { useMeterFrame } from '../lib/crucible-bridge';
import { formatDb } from '../lib/db';

type Side = 'in' | 'out';

const LED_MASK =
  'repeating-linear-gradient(to top, rgba(9,8,6,0) 0px, rgba(9,8,6,0) 2.6px, #090806 2.6px, #090806 4.1px)';

const RailBar = ({ fillRef }: { fillRef: React.RefObject<HTMLDivElement | null> }) => (
  <div
    className="relative overflow-hidden"
    style={{ width: 7, background: 'var(--color-meter-track)', borderRadius: 1, boxShadow: 'inset 0 1px 2px rgba(0,0,0,0.6)' }}
  >
    <div
      ref={fillRef}
      className="absolute bottom-0 w-full"
      style={{ height: '0%', background: 'linear-gradient(to top, #E8E3DB, var(--color-accent-primary))' }}
    />
    <div aria-hidden="true" className="absolute inset-0 pointer-events-none" style={{ background: LED_MASK }} />
  </div>
);

const Readout = ({ readoutRef }: { readoutRef: React.RefObject<HTMLSpanElement | null> }) => (
  <span
    ref={readoutRef}
    className="text-center tabular-nums"
    style={{ fontFamily: 'var(--font-lcd)', fontSize: 9, color: 'var(--color-lcd-text)', textShadow: '0 0 5px rgba(154,144,136,0.4)', width: '5ch' }}
  >
    -INF
  </span>
);

const MeterRail = ({ side }: { side: Side }) => {
  const { frameRef } = useMeterFrame();
  const leftFill = useRef<HTMLDivElement>(null);
  const rightFill = useRef<HTMLDivElement>(null);
  const leftReadout = useRef<HTMLSpanElement>(null);
  const rightReadout = useRef<HTMLSpanElement>(null);

  useEffect(() => {
    let raf = 0;
    const tick = (): void => {
      const frame = frameRef.current;
      const rmsL = side === 'in' ? frame.inL_rms : frame.outL_rms;
      const rmsR = side === 'in' ? frame.inR_rms : frame.outR_rms;
      const peakL = side === 'in' ? frame.inL_peak : frame.outL_peak;
      const peakR = side === 'in' ? frame.inR_peak : frame.outR_peak;
      if (leftFill.current) leftFill.current.style.height = `${(rmsL * 100).toFixed(1)}%`;
      if (rightFill.current) rightFill.current.style.height = `${(rmsR * 100).toFixed(1)}%`;
      if (leftReadout.current) leftReadout.current.textContent = formatDb(peakL);
      if (rightReadout.current) rightReadout.current.textContent = formatDb(peakR);
      raf = window.requestAnimationFrame(tick);
    };
    raf = window.requestAnimationFrame(tick);
    return () => { window.cancelAnimationFrame(raf); };
  }, [frameRef, side]);

  const label = side === 'in' ? 'IN' : 'OUT';
  const edge = side === 'in' ? { borderRight: '1px solid #000' } : { borderLeft: '1px solid #000' };

  return (
    <div
      className="relative flex flex-col shrink-0"
      style={{ background: 'var(--color-screen)', boxShadow: SCREEN_BG_SHADOW, zIndex: 0, ...edge }}
    >
      <GlassScreenLayers specular={RAIL_SPECULAR} />
      <div className="relative flex flex-1 min-h-0 justify-center" style={{ zIndex: 1, padding: 14 }}>
        <div className="flex flex-col items-center min-h-0" style={{ gap: 6 }}>
          <span className="uppercase" style={{ fontSize: 10, letterSpacing: '0.06em', color: 'var(--color-text-secondary)' }}>{label}</span>
          <div className="flex" style={{ gap: 5 }}>
            <span className="text-center" style={{ fontSize: 9, width: 7, color: 'var(--color-dimmed)' }}>L</span>
            <span className="text-center" style={{ fontSize: 9, width: 7, color: 'var(--color-dimmed)' }}>R</span>
          </div>
          <div className="flex flex-1 min-h-0 items-stretch" style={{ gap: 5 }}>
            <RailBar fillRef={leftFill} />
            <RailBar fillRef={rightFill} />
          </div>
          <div className="flex flex-col items-center">
            <Readout readoutRef={leftReadout} />
            <Readout readoutRef={rightReadout} />
          </div>
        </div>
      </div>
    </div>
  );
};

export default MeterRail;
