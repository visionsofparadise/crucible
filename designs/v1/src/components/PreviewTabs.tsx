import { cn } from '../design-system/cn';

interface PreviewTabsProps {
  active: 'instrument' | 'fx';
  onChange: (next: 'instrument' | 'fx') => void;
}

const tabClasses = (active: boolean) =>
  cn(
    'px-4 py-2 text-[11px] uppercase tracking-[0.08em] cursor-pointer transition-colors border-b-2 bg-transparent',
    active
      ? 'border-accent-primary text-text-primary'
      : 'border-transparent text-text-secondary hover:text-text-primary',
  );

const PreviewTabs = ({ active, onChange }: PreviewTabsProps) => (
  <div className="flex items-center gap-1 mb-6 border-b border-border w-[920px]">
    <span className="type-label mr-3">PREVIEW</span>
    <button type="button" onClick={() => { onChange('instrument'); }} className={tabClasses(active === 'instrument')}>
      Crucible Instrument
    </button>
    <button type="button" onClick={() => { onChange('fx'); }} className={tabClasses(active === 'fx')}>
      Crucible FX
    </button>
  </div>
);

export default PreviewTabs;
