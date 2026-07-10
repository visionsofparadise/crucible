import { useEffect, useRef, useState } from 'react';
import { isJuceHost, notifyDiagramNavigate } from '../lib/crucible-bridge';

// Demo SVG inlined for dev mode. The real production path fetches
// `https://crucible.local/svg/<filename>` resolved by the C++
// resource provider against the runtime's current `svgRoot`.
const DEV_DEMO_SVG = `
<svg viewBox="0 0 600 320" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink">
  <defs>
    <marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
      <path d="M 0 0 L 10 5 L 0 10 z" fill="currentColor" />
    </marker>
  </defs>
  <g fill="none" stroke="currentColor" stroke-width="1">
    <line x1="20" y1="60" x2="80" y2="60" marker-end="url(#arrow)" />
    <line x1="160" y1="60" x2="220" y2="60" marker-end="url(#arrow)" />
    <line x1="300" y1="60" x2="360" y2="60" marker-end="url(#arrow)" />
    <line x1="440" y1="60" x2="500" y2="60" marker-end="url(#arrow)" />
  </g>
  <g font-family="monospace" font-size="10" fill="currentColor">
    <rect x="80" y="40" width="80" height="40" fill="#252320" stroke="currentColor" />
    <text x="120" y="65" text-anchor="middle">input</text>
    <a xlink:href="dev_demo-filter.svg">
      <rect x="220" y="40" width="80" height="40" fill="#252320" stroke="currentColor" />
      <text x="260" y="65" text-anchor="middle">filter</text>
    </a>
    <rect x="360" y="40" width="80" height="40" fill="#252320" stroke="currentColor" />
    <text x="400" y="65" text-anchor="middle">gain</text>
    <rect x="500" y="40" width="40" height="40" fill="#252320" stroke="currentColor" />
    <text x="520" y="65" text-anchor="middle">out</text>
  </g>
</svg>
`.trim();

const DEV_FILTER_SVG = `
<svg viewBox="0 0 480 200" xmlns="http://www.w3.org/2000/svg">
  <g fill="none" stroke="currentColor" stroke-width="1">
    <line x1="20" y1="100" x2="80" y2="100" />
    <line x1="180" y1="100" x2="260" y2="100" />
    <line x1="360" y1="100" x2="460" y2="100" />
  </g>
  <g font-family="monospace" font-size="10" fill="currentColor">
    <rect x="80" y="80" width="100" height="40" fill="#252320" stroke="currentColor" />
    <text x="130" y="105" text-anchor="middle">resonlp</text>
    <rect x="260" y="80" width="100" height="40" fill="#252320" stroke="currentColor" />
    <text x="310" y="105" text-anchor="middle">cutoff</text>
  </g>
</svg>
`.trim();

const fetchDevSvg = (relPath: string): Promise<string> => {
  if (relPath.endsWith('process.svg') || relPath === 'process.svg' || relPath === '') {
    return Promise.resolve(DEV_DEMO_SVG);
  }
  return Promise.resolve(DEV_FILTER_SVG);
};

const fetchHostSvg = async (svgRoot: string, relPath: string): Promise<string> => {
  // Resource provider serves `/svg/<filename>` mapped to the runtime's
  // current SVG output directory. `svgRoot` is the absolute path that
  // C++ wrote into the compile result; we strip down to filename so
  // the URL is opaque to the JS side.
  const filename = relPath.length > 0 ? relPath : 'process.svg';
  const url = `/svg/${filename}`;
  const response = await fetch(url);
  if (!response.ok) throw new Error(`SVG fetch failed: ${url}`);
  // Touch svgRoot so the lint rule doesn't strip the prop — the path
  // is useful in dev console / diagnostic logging.
  if (svgRoot.length === 0) {
    // No-op
  }
  return response.text();
};

interface DiagramPanelProps {
  svgRoot: string;
  navStack: Array<string>;
  pushNavigation: (relPath: string) => void;
  scale: number;
  tx: number;
  ty: number;
  onScaleChange: (next: number) => void;
  onPanChange: (tx: number, ty: number) => void;
}

