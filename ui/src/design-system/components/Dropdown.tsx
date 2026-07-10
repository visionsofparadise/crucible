import { ChevronDown } from 'lucide-react';
import { cn } from '../cn';

interface DropdownProps extends Omit<React.ComponentPropsWithRef<'button'>, 'onChange'> {
  value: string;
  options: Array<string>;
  onChange?: (next: string) => void;
}

const Dropdown = ({ value, options, onChange, className, ref, ...rest }: DropdownProps) => (
  <div className="relative inline-flex">
    <button
      ref={ref}
      type="button"
      className={cn(
        'flex items-center gap-1 px-2 py-1 bg-elevated border border-border text-text-primary',
        'hover:border-text-secondary transition-colors cursor-pointer',
        'text-[10px] uppercase tracking-[0.06em] tabular-nums',
        className,
      )}
      {...rest}
    >
      <span className="text-text-primary">{value}</span>
      <ChevronDown size={10} strokeWidth={1.5} className="text-text-secondary" />
    </button>
    <select
      value={value}
      onChange={(event) => { onChange?.(event.target.value); }}
      className="absolute inset-0 opacity-0 cursor-pointer"
      aria-label="Select option"
    >
      {options.map((option) => (
        <option key={option} value={option}>
          {option}
        </option>
      ))}
    </select>
  </div>
);

export default Dropdown;
