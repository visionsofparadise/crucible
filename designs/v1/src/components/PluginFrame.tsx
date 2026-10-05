import { useState } from 'react';
import Header from './Header';
import InputPanel from './InputPanel';
import WorkspacePanel from './WorkspacePanel';
import MetersPanel from './MetersPanel';
import Footer from './Footer';

interface PluginFrameProps {
  variant: 'instrument' | 'fx';
}

const INSTRUMENT_DEMO = `freq = hslider("freq",440,50,2000,0.1);
gain = hslider("gain",0.5,0,1,0.01) : si.smoo;
process = os.osc(freq) * gain <: _, _;
`;

const FX_DEMO = `cutoff = hslider("cutoff",1200,20,20000,1) : si.smoo;
res    = hslider("resonance",0.5,0,1,0.01) : si.smoo;
process = fi.resonlp(cutoff, res*10+0.5, 1);
`;

const PluginFrame = ({ variant }: PluginFrameProps) => {
  const [bypass, setBypass] = useState(false);
  const [source, setSource] = useState(variant === 'instrument' ? INSTRUMENT_DEMO : FX_DEMO);

  return (
    <div className="bg-surface border border-border w-[920px]">
      <Header variant={variant} bypass={bypass} onBypassChange={setBypass} />
      <div className="flex bg-surface">
        <div className="flex flex-col flex-1 min-w-0">
          <div className="border-b border-border">
            <InputPanel source={source} onSourceChange={setSource} />
          </div>
          <div className="flex-1 flex flex-col">
            <WorkspacePanel />
          </div>
          <Footer />
        </div>
        <div className="border-l border-border bg-surface shrink-0">
          <MetersPanel showInput={variant === 'fx'} />
        </div>
      </div>
    </div>
  );
};

export default PluginFrame;
