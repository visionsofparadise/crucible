import { useState } from 'react';
import { cn } from '../design-system/cn';
import ControlsPanel from './ControlsPanel';
import DiagramPanel from './DiagramPanel';

type Tab = 'controls' | 'diagram';

const tabClasses = (active: boolean) =>
  cn(
    'type-label cursor-pointer transition-colors bg-transparent py-1 border border-transparent',
    active ? 'text-text-primary' : 'text-text-secondary hover:text-text-primary',
  );

const WorkspacePanel = () => {
  const [tab, setTab] = useState<Tab>('controls');
  const [scale, setScale] = useState(1);
  const [tx, setTx] = useState(0);
  const [ty, setTy] = useState(0);

  const resetView = () => {
    setScale(1);
    setTx(0);
    setTy(0);
  };

  const onPanChange = (nextTx: number, nextTy: number) => {
    setTx(nextTx);
    setTy(nextTy);
  };

  return (
    <div className="flex flex-col bg-elevated">
      <div className="flex items-center px-4 py-2 gap-2">
        <button type="button" onClick={() => { setTab('controls'); }} className={tabClasses(tab === 'controls')}>
          CONTROLS
        </button>
        <span className="type-label text-dimmed">/</span>
        <button type="button" onClick={() => { setTab('diagram'); }} className={tabClasses(tab === 'diagram')}>
          DIAGRAM
        </button>
        {tab === 'diagram' && (
          <div className="ml-auto flex items-center gap-2">
            <span className="type-value text-[10px] text-text-secondary tabular-nums">
              {(scale * 100).toFixed(0)}%
            </span>
            <button
              type="button"
              onClick={resetView}
              className="px-2 py-1 bg-transparent border border-border text-text-secondary hover:text-text-primary hover:border-text-secondary transition-colors cursor-pointer text-[10px] uppercase tracking-[0.06em]"
            >
              RESET
            </button>
          </div>
        )}
      </div>
      {tab === 'controls' ? (
        <ControlsPanel />
      ) : (
        <DiagramPanel scale={scale} tx={tx} ty={ty} onScaleChange={setScale} onPanChange={onPanChange} />
      )}
    </div>
  );
};

export default WorkspacePanel;