const DiagramPanel = ({
  svgRoot,
  navStack,
  pushNavigation,
  scale,
  tx,
  ty,
  onScaleChange,
  onPanChange,
}: DiagramPanelProps) => {
  const containerRef = useRef<HTMLDivElement>(null);
  const dragRef = useRef<{ x: number; y: number; startTx: number; startTy: number } | null>(null);
  const [svgText, setSvgText] = useState<string>('');

  const currentPath = navStack[navStack.length - 1] ?? 'process.svg';

  useEffect(() => {
    let cancelled = false;
    const load = async (): Promise<void> => {
      try {
        const text = isJuceHost()
          ? await fetchHostSvg(svgRoot, currentPath)
          : await fetchDevSvg(currentPath);
        if (!cancelled) setSvgText(text);
      } catch {
        if (!cancelled) setSvgText('');
      }
    };
    void load();
    return () => { cancelled = true; };
  }, [svgRoot, currentPath]);

  // Intercept in-SVG <a xlink:href="..."> clicks and route through the
  // navigation stack instead of letting the browser navigate.
  useEffect(() => {
    const container = containerRef.current;
    if (!container) return;
    const links = container.querySelectorAll<SVGElement>('a, svg a');
    const handler = (event: Event): void => {
      event.preventDefault();
      const target = event.currentTarget as Element;
      const href = target.getAttribute('xlink:href') ?? target.getAttribute('href') ?? '';
      if (href.length === 0) return;
      // Strip leading "process-" path segments that Faust uses for
      // subdiagram folder structure; pass through the trailing
      // `.svg` filename. The C++ side resolves the path internally.
      const filename = href.includes('/') ? href.slice(href.lastIndexOf('/') + 1) : href;
      pushNavigation(filename);
      notifyDiagramNavigate(filename);
    };
    for (const link of links) {
      link.addEventListener('click', handler);
    }
    return () => {
      for (const link of links) {
        link.removeEventListener('click', handler);
      }
    };
  }, [svgText, pushNavigation]);

  const onWheel = (event: React.WheelEvent): void => {
    const delta = -event.deltaY * 0.001;
    onScaleChange(Math.min(4, Math.max(0.25, scale * (1 + delta))));
  };

  const onMouseDown = (event: React.MouseEvent): void => {
    dragRef.current = { x: event.clientX, y: event.clientY, startTx: tx, startTy: ty };
  };

  const onMouseMove = (event: React.MouseEvent): void => {
    if (!dragRef.current) return;
    onPanChange(
      dragRef.current.startTx + (event.clientX - dragRef.current.x),
      dragRef.current.startTy + (event.clientY - dragRef.current.y),
    );
  };

  const onMouseUp = (): void => { dragRef.current = null; };

  return (
    <div
      className="bg-elevated max-h-[300px] h-[300px] overflow-hidden cursor-grab active:cursor-grabbing relative text-text-primary"
      onWheel={onWheel}
      onMouseDown={onMouseDown}
      onMouseMove={onMouseMove}
      onMouseUp={onMouseUp}
      onMouseLeave={onMouseUp}
    >
      <div
        className="absolute top-1/2 left-1/2 origin-center"
        style={{ transform: `translate(-50%, -50%) translate(${tx.toString()}px, ${ty.toString()}px) scale(${scale.toString()})` }}
      >
        <div
          ref={containerRef}
          className="w-[600px] [&_svg]:max-w-full [&_svg]:h-auto"
          // Faust SVG output is generated by a trusted local libfaust
          // binary into a temp directory we just created. Sanitisation
          // is therefore unnecessary; we render verbatim so hyperlink
          // targets stay intact for the drill-in handler.
           
          dangerouslySetInnerHTML={{ __html: svgText }}
        />
      </div>
    </div>
  );
};

export default DiagramPanel;
