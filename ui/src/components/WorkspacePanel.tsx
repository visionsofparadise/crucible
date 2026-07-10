import { useCallback, useEffect, useState } from 'react';
import { ArrowUp } from 'lucide-react';
import { cn } from '../design-system/cn';
import { useCompileResult } from '../lib/crucible-bridge';
import ControlsPanel from './ControlsPanel';
import DiagramPanel from './DiagramPanel';

type Tab = 'controls' | 'diagram';

const tabClasses = (active: boolean) =>
  cn(
    'type-label cursor-pointer transition-colors bg-transparent py-1 border border-transparent',
    active ? 'text-text-primary' : 'text-text-secondary hover:text-text-primary',
  );

const WorkspacePanel = () => {
  const result = useCompileResult();
  const [tab, setTab] = useState<Tab>('controls');
  const [scale, setScale] = useState(1);
  const [tx, setTx] = useState(0);
  const [ty, setTy] = useState(0);
  // Drill-in navigation stack. `[]` means we're on `process.svg`. Each
  // entry is the filename of a subdiagram (e.g. `process-filter.svg`).
  const [navStack, setNavStack] = useState<Array<string>>([]);

  const resetView = useCallback((): void => {
    setScale(1);
    setTx(0);
    setTy(0);
  }, []);

  // Compile result change → reset drill-in stack and pan/zoom so the
  // user sees the root diagram of the freshly-compiled program.
  useEffect(() => {
    if (result?.success === true) {
      setNavStack([]);
      resetView();
    }
  }, [result, resetView]);

  const pushNavigation = useCallback((relPath: string): void => {
    setNavStack((stack) => [...stack, relPath]);
    setScale(1);
    setTx(0);
    setTy(0);
  }, []);

  const popNavigation = useCallback((): void => {
    setNavStack((stack) => stack.slice(0, -1));
    setScale(1);
    setTx(0);
    setTy(0);
  }, []);

  const onPanChange = (nextTx: number, nextTy: number): void => {
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
            {navStack.length > 0 && (
              <button
                type="button"
                onClick={popNavigation}
                aria-label="Up one diagram level"
                className="flex items-center gap-1 px-2 py-1 bg-transparent border border-border text-text-secondary hover:text-text-primary hover:border-text-secondary transition-colors cursor-pointer text-[10px] uppercase tracking-[0.06em]"
              >
                <ArrowUp size={12} strokeWidth={1.5} />
                <span>UP</span>
              </button>
            )}
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
        <DiagramPanel
          svgRoot={result?.svgRoot ?? ''}
          navStack={navStack}
          pushNavigation={pushNavigation}
          scale={scale}
          tx={tx}
          ty={ty}
          onScaleChange={setScale}
          onPanChange={onPanChange}
        />
      )}
    </div>
  );
};

export default WorkspacePanel;
