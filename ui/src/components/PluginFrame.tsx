import { useRef } from 'react';
import Header from './Header';
import InputPanel from './InputPanel';
import WorkspacePanel from './WorkspacePanel';
import { useContinuousMetal } from '../lib/textures';

// Band heights (62 + 2 + 334 + 2 + 344) sum to 744; both PluginEditor.cpp files
// pass setSize(680, 744). Changing any height here requires changing that sum in both.
const FRAME_SHADOW =
  '0 30px 70px rgba(0,0,0,0.6), 0 2px 0 rgba(255,255,255,0.03), inset 0 1px 0 rgba(255,255,255,0.07)';
const HEADER_SEAM =
  'linear-gradient(180deg, rgba(255,255,255,0.11) 0%, rgba(255,255,255,0.03) 35%, rgba(0,0,0,0.55) 100%)';
const WORKSPACE_SEAM =
  'linear-gradient(180deg, rgba(0,0,0,0.55) 0%, rgba(255,255,255,0.03) 65%, rgba(255,255,255,0.11) 100%)';

const PluginFrame = () => {
  const frameRef = useRef<HTMLDivElement>(null);
  useContinuousMetal(frameRef);

  return (
    <div
      ref={frameRef}
      className="w-[680px] overflow-hidden bg-surface text-text-primary"
      style={{ boxShadow: FRAME_SHADOW }}
    >
      <div className="h-[62px] overflow-hidden">
        <Header />
      </div>
      <div className="h-[2px]" style={{ background: HEADER_SEAM }} />
      <div className="h-[334px] overflow-hidden">
        <InputPanel />
      </div>
      <div className="h-[2px]" style={{ background: WORKSPACE_SEAM }} />
      <div className="h-[344px] overflow-hidden">
        <WorkspacePanel />
      </div>
    </div>
  );
};

export default PluginFrame;
