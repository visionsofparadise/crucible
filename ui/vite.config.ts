import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import tailwindcss from "@tailwindcss/vite";

// `base: "./"` so bundled asset URLs are relative — required for the JUCE
// `withResourceProvider` strategy that serves these assets from JUCE's
// managed resource-provider origin (see Core/WebviewBridge.cpp).
//
// `rollupOptions.output` keeps filenames deterministic so the C++ resource
// provider can look them up by request path (`juce_add_binary_data` mangles
// dots/hyphens to underscores; matching against the original filename works
// only if the original filename is stable).
//
// `manualChunks` pins the vendored `juce-framework-frontend` package into
// its own deterministic chunk so the dynamic
// `import("juce-framework-frontend")` in lib/crucible-bridge.ts resolves
// to a stable filename. Pattern mirrored from gain-shaper.
export default defineConfig({
	plugins: [react(), tailwindcss()],
	base: "./",
	build: {
		rollupOptions: {
			output: {
				entryFileNames: "assets/[name].js",
				chunkFileNames: "assets/[name].js",
				assetFileNames: "assets/[name][extname]",
				manualChunks: {
					"juce-framework-frontend": ["juce-framework-frontend"],
				},
			},
		},
	},
});
