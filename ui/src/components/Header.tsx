import { useRef } from "react";
import { MetalOverlay, METAL_BAND_GRADIENT, lensGlyphColor, lensSurface } from "../design-system/chrome";
import DotCircleIcon from "../design-system/icons/DotCircleIcon";
import RingsIcon from "../design-system/icons/RingsIcon";
import { useBoolSlot, useVariant } from "../lib/crucible-bridge";
import { useInkMask } from "../lib/use-ink-mask";

const BYPASS_SLOT_INDEX = 0;

const ICON_GLOW = "drop-shadow(0 0 5px color-mix(in srgb, var(--color-accent-primary) 45%, transparent))";

const Header = () => {
	const variant = useVariant();
	const Icon = variant === "instrument" ? DotCircleIcon : RingsIcon;
	const { value: bypass, setValue: setBypass } = useBoolSlot(BYPASS_SLOT_INDEX);
	const titleRef = useRef<HTMLSpanElement>(null);

	useInkMask(titleRef, 0.7);

	const powerOn = !bypass;
	const surface = lensSurface(powerOn);

	return (
		<div
			className="relative flex h-full items-center overflow-hidden"
			style={{
				gap: 14,
				background: METAL_BAND_GRADIENT,
				padding: "15px 18px",
				boxShadow: "inset 0 1px 0 rgba(255,255,255,0.05), 0 1px 2px rgba(0,0,0,0.4)",
				zIndex: 0,
			}}
		>
			<MetalOverlay />
			<Icon color="var(--color-accent-primary)" size={26} className="shrink-0" style={{ filter: ICON_GLOW }} />
			<span
				ref={titleRef}
				className="shrink-0 uppercase"
				style={{
					fontFamily: "var(--font-title)",
					fontSize: 38,
					fontWeight: 400,
					letterSpacing: "0.02em",
					lineHeight: 0.85,
					color: "var(--color-text-bright)",
					textShadow:
						"0 1px 0 rgba(0,0,0,0.6), 0 0 18px color-mix(in srgb, var(--color-accent-primary) 24%, transparent)",
					transform: "rotate(-1deg)",
				}}
			>
				CRUCIBLE
			</span>
			<button
				type="button"
				aria-label="Bypass"
				aria-pressed={bypass}
				onClick={() => {
					setBypass(!bypass);
				}}
				className="ml-auto flex shrink-0 cursor-pointer items-center justify-center border-none p-0 focus-visible:outline focus-visible:outline-1 focus-visible:outline-text-secondary"
				style={{
					width: 22,
					height: 22,
					borderRadius: 6,
					background: surface.background,
					boxShadow: surface.boxShadow,
					transform: powerOn ? "translateY(1px)" : "translateY(0)",
					transition: "transform 0.09s ease, box-shadow 0.14s ease",
				}}
			>
				<svg width="15" height="15" viewBox="0 0 24 24" fill="none">
					<path d="M12 3.4 L12 11.6" stroke={lensGlyphColor(powerOn)} strokeWidth="1.4" strokeLinecap="round" />
					<path
						d="M7.4 6.7 a6 6 0 1 0 9.2 0"
						stroke={lensGlyphColor(powerOn)}
						strokeWidth="1.4"
						strokeLinecap="round"
					/>
				</svg>
			</button>
		</div>
	);
};

export default Header;
