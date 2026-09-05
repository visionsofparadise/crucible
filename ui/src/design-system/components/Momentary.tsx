import { useCallback } from "react";
import { lensSurface } from "../chrome";
import { cn } from "../cn";

interface MomentaryProps {
	label: string;
	onTrigger?: () => void;
	/** Called when the value should drop back to false. Used by callers that
	 *  want to drive a `slot.b.NNN` write directly (true on press, false a
	 *  few ms later). */
	onChange?: (next: boolean) => void;
	releaseAfterMs?: number;
	className?: string;
}

const OFF_SURFACE = lensSurface(false);

const Momentary = ({ label, onTrigger, onChange, releaseAfterMs = 80, className }: MomentaryProps) => {
	const handlePointerDown = useCallback(() => {
		onChange?.(true);
		onTrigger?.();

		if (onChange) {
			window.setTimeout(() => {
				onChange(false);
			}, releaseAfterMs);
		}
	}, [onChange, onTrigger, releaseAfterMs]);

	return (
		<div className={cn("flex flex-col items-center gap-2 select-none", className)}>
			<button
				type="button"
				aria-label={label}
				onPointerDown={handlePointerDown}
				className="cursor-pointer active:translate-y-px transition-transform"
				style={{
					width: 22,
					height: 22,
					border: "none",
					borderRadius: 6,
					background: OFF_SURFACE.background,
					boxShadow: OFF_SURFACE.boxShadow,
				}}
			/>
			<span className="type-control-label">{label}</span>
		</div>
	);
};

export default Momentary;
