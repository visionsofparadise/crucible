import { cn } from '../cn';

interface ToggleProps {
  label: string;
  on: boolean;
  onChange?: (next: boolean) => void;
  className?: string;
}

const Toggle = ({ label, on, onChange, className }: ToggleProps) => (
  <div className={cn('flex flex-col items-center gap-2 select-none', className)}>
    <button
      type="button"
      aria-pressed={on}
      aria-label={label}
      onClick={() => { onChange?.(!on); }}
      className={cn(
        'w-10 h-10 border cursor-pointer transition-colors',
        on ? 'border-accent-primary bg-accent-primary/15' : 'border-border bg-elevated',
      )}
    />
    <span className="type-label">{label}</span>
  </div>
);

export default Toggle;
