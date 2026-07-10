import { useCallback, useEffect, useState } from 'react';
import { ArrowUp } from 'lucide-react';
import { MetalOverlay } from '../design-system/chrome';
import PillButton from '../design-system/components/PillButton';
import { useCompileResult } from '../lib/crucible-bridge';
import ControlsPanel from './ControlsPanel';
import DiagramPanel from './DiagramPanel';

type Tab = 'controls' | 'diagram';

const tabStyle = (active: boolean): React.CSSProperties => ({
  background: 'transparent',
  border: 'none',
  padding: '2px 0',
  cursor: 'pointer',
  fontFamily: 'inherit',
  fontSize: 10,
  textTransform: 'uppercase',
  letterSpacing: '0.08em',
  color: active ? 'var(--color-text-primary)' : 'var(--color-text-secondary)',
});

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
    <div
      className="relative flex flex-col bg-elevated overflow-hidden"
      style={{ zIndex: 0, boxShadow: 'inset 0 1px 0 rgba(255,255,255,0.03)' }}
    >
      <MetalOverlay />
      <div className="relative flex items-center" style={{ zIndex: 1, height: 44, padding: '0 16px', gap: 10 }}>
        <button type="button" onClick={() => { setTab('controls'); }} style={tabStyle(tab === 'controls')}>
          CONTROLS
        </button>
        <span style={{ fontSize: 10, color: 'var(--color-gutter)' }}>/</span>
        <button type="button" onClick={() => { setTab('diagram'); }} style={tabStyle(tab === 'diagram')}>
          DIAGRAM
        </button>
        <div className="ml-auto flex items-center" style={{ gap: 14 }}>
          {tab === 'diagram' && (
            <div className="flex items-center" style={{ gap: 8 }}>
              {navStack.length > 0 && (
                <PillButton label="UP" icon={ArrowUp} onClick={popNavigation} aria-label="Up one diagram level" />
              )}
              <span
                className="tabular-nums"
                style={{ fontFamily: 'var(--font-lcd)', fontSize: 10, color: 'var(--color-lcd-text)', textShadow: '0 0 5px rgba(154,144,136,0.4)' }}
              >
                {(scale * 100).toFixed(0)}%
              </span>
              <PillButton label="RESET" onClick={resetView} />
            </div>
          )}
          <span style={{ fontSize: 9, textTransform: 'uppercase', letterSpacing: '0.06em', color: 'var(--color-text-secondary)' }}>
            ©&nbsp;ZCROSS
          </span>
        </div>
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
