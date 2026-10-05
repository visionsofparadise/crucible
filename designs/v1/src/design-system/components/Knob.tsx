import { useState, useCallback, useRef } from 'react';
import { cn } from '../cn';

interface KnobProps extends Omit<React.ComponentPropsWithRef<'div'>, 'onChange'> {
  value: number;
  label: string;
  size?: number;
  onChange?: (v: number) => void;
  overridden?: boolean;
  /** Optional override for the value display. Defaults to `Math.round(value * 100)`. */
  display?: string;
  /** Fixed character width for the value display, prevents jitter when display strings vary in length. */
  displayWidth?: number;
  /** Pixels of vertical drag covering the full 0..1 range. Larger = finer control. Default 150. */
  sensitivity?: number;
  /**
   * Default knob value (0..1). When set, double-clicking the SVG arc resets
   * the knob to this value via `onChange`. The reset gesture is the
   * mastering convention for "return to neutral" — every continuous parameter
   * exposes one so users can recover from over-edits without hunting for the
   * canonical position.
   */
  defaultValue?: number;
}

const Knob = ({
  value,
  label,
  size = 40,
  onChange,
  overridden = false,
  display,
  displayWidth,
  sensitivity = 150,
  defaultValue,
  className,
  ref,
  ...rest
}: KnobProps) => {
  const svgRef = useRef<SVGSVGElement>(null);
  const [dragging, setDragging] = useState(false);
  const startY = useRef(0);
  const startValue = useRef(0);

  const radius = (size - 4) / 2;
  const cx = size / 2;
  const cy = size / 2;
  const startAngle = 135;
  const totalSweep = 270;
  const valueSweep = totalSweep * value;

  const polarToCartesian = (angle: number) => {
    const rad = (angle * Math.PI) / 180;

    return {
      x: cx + radius * Math.cos(rad),
      y: cy + radius * Math.sin(rad),
    };
  };

  const arcPath = (start: number, sweep: number) => {
    if (sweep <= 0) return '';
    const end = start + sweep;
    const sp = polarToCartesian(start);
    const ep = polarToCartesian(end);
    const largeArc = sweep > 180 ? 1 : 0;

    return `M ${sp.x} ${sp.y} A ${radius} ${radius} 0 ${largeArc} 1 ${ep.x} ${ep.y}`;
  };

  const onPointerDown = useCallback(
    (ev: React.PointerEvent) => {
      if (!onChange) return;
      setDragging(true);
      (ev.target as Element).setPointerCapture(ev.pointerId);
      startY.current = ev.clientY;
      startValue.current = value;
    },
    [onChange, value],
  );

  const onPointerMove = useCallback(
    (ev: React.PointerEvent) => {
      if (!dragging || !onChange) return;
      const delta = (startY.current - ev.clientY) / sensitivity;

      onChange(Math.max(0, Math.min(1, startValue.current + delta)));
    },
    [dragging, onChange, sensitivity],
  );

  const onPointerUp = useCallback(() => {
    setDragging(false);
  }, []);

  const onDoubleClick = useCallback(() => {
    if (!onChange || defaultValue === undefined) return;
    onChange(Math.max(0, Math.min(1, defaultValue)));
  }, [onChange, defaultValue]);

  return (
    <div ref={ref} className={cn('flex flex-col items-center gap-1 select-none', className)} {...rest}>
      <span
        className="type-label tabular-nums text-center"
        style={displayWidth ? { display: 'inline-block', width: `${String(displayWidth)}ch` } : undefined}
      >
        {display ?? Math.round(value * 100)}
      </span>
      <svg
        ref={svgRef}
        width={size}
        height={size}
        className={onChange ? 'cursor-pointer' : undefined}
        onPointerDown={onPointerDown}
        onPointerMove={onPointerMove}
        onPointerUp={onPointerUp}
        onDoubleClick={onDoubleClick}
      >
        <path
          d={arcPath(startAngle, totalSweep)}
          fill="none"
          stroke="var(--color-dimmed)"
          strokeWidth={2}
          strokeLinecap="butt"
        />
        {valueSweep > 0 && (
          <path
            d={arcPath(startAngle, valueSweep)}
            fill="none"
            stroke={overridden ? 'var(--color-accent-primary)' : 'var(--color-text-primary)'}
            strokeWidth={2}
            strokeLinecap="butt"
          />
        )}
      </svg>
      {label !== '' && <span className="type-label">{label}</span>}
    </div>
  );
};

export default Knob;
