import type { IconProps } from './PrimaryIcon';

const RingsIcon = ({ color, size = 32, className, ref, style, ...rest }: IconProps) => (
  <div ref={ref} className={className} style={style} {...rest}>
    <svg width={size} height={size} viewBox="0 0 32 32">
      <circle cx="16" cy="16" r="14.5" stroke={color} fill="none" strokeWidth="1.4" />
      <circle cx="16" cy="16" r="10" stroke={color} fill="none" strokeWidth="1.4" opacity="0.6" />
      <circle cx="16" cy="16" r="5.5" stroke={color} fill="none" strokeWidth="1.4" opacity="0.4" />
    </svg>
  </div>
);

export default RingsIcon;
