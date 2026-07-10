import { cn } from '../cn';

const SEG_BASE: React.CSSProperties = {
  border: 'none',
  cursor: 'pointer',
  fontFamily: 'inherit',
  fontSize: 10,
  fontVariantNumeric: 'tabular-nums',
  textTransform: 'uppercase',
  letterSpacing: '0.06em',
  padding: '3px 7px',
  borderRadius: 2,
  transition: 'background 0.1s ease',
};

const SEG_ON: React.CSSProperties = {
  ...SEG_BASE,
  background: 'var(--color-accent-primary)',
  color: 'var(--color-seg-on-text)',
  boxShadow: '0 0 6px color-mix(in srgb, var(--color-accent-primary) 60%, transparent)',
};

const SEG_OFF: React.CSSProperties = {
  ...SEG_BASE,
  background: 'transparent',
  color: 'var(--color-text-secondary)',
};

const ButtonSelection = ({
  options,
  active,
  onSelect,
  columns,
  className,
  ref,
  ...rest
}: Omit<React.ComponentPropsWithRef<'div'>, 'onSelect'> & {
  options: Array<string>;
  active: string;
  onSelect?: (option: string) => void;
  columns?: number;
}) => (
  <div
    ref={ref}
    className={cn('grid', className)}
    style={{
      gap: 2,
      gridTemplateColumns: `repeat(${String(columns ?? options.length)}, 1fr)`,
      background: '#100E0B',
      borderRadius: 3,
      padding: 2,
      boxShadow: 'inset 0 1px 2px rgba(0,0,0,0.7)',
    }}
    {...rest}
  >
    {options.map((option) => (
      <button
        key={option}
        type="button"
        onClick={() => onSelect?.(option)}
        style={option === active ? SEG_ON : SEG_OFF}
        aria-label={option}
        aria-pressed={option === active}
      >
        {option}
      </button>
    ))}
  </div>
);

export default ButtonSelection;
