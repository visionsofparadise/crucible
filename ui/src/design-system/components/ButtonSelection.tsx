import { cn } from '../cn'

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
    className={cn(columns ? 'grid gap-1' : 'flex flex-wrap gap-1', className)}
    style={columns ? { gridTemplateColumns: `repeat(${columns}, 1fr)` } : undefined}
    {...rest}
  >
    {options.map((option) => (
      <button
        key={option}
        onClick={() => onSelect?.(option)}
        className={cn(
          'type-label transition-colors px-2 py-1 cursor-pointer border-none',
          option === active
            ? 'bg-text-primary text-surface'
            : 'bg-transparent text-text-secondary hover:text-text-primary',
        )}
        aria-label={option}
      >
        {option}
      </button>
    ))}
  </div>
);

export default ButtonSelection;
