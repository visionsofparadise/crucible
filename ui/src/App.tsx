import PluginFrame from "./components/PluginFrame";

import "./lib/crucible-bridge";

export const App = () => (
	<div className="min-h-screen flex items-start justify-center bg-surface text-text-primary">
		<PluginFrame />
	</div>
);
