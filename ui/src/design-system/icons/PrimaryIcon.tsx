export interface IconProps extends Omit<React.ComponentPropsWithRef<'div'>, 'color'> {
  color: string
  size?: number
}

const PrimaryIcon = ({ color, size = 32, className, ref, style, ...rest }: IconProps) => (
  <div ref={ref} style={{ width: size, height: size, backgroundColor: color, ...style }} className={className} {...rest} />
)

export default PrimaryIcon
