import Header from './Header';
import InputPanel from './InputPanel';
import WorkspacePanel from './WorkspacePanel';
import MetersPanel from './MetersPanel';
import Footer from './Footer';

// Layout-only shell. Variant, bypass, source and meter state all flow
// through `crucible-bridge` hooks inside the child components — this
// component owns no application state.
const PluginFrame = () => (
  <div className="bg-surface border border-border w-[920px]">
    <Header />
    <div className="flex bg-surface">
      <div className="flex flex-col flex-1 min-w-0">
        <div className="border-b border-border">
          <InputPanel />
        </div>
        <div className="flex-1 flex flex-col">
          <WorkspacePanel />
        </div>
        <Footer />
      </div>
      <div className="border-l border-border bg-surface shrink-0">
        <MetersPanel />
      </div>
    </div>
  </div>
);

export default PluginFrame;
