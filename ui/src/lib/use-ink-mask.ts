import { useEffect } from "react";
import { INK_STRENGTH, inkTextureUrl } from "./textures";

export const useInkMask = (titleRef: React.RefObject<HTMLElement | null>, strength: number = INK_STRENGTH): void => {
	useEffect(() => {
		const element = titleRef.current;

		if (!element || strength <= 0) return;

		let cancelled = false;
		const image = new Image();

		image.onload = (): void => {
			if (cancelled) return;

			try {
				const width = 512;
				const height = Math.round(image.height * (width / image.width));
				const canvas = document.createElement("canvas");

				canvas.width = width;
				canvas.height = height;

				const context = canvas.getContext("2d");

				if (!context) return;

				context.drawImage(image, 0, 0, width, height);

				const source = context.getImageData(0, 0, width, height).data;

				// Smoothstep the luminance across [0.16, 0.48] into an erosion factor,
				// then write it as inverse alpha into a white RGBA mask.
				const threshold0 = 0.16;
				const threshold1 = 0.48;
				const smooth = (input: number): number => {
					const clamped = Math.min(1, Math.max(0, (input - threshold0) / (threshold1 - threshold0)));

					return clamped * clamped * (3 - 2 * clamped);
				};

				const mask = context.createImageData(width, height);
				const out = mask.data;

				for (let read = 0, write = 0; read < source.length; read += 4, write += 4) {
					const luminance =
						(0.299 * (source[read] ?? 0) + 0.587 * (source[read + 1] ?? 0) + 0.114 * (source[read + 2] ?? 0)) /
						255;
					const erode = smooth(luminance);

					out[write] = 255;
					out[write + 1] = 255;
					out[write + 2] = 255;
					out[write + 3] = Math.round(255 * (1 - erode * strength));
				}

				context.putImageData(mask, 0, 0);

				const maskUrl = `url(${canvas.toDataURL("image/png")})`;

				// `-webkit-` mask aliases are still required by the WebView2/WKWebView backends.
				const style = element.style;

				style.setProperty("mask-image", maskUrl);
				style.setProperty("-webkit-mask-image", maskUrl);
				style.setProperty("mask-size", "100% 100%");
				style.setProperty("-webkit-mask-size", "100% 100%");
				style.setProperty("mask-repeat", "no-repeat");
				style.setProperty("-webkit-mask-repeat", "no-repeat");
			} catch {
				// Canvas is tainted or unavailable — leave the wordmark unmasked.
			}
		};

		image.src = inkTextureUrl;

		return () => {
			cancelled = true;
		};
	}, [titleRef, strength]);
};
