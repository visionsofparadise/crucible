import { useState } from "react";
import ButtonSelection from "../design-system/components/ButtonSelection";
import DynamicControl from "./DynamicControl";
import type { CrucibleControlSchema, CrucibleGroupSchema, CrucibleSchemaNode } from "../lib/juce";

const isGroup = (node: CrucibleSchemaNode): node is CrucibleGroupSchema =>
	node.type === "vgroup" || node.type === "hgroup" || node.type === "tgroup";

const isControl = (node: CrucibleSchemaNode): node is CrucibleControlSchema => !isGroup(node);

const labelChildren = (group: CrucibleGroupSchema): Array<{ label: string; group: CrucibleGroupSchema }> => {
	const groups = group.items.filter(isGroup);

	return groups.map((entry) => ({ label: entry.label, group: entry }));
};


interface DynamicGroupProps {
	group: CrucibleGroupSchema;
	depth?: number;
}

const connectorStyle: React.CSSProperties = { fontSize: 10, color: "var(--color-accent-primary)", opacity: 0.7 };

const groupLabelStyle = (depth: number): React.CSSProperties =>
	depth > 0
		? { fontSize: 10, letterSpacing: "0.06em", color: "var(--color-text-sub)" }
		: { fontSize: 10, letterSpacing: "0.1em", color: "var(--color-text-primary)" };

const GroupLabelRow = ({ label, depth }: { label: string; depth: number }) => (
	<>
		{depth > 0 && <span style={connectorStyle}>└</span>}
		{label.length > 0 && (
			<span className="uppercase" style={groupLabelStyle(depth)}>
				{label}
			</span>
		)}
	</>
);

const DynamicGroup = ({ group, depth = 0 }: DynamicGroupProps) => {
	if (group.type === "tgroup") return <TabbedGroup group={group} depth={depth} />;

	const childGroups = group.items.filter(isGroup);
	const childControls = group.items.filter(isControl);

	return (
		<div className="flex flex-col gap-2">
			{group.label.length > 0 && (
				<div className="flex items-center gap-2" style={{ paddingLeft: depth * 12 }}>
					<GroupLabelRow label={group.label} depth={depth} />
				</div>
			)}
			{childControls.length > 0 && (
				<div
					className="flex flex-wrap items-end gap-x-5 gap-y-[14px] pb-2"
					style={{ paddingLeft: depth * 12 + (depth > 0 ? 16 : 0) }}
				>
					{childControls.map((control) => (
						<DynamicControl key={control.address} control={control} />
					))}
				</div>
			)}
			{childGroups.map((childGroup) => (
				<DynamicGroup
					key={
						childGroup.label ||
						childGroup.items.map((entry) => (isControl(entry) ? entry.address : entry.label)).join("|")
					}
					group={childGroup}
					depth={depth + 1}
				/>
			))}
		</div>
	);
};

const TabbedGroup = ({ group, depth }: { group: CrucibleGroupSchema; depth: number }) => {
	const labels = labelChildren(group);
	const [active, setActive] = useState<string>(labels[0]?.label ?? "");
	const activeGroup = labels.find((entry) => entry.label === active)?.group ?? labels[0]?.group;

	return (
		<div className="flex flex-col gap-2">
			<div className="flex items-center gap-2" style={{ paddingLeft: depth * 12 }}>
				<GroupLabelRow label={group.label} depth={depth} />
				<ButtonSelection options={labels.map((entry) => entry.label)} active={active} onSelect={setActive} />
			</div>
			{activeGroup && <DynamicGroup group={activeGroup} depth={depth + 1} />}
		</div>
	);
};

export default DynamicGroup;
