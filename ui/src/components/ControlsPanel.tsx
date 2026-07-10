import { useCompileResult } from '../lib/crucible-bridge';
import type { CrucibleGroupSchema, CrucibleSchemaNode } from '../lib/juce';
import DynamicGroup from './DynamicGroup';

const FLOAT_SLOT_COUNT = 128;
const BOOL_SLOT_COUNT = 32;
const BARGRAPH_SLOT_COUNT = 16;

const isGroup = (node: CrucibleSchemaNode): node is CrucibleGroupSchema =>
  node.type === 'vgroup' || node.type === 'hgroup' || node.type === 'tgroup';

interface SlotCounts {
  float: number;
  bool: number;
  bargraph: number;
  unbound: number;
}

const countControls = (group: CrucibleGroupSchema, counts: SlotCounts): void => {
  for (const item of group.items) {
    if (isGroup(item)) {
      countControls(item, counts);
      continue;
    }
    const control = item;
    if (control.slotId === '') counts.unbound += 1;
    if (control.type === 'hslider' || control.type === 'vslider' || control.type === 'nentry') counts.float += 1;
    else if (control.type === 'checkbox' || control.type === 'button') counts.bool += 1;
    else counts.bargraph += 1;
  }
};

const ControlsPanel = () => {
  const result = useCompileResult();

  if (!result?.schema || !result.success) {
    return (
      <div className="bg-elevated h-[300px] max-h-[300px] overflow-y-auto scrollbar-ruler px-4 pb-3 flex items-center justify-center">
        <span className="type-label text-text-secondary">
          No source loaded — open a `.dsp` file via the INPUT toolbar.
        </span>
      </div>
    );
  }

  const counts: SlotCounts = { float: 0, bool: 0, bargraph: 0, unbound: 0 };
  countControls(result.schema.ui, counts);
  const overBudget = counts.unbound > 0
    || counts.float > FLOAT_SLOT_COUNT
    || counts.bool > BOOL_SLOT_COUNT
    || counts.bargraph > BARGRAPH_SLOT_COUNT;

  return (
    <div className="bg-elevated h-[300px] max-h-[300px] overflow-y-auto scrollbar-ruler px-4 pb-3 flex flex-col gap-2">
      {overBudget && (
        <div className="bg-surface border border-accent-primary text-accent-primary px-3 py-2 type-label tabular-nums">
          Some controls were not bound: this Faust source declares
          {' '}{counts.float} continuous, {counts.bool} boolean and
          {' '}{counts.bargraph} bargraph controls; Crucible's slot pool
          {' '}fits {FLOAT_SLOT_COUNT}/{BOOL_SLOT_COUNT}/{BARGRAPH_SLOT_COUNT}.
        </div>
      )}
      <DynamicGroup group={result.schema.ui} />
    </div>
  );
};

export default ControlsPanel;
