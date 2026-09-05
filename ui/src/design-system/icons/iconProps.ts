export interface IconProps extends Omit<React.ComponentPropsWithRef<"div">, "color"> {
	color: string;
	size?: number;
}
