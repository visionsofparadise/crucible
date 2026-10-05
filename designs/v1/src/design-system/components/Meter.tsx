import { cn } from '../cn';

interface MeterProps extends React.ComponentPropsWithRef<'div'> {
  level: number;
  /**
   * Bar height. Number → pixels; string → passed through (e.g. `"100%"` to
   * stretch to a flex parent's height).
   */
  height?: number | string;
  width?: number | string;
  orientation?: 'vertical' | 'horizontal';
  barRef?: React.Ref<HTMLDivElement>;
}

const Meter = ({
  level,
  height,
  width,
  orientation = 'vertical',
  barRef,
  className,
  ref,
  style,
  ...rest
}: MeterProps) => {
  const isHorizontal = orientation === 'horizontal';
  const resolvedHeight = height ?? (isHorizontal ? 4 : 80);
  const resolvedWidth = width ?? (isHorizontal ? 80 : 4);
  return (
    <div ref={ref} className={cn('relative bg-elevated', className)} style={{ width: resolvedWidth, height: resolvedHeight, ...style }} {...rest}>
      <div
        ref={barRef}
        className={isHorizontal ? 'absolute left-0 h-full' : 'absolute bottom-0 w-full'}
        style={
          isHorizontal
            ? {
                width: `${level * 100}%`,
                background: 'linear-gradient(to right, var(--color-text-primary), var(--color-accent-primary))',
              }
            : {
                height: `${level * 100}%`,
                background: 'linear-gradient(to top, var(--color-text-primary), var(--color-accent-primary))',
              }
        }
      />
    </div>
  );
};

export default Meter;
