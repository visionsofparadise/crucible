import { useCallback } from 'react';
import { cn } from '../cn';

interface MomentaryProps {
  label: string;
  /** Called once on press, then again with `false` after `releaseAfterMs`. */
  onTrigger?: () => void;
  /** Called when the value should drop back to false. Used by callers that
   *  want to drive a `slot.b.NNN` write directly (true on press, false a
   *  few ms later). */
  onChange?: (next: boolean) => void;
  releaseAfterMs?: number;
  className?: string;
}

const Momentary = ({
  label,
  onTrigger,
  onChange,
  releaseAfterMs = 80,
  className,
}: MomentaryProps) => {
  const handlePointerDown = useCallback(() => {
    onChange?.(true);
    onTrigger?.();
    if (onChange) {
      window.setTimeout(() => { onChange(false); }, releaseAfterMs);
    }
  }, [onChange, onTrigger, releaseAfterMs]);

  return (
    <div className={cn('flex flex-col items-center gap-2 select-none', className)}>
      <button
        type="button"
        aria-label={label}
        onPointerDown={handlePointerDown}
        className="w-10 h-10 border border-border bg-elevated active:bg-accent-primary/30 active:border-accent-primary cursor-pointer transition-colors"
      />
      <span className="type-label">{label}</span>
    </div>
  );
};

export default Momentary;
