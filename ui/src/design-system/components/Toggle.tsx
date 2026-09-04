import { lensSurface } from "../chrome";
import { cn } from "../cn";

interface ToggleProps {
	label: string;
	on: boolean;
	onChange?: (next: boolean) => void;
	className?: string;
}

const Toggle = ({ label, on, onChange, className }: ToggleProps) => {
	const surface = lensSurface(on);

	return (
		<div className={cn("flex flex-col items-center gap-2 select-none", className)}>
			<button
				type="button"
				aria-pressed={on}
				aria-label={label}
				onClick={() => {
					onChange?.(!on);
				}}
				className="cursor-pointer"
				style={{
					width: 22,
					height: 22,
					border: "none",
					borderRadius: 6,
					background: surface.background,
					boxShadow: surface.boxShadow,
				}}
			/>
			<span className="type-control-label">{label}</span>
		</div>
	);
};

export default Toggle;
