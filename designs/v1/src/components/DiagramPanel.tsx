import { useRef } from 'react';

const DemoDiagram = () => (
  <svg viewBox="0 0 600 320" xmlns="http://www.w3.org/2000/svg" className="block">
    <defs>
      <marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
        <path d="M 0 0 L 10 5 L 0 10 z" fill="var(--color-text-secondary)" />
      </marker>
    </defs>
    <g fill="none" stroke="var(--color-text-secondary)" strokeWidth="1">
      <line x1="20" y1="60" x2="80" y2="60" markerEnd="url(#arrow)" />
      <line x1="160" y1="60" x2="220" y2="60" markerEnd="url(#arrow)" />
      <line x1="300" y1="60" x2="360" y2="60" markerEnd="url(#arrow)" />
      <line x1="440" y1="60" x2="500" y2="60" markerEnd="url(#arrow)" />
      <line x1="540" y1="60" x2="580" y2="60" markerEnd="url(#arrow)" />

      <line x1="20" y1="160" x2="80" y2="160" markerEnd="url(#arrow)" />
      <line x1="160" y1="160" x2="220" y2="160" markerEnd="url(#arrow)" />
      <line x1="300" y1="160" x2="360" y2="160" markerEnd="url(#arrow)" />
      <line x1="440" y1="160" x2="500" y2="160" markerEnd="url(#arrow)" />
      <line x1="540" y1="160" x2="580" y2="160" markerEnd="url(#arrow)" />

      <line x1="260" y1="80" x2="260" y2="240" />
      <line x1="260" y1="240" x2="380" y2="240" markerEnd="url(#arrow)" />
      <path d="M 380 240 Q 420 240 420 200 L 420 80" />
    </g>
    <g fontFamily="var(--font-mono)" fontSize="10" fill="var(--color-text-primary)">
      <g>
        <rect x="80" y="40" width="80" height="40" fill="var(--color-elevated)" stroke="var(--color-border)" />
        <text x="120" y="65" textAnchor="middle">input</text>
      </g>
      <g>
        <rect x="220" y="40" width="80" height="40" fill="var(--color-elevated)" stroke="var(--color-border)" />
        <text x="260" y="60" textAnchor="middle">*</text>
        <text x="260" y="74" textAnchor="middle" fill="var(--color-text-secondary)">gain</text>
      </g>
      <g>
        <rect x="360" y="40" width="80" height="40" fill="var(--color-elevated)" stroke="var(--color-accent-primary)" />
        <text x="400" y="60" textAnchor="middle">tanh</text>
        <text x="400" y="74" textAnchor="middle" fill="var(--color-text-secondary)">drive</text>
      </g>
      <g>
        <rect x="500" y="40" width="40" height="40" fill="var(--color-elevated)" stroke="var(--color-border)" />
        <text x="520" y="65" textAnchor="middle">L</text>
      </g>

      <g>
        <rect x="80" y="140" width="80" height="40" fill="var(--color-elevated)" stroke="var(--color-border)" />
        <text x="120" y="165" textAnchor="middle">input</text>
      </g>
      <g>
        <rect x="220" y="140" width="80" height="40" fill="var(--color-elevated)" stroke="var(--color-border)" />
        <text x="260" y="160" textAnchor="middle">*</text>
        <text x="260" y="174" textAnchor="middle" fill="var(--color-text-secondary)">gain</text>
      </g>
      <g>
        <rect x="360" y="140" width="80" height="40" fill="var(--color-elevated)" stroke="var(--color-accent-primary)" />
        <text x="400" y="160" textAnchor="middle">tanh</text>
        <text x="400" y="174" textAnchor="middle" fill="var(--color-text-secondary)">drive</text>
      </g>
      <g>
        <rect x="500" y="140" width="40" height="40" fill="var(--color-elevated)" stroke="var(--color-border)" />
        <text x="520" y="165" textAnchor="middle">R</text>
      </g>

      <g>
        <rect x="320" y="220" width="60" height="40" fill="var(--color-elevated)" stroke="var(--color-border)" />
        <text x="350" y="240" textAnchor="middle">filter</text>
        <text x="350" y="254" textAnchor="middle" fill="var(--color-text-secondary)">lpf</text>
      </g>
    </g>
  </svg>
);

interface DiagramPanelProps {
  scale: number;
  tx: number;
  ty: number;
  onScaleChange: (next: number) => void;
  onPanChange: (tx: number, ty: number) => void;
}

const DiagramPanel = ({ scale, tx, ty, onScaleChange, onPanChange }: DiagramPanelProps) => {
  const dragRef = useRef<{ x: number; y: number; startTx: number; startTy: number } | null>(null);

  const onWheel = (e: React.WheelEvent) => {
    const delta = -e.deltaY * 0.001;
    onScaleChange(Math.min(4, Math.max(0.25, scale * (1 + delta))));
  };

  const onMouseDown = (e: React.MouseEvent) => {
    dragRef.current = { x: e.clientX, y: e.clientY, startTx: tx, startTy: ty };
  };

  const onMouseMove = (e: React.MouseEvent) => {
    if (!dragRef.current) return;
    onPanChange(
      dragRef.current.startTx + (e.clientX - dragRef.current.x),
      dragRef.current.startTy + (e.clientY - dragRef.current.y),
    );
  };

  const onMouseUp = () => {
    dragRef.current = null;
  };

  return (
    <div
      className="bg-elevated max-h-[300px] h-[300px] overflow-hidden cursor-grab active:cursor-grabbing relative"
      onWheel={onWheel}
      onMouseDown={onMouseDown}
      onMouseMove={onMouseMove}
      onMouseUp={onMouseUp}
      onMouseLeave={onMouseUp}
    >
      <div
        className="absolute top-1/2 left-1/2 origin-center"
        style={{ transform: `translate(-50%, -50%) translate(${tx}px, ${ty}px) scale(${scale})` }}
      >
        <div className="w-[600px]">
          <DemoDiagram />
        </div>
      </div>
    </div>
  );
};

export default DiagramPanel;
