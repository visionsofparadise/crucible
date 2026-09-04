import { cn } from "../cn";

interface MeterProps extends React.ComponentPropsWithRef<"div"> {
	level: number;
	/**
	 * Bar height. Number → pixels; string → passed through (e.g. `"100%"` to
	 * stretch to a flex parent's height).
	 */
	height?: number | string;
	width?: number | string;
	orientation?: "vertical" | "horizontal";
	barRef?: React.Ref<HTMLDivElement>;
}

const LED_MASK_HORIZONTAL =
	"repeating-linear-gradient(to right, rgba(9,8,6,0) 0px, rgba(9,8,6,0) 2.6px, var(--color-meter-track) 2.6px, var(--color-meter-track) 4.1px)";
const LED_MASK_VERTICAL =
	"repeating-linear-gradient(to top, rgba(9,8,6,0) 0px, rgba(9,8,6,0) 2.6px, var(--color-meter-track) 2.6px, var(--color-meter-track) 4.1px)";

const Meter = ({
	level,
	height,
	width,
	orientation = "vertical",
	barRef,
	className,
	ref,
	style,
	...rest
}: MeterProps) => {
	const isHorizontal = orientation === "horizontal";
	const resolvedHeight = height ?? (isHorizontal ? 7 : 80);
	const resolvedWidth = width ?? (isHorizontal ? 80 : 7);

	return (
		<div
			ref={ref}
			className={cn("relative overflow-hidden", className)}
			style={{
				width: resolvedWidth,
				height: resolvedHeight,
				background: "var(--color-meter-track)",
				borderRadius: 1,
				boxShadow: "inset 0 1px 2px rgba(0,0,0,0.7)",
				...style,
			}}
			{...rest}
		>
			<div
				ref={barRef}
				className={isHorizontal ? "absolute left-0 top-0 h-full" : "absolute bottom-0 w-full"}
				style={
					isHorizontal
						? {
								width: `${(level * 100).toString()}%`,
								background: "linear-gradient(to right, #E8E3DB, var(--color-accent-primary))",
							}
						: {
								height: `${(level * 100).toString()}%`,
								background: "linear-gradient(to top, #E8E3DB, var(--color-accent-primary))",
							}
				}
			/>
			<div
				aria-hidden="true"
				className="absolute inset-0 pointer-events-none"
				style={{ background: isHorizontal ? LED_MASK_HORIZONTAL : LED_MASK_VERTICAL }}
			/>
		</div>
	);
};

export default Meter;
