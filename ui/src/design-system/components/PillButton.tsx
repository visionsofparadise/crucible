import { cn } from "../cn";

interface PillButtonProps extends React.ComponentPropsWithRef<"button"> {
	label: string;
	icon?: React.ComponentType<{ size?: number | string; strokeWidth?: number | string }>;
}

const PillButton = ({ label, icon: Icon, className, ...rest }: PillButtonProps) => (
	<button
		type="button"
		aria-label={label}
		className={cn(
			"flex items-center gap-1 px-2 py-1 bg-transparent border border-border text-text-secondary hover:text-text-bright hover:border-accent-primary transition-colors cursor-pointer text-[10px] uppercase tracking-[0.06em] rounded-[3px]",
			className,
		)}
		{...rest}
	>
		{Icon && <Icon size={12} strokeWidth={1.5} />}
		<span>{label}</span>
	</button>
);

export default PillButton;
