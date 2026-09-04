import { useEffect, useRef, useState } from "react";
import { isJuceHost, notifyDiagramNavigate } from "../lib/crucible-bridge";

// Demo SVG inlined for dev mode. The real production path fetches
// `https://crucible.local/svg/<filename>` resolved by the C++
// resource provider against the runtime's current `svgRoot`.
// This is verbatim Faust `-svg` output (captured from a real compile)
// so dev mode exercises the exact markup production renders: XML
// prolog, per-block split anchors, style-attribute fills, mm units.
const DEV_DEMO_SVG = `<?xml version="1.0"?>
<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" viewBox="0 0 101.800000 72.000000" width="50.900000mm" height="36.000000mm" version="1.1">
<rect x="1.000000" y="1.000000" width="100.800000" height="71.000000" rx="0" ry="0" style="stroke:none;fill:#cccccc;"/>
<rect x="0.000000" y="0.000000" width="100.800000" height="71.000000" rx="0" ry="0" style="stroke:none;fill:#ffffff;"/>
<text x="10.000000" y="7.000000" font-family="Arial" font-size="7"></text>
<a xlink:href="dev_demo-filter.svg">
<rect x="25.000000" y="25.000000" width="25.800000" height="24.000000" rx="0" ry="0" style="stroke:none;fill:#cccccc;"/>
<rect x="24.000000" y="24.000000" width="25.800000" height="24.000000" rx="0" ry="0" style="stroke:none;fill:#003366;"/>
</a>
<a xlink:href="dev_demo-filter.svg">
<text x="36.900000" y="38.000000" font-family="Arial" font-size="7" text-anchor="middle" fill="#FFFFFF">voice</text>
</a>
<circle cx="26.000000" cy="26.000000" r="1"/>
<line x1="15.000000" y1="15.000000" x2="15.000000" y2="57.000000"  style="stroke: black; stroke-linecap:round; stroke-width:0.25; stroke-dasharray:3,3;"/>
<line x1="15.000000" y1="57.000000" x2="86.800000" y2="57.000000"  style="stroke: black; stroke-linecap:round; stroke-width:0.25; stroke-dasharray:3,3;"/>
<line x1="86.800000" y1="57.000000" x2="86.800000" y2="15.000000"  style="stroke: black; stroke-linecap:round; stroke-width:0.25; stroke-dasharray:3,3;"/>
<line x1="15.000000" y1="15.000000" x2="20.000000" y2="15.000000"  style="stroke: black; stroke-linecap:round; stroke-width:0.25; stroke-dasharray:3,3;"/>
<line x1="49.025000" y1="15.000000" x2="86.800000" y2="15.000000"  style="stroke: black; stroke-linecap:round; stroke-width:0.25; stroke-dasharray:3,3;"/>
<text x="20.000000" y="17.000000" font-family="Arial" font-size="7">process</text>
<line x1="88.800000" y1="31.000000" x2="91.800000" y2="32.000000"  transform="rotate(0.000000,91.800000,32.000000)" style="stroke: black; stroke-width:0.25;"/>
<line x1="88.800000" y1="33.000000" x2="91.800000" y2="32.000000"  transform="rotate(0.000000,91.800000,32.000000)" style="stroke: black; stroke-width:0.25;"/>
<line x1="49.800000" y1="36.000000" x2="53.800000" y2="36.000000"  style="stroke:black; stroke-linecap:round; stroke-width:0.25;"/>
<line x1="53.800000" y1="36.000000" x2="65.800000" y2="32.000000"  style="stroke:black; stroke-linecap:round; stroke-width:0.25;"/>
<line x1="65.800000" y1="32.000000" x2="91.800000" y2="32.000000"  style="stroke:black; stroke-linecap:round; stroke-width:0.25;"/>
</svg>`;

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

// Faust's `-svg` output ships with its own light styling (white bg,
// black text, coloured blocks). Re-theme it to the Crucible screen look
// with high-specificity `!important` rules that override Faust's inline
// fills. Every selector is scoped under `.diagram-svg` (the panel
// container) — an unscoped `<style>` in the SVG string leaks globally
// and repaints every other icon/SVG in the GUI, so the rules must be
// confined to this panel. Semi-transparent block fills layer over the
// background rect so blocks read darker than the field; clickable
// subdiagram blocks (wrapped in <a>) take the accent so drill-in
// targets are obvious.
const SVG_THEME_CSS = `
  .diagram-svg rect { fill: rgba(20,17,13,0.72) !important; stroke: #4A463F !important; stroke-width: 0.6 !important; }
  .diagram-svg text { fill: #E8E3DB !important; font-family: var(--font-mono, 'Azeret Mono', monospace) !important; font-size: 6px !important; }
  .diagram-svg line, .diagram-svg polyline, .diagram-svg path { stroke: #E8E3DB !important; }
  .diagram-svg polygon, .diagram-svg circle, .diagram-svg ellipse { fill: #E8E3DB !important; }
  .diagram-svg a { cursor: pointer; }
  .diagram-svg a rect { stroke: var(--color-accent-primary, #E0653C) !important; }
  .diagram-svg a text { fill: var(--color-accent-primary, #E0653C) !important; }
`;

