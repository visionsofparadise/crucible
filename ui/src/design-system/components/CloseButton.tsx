import { cn } from "../cn";

const CloseButton = ({
	size = 12,
	className,
	ref,
	...rest
}: React.ComponentPropsWithRef<"button"> & { size?: number }) => (
	<button
		ref={ref}
		className={cn(
			"group ml-auto h-full aspect-square bg-border hover:bg-accent-primary border-none p-0 cursor-default flex items-center justify-center transition-colors focus-visible:outline focus-visible:outline-1 focus-visible:outline-text-secondary",
			className,
		)}
		aria-label="Close"
		{...rest}
	>
		<svg
			width={size}
			height={size}
			viewBox="0 0 12 12"
			className="text-text-primary group-hover:text-surface group-hover:spin-slow transition-colors"
		>
			<line x1="1" y1="1" x2="11" y2="11" stroke="currentColor" strokeWidth="2" />
			<line x1="11" y1="1" x2="1" y2="11" stroke="currentColor" strokeWidth="2" />
		</svg>
	</button>
);

export default CloseButton;
