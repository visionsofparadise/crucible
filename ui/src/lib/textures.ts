import { useLayoutEffect } from "react";
import glassUrl from "../assets/textures/glass.jpg";
import inkUrl from "../assets/textures/ink.jpg";
import metalUrl from "../assets/textures/metal.jpg";

export const metalTextureUrl = metalUrl;
export const glassTextureUrl = glassUrl;
export const inkTextureUrl = inkUrl;

export const INK_STRENGTH = 0.7;

export const metalOverlayStyle: React.CSSProperties = {
	position: "absolute",
	inset: 0,
	pointerEvents: "none",
	zIndex: -1,
	backgroundImage: `url(${metalTextureUrl})`,
	backgroundSize: "cover",
	backgroundPosition: "center",
	mixBlendMode: "var(--metal-blend, soft-light)" as React.CSSProperties["mixBlendMode"],
	opacity: "var(--metal-strength, 0.6)",
};

export const useContinuousMetal = (frameRef: React.RefObject<HTMLElement | null>): void => {
	useLayoutEffect(() => {
		const frame = frameRef.current;

		if (!frame) return;

		const apply = (): void => {
			const frameRect = frame.getBoundingClientRect();
			const size = `${frame.offsetWidth}px ${frame.offsetHeight}px`;

			frame.querySelectorAll<HTMLElement>("[data-metal-overlay]").forEach((overlay) => {
				const rect = overlay.getBoundingClientRect();

				overlay.style.backgroundSize = size;
				overlay.style.backgroundPosition = `${(-(rect.left - frameRect.left)).toFixed(1)}px ${(-(rect.top - frameRect.top)).toFixed(1)}px`;
			});
		};

		apply();

		const observer = new ResizeObserver(apply);

		observer.observe(frame);
		void document.fonts.ready.then(apply);

		return () => {
			observer.disconnect();
		};
	}, [frameRef]);
};

export const glassOverlayStyle: React.CSSProperties = {
	position: "absolute",
	inset: 0,
	pointerEvents: "none",
	zIndex: 0,
	backgroundImage: `url(${glassTextureUrl})`,
	backgroundSize: "cover",
	backgroundPosition: "center",
	mixBlendMode: "var(--glass-blend, screen)" as React.CSSProperties["mixBlendMode"],
	opacity: "var(--glass-strength, 0.02)",
};
