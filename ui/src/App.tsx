import PluginFrame from "./components/PluginFrame";

// Side-effect import: registers the JUCE-host callback slots, the
// dev-mode synthetic compile loop, and the document-level
// pointerdown listener that drives click-on-top.
import "./lib/crucible-bridge";

export const App = () => (
	<div className="min-h-screen flex items-start justify-center bg-surface text-text-primary">
		<PluginFrame />
	</div>
);
