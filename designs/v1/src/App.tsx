import { useState } from 'react';
import PluginFrame from './components/PluginFrame';
import PreviewTabs from './components/PreviewTabs';

export const App = () => {
  const [variant, setVariant] = useState<'instrument' | 'fx'>('instrument');
  return (
    <div className="min-h-screen bg-surface flex flex-col items-center py-10">
      <PreviewTabs active={variant} onChange={setVariant} />
      <PluginFrame key={variant} variant={variant} />
    </div>
  );
};