// Faust wraps each drillable block in a live `<a xlink:href="…svg">`.
// Left as a real hyperlink the webview treats it as navigable: hovering
// shows Chromium's status-bar URL, and a press-drag-release over the
// diagram (natural, since the diagram is also a pan surface) drops the
// link onto the page and triggers a real top-level navigation to a
// resource that isn't served, which replaces the React app with a
// failed-load error page. Rewrite the target to a data attribute the
// drill-in click handler reads; the `<a>` stays for layout/accent but
// is no longer a hyperlink, so neither symptom can occur.
const deLinkAnchors = (raw: string): string =>
	raw.replace(/<a\b[^>]*>/gi, (tag) =>
		tag.replace(/\b(?:xlink:href|href)\s*=\s*(["'])(.*?)\1/gi, "data-diagram-href=$1$2$1"),
	);

const fetchDevSvg = (relPath: string): Promise<string> => {
	if (relPath.endsWith("process.svg") || relPath === "process.svg" || relPath === "") {
		return Promise.resolve(DEV_DEMO_SVG);
	}

	return Promise.resolve(DEV_FILTER_SVG);
};

const fetchHostSvg = async (svgRoot: string, relPath: string): Promise<string> => {
	// Resource provider serves `/svg/<filename>` mapped to the runtime's
	// current SVG output directory. `svgRoot` is the absolute path that
	// C++ wrote into the compile result; we strip down to filename so
	// the URL is opaque to the JS side.
	const filename = relPath.length > 0 ? relPath : "process.svg";
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
	const dragRef = useRef<{ x: number; y: number; startTx: number; startTy: number; panning: boolean } | null>(null);
	const suppressClickRef = useRef(false);
	const [svgText, setSvgText] = useState<string>("");

	const currentPath = navStack[navStack.length - 1] ?? "process.svg";

	useEffect(() => {
		let cancelled = false;
		const load = async (): Promise<void> => {
			try {
				const text = isJuceHost() ? await fetchHostSvg(svgRoot, currentPath) : await fetchDevSvg(currentPath);

				if (!cancelled) setSvgText(deLinkAnchors(text));
			} catch {
				if (!cancelled) setSvgText("");
			}
		};

		void load();

		return () => {
			cancelled = true;
		};
	}, [svgRoot, currentPath]);

	// Drill-in navigation: one delegated listener on the container.
	// Delegation survives every innerHTML swap, and reading up from
	// `event.target` handles clicks landing on the rect/text children
	// inside the de-linked `<a>` wrappers.
	useEffect(() => {
		const container = containerRef.current;

		if (!container) return;

		const handler = (event: Event): void => {
			if (suppressClickRef.current) {
				suppressClickRef.current = false;

				return;
			}

			const target = event.target;

			if (!(target instanceof Element)) return;

			const anchor = target.closest("a[data-diagram-href]");

			if (!anchor) return;

			const href = anchor.getAttribute("data-diagram-href") ?? "";

			if (href.length === 0) return;

			// Strip leading "process-" path segments that Faust uses for
			// subdiagram folder structure; pass through the trailing
			// `.svg` filename. The C++ side resolves the path internally.
			const filename = href.includes("/") ? href.slice(href.lastIndexOf("/") + 1) : href;

			pushNavigation(filename);
			notifyDiagramNavigate(filename);
		};

		container.addEventListener("click", handler);

		return () => {
			container.removeEventListener("click", handler);
		};
	}, [pushNavigation]);

	const onWheel = (event: React.WheelEvent): void => {
		const delta = -event.deltaY * 0.001;

		onScaleChange(Math.min(4, Math.max(0.25, scale * (1 + delta))));
	};

	// Pan starts only after the pointer travels past this distance.
	// Real clicks carry a pixel or two of jitter; panning on that jitter
	// shifts the SVG under the cursor mid-click, so mousedown and
	// mouseup land on different elements and the browser retargets the
	// click to their common ancestor — the drill-in anchor never sees it.
	const DRAG_THRESHOLD_PX = 4;

	const onMouseDown = (event: React.MouseEvent): void => {
		suppressClickRef.current = false;
		dragRef.current = { x: event.clientX, y: event.clientY, startTx: tx, startTy: ty, panning: false };
	};

	const onMouseMove = (event: React.MouseEvent): void => {
		const drag = dragRef.current;

		if (!drag) return;

		const dx = event.clientX - drag.x;
		const dy = event.clientY - drag.y;

		if (!drag.panning) {
			if (Math.abs(dx) < DRAG_THRESHOLD_PX && Math.abs(dy) < DRAG_THRESHOLD_PX) return;

			drag.panning = true;
		}

		onPanChange(drag.startTx + dx, drag.startTy + dy);
	};

	const onMouseUp = (): void => {
		// A completed pan must not drill in; the click event (if any)
		// fires right after mouseup, so flag it for the click handler.
		// The flag is also cleared on the next mousedown, covering pans
		// that end outside the panel and never produce a click.
		if (dragRef.current?.panning === true) suppressClickRef.current = true;

		dragRef.current = null;
	};

	return (
		<div
			className="diagram-svg relative z-[1] max-h-[300px] h-[300px] overflow-hidden cursor-grab active:cursor-grabbing text-text-primary"
			onWheel={onWheel}
			onMouseDown={onMouseDown}
			onMouseMove={onMouseMove}
			onMouseUp={onMouseUp}
			onMouseLeave={onMouseUp}
		>
			<style>{SVG_THEME_CSS}</style>
			<div
				className="absolute top-1/2 left-1/2 origin-center"
				style={{
					transform: `translate(-50%, -50%) translate(${tx.toString()}px, ${ty.toString()}px) scale(${scale.toString()})`,
				}}
			>
				<div
					ref={containerRef}
					className="w-[600px] [&_svg]:max-w-full [&_svg]:h-auto"
					// Faust SVG output is generated by a trusted local libfaust
					// binary into a temp directory we just created, so sanitisation
					// is unnecessary. `deLinkAnchors` has already rewritten each
					// hyperlink target into a data attribute for the drill-in
					// handler; the theme is a scoped `<style>` above, not injected.
					dangerouslySetInnerHTML={{ __html: svgText }}
				/>
			</div>
		</div>
	);
};

export default DiagramPanel;
