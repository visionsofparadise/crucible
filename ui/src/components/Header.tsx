import { Power } from 'lucide-react';
import DotCircleIcon from '../design-system/icons/DotCircleIcon';
import RingsIcon from '../design-system/icons/RingsIcon';
import { useBoolSlot, useVariant } from '../lib/crucible-bridge';

// Bypass binds to `slot.b.000` — `kBypassSlotIndex` in `Core/SlotPool.h`.
const BYPASS_SLOT_INDEX = 0;

const Header = () => {
  const variant = useVariant();
  const Icon = variant === 'instrument' ? DotCircleIcon : RingsIcon;
  const title = variant === 'instrument' ? 'CRUCIBLE INSTRUMENT' : 'CRUCIBLE FX';
  const { value: bypass, setValue: setBypass } = useBoolSlot(BYPASS_SLOT_INDEX);
  return (
    <div className="flex items-center bg-elevated border-b border-border px-4 py-2 gap-3">
      <Icon color="var(--color-accent-primary)" size={20} className="shrink-0" />
      <span className="type-display text-[24px] leading-none shrink-0">{title}</span>
      <button
        type="button"
        aria-label="Bypass"
        aria-pressed={bypass}
        onClick={() => { setBypass(!bypass); }}
        className="ml-auto cursor-pointer bg-transparent border-none p-1 flex items-center justify-center transition-colors focus-visible:outline focus-visible:outline-1 focus-visible:outline-text-secondary"
      >
        <Power
          size={20}
          strokeWidth={1.5}
          className={bypass ? 'text-text-secondary hover:text-text-primary' : 'text-accent-primary'}
        />
      </button>
    </div>
  );
};

export default Header;
