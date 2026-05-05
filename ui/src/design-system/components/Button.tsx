import { cn } from '../cn'

export interface ButtonProps extends React.ComponentPropsWithRef<'button'> {
  active?: boolean
  variant?: 'default' | 'ghost' | 'outline'
  size?: 'sm' | 'md'
}

const variantClasses = {
  default: 'bg-elevated border-none text-text-primary hover:text-text-primary',
  ghost: 'bg-transparent border-none text-text-secondary hover:text-text-primary',
  outline: 'bg-transparent border border-border text-text-secondary hover:text-text-primary',
} as const

const sizeClasses = {
  sm: 'px-2 py-0.5 text-[10px] uppercase tracking-[0.06em]',
  md: 'px-3 py-1.5 text-[11px] uppercase tracking-[0.04em]',
} as const

const Button = ({
  active = false,
  variant = 'default',
  size = 'md',
  className,
  disabled = false,
  ref,
  ...rest
}: ButtonProps) => (
  <button
    ref={ref}
    disabled={disabled}
    className={cn(
      'transition-colors font-[inherit]',
      disabled ? 'cursor-not-allowed' : 'cursor-pointer',
      active ? 'bg-text-primary text-surface' : variantClasses[variant],
      sizeClasses[size],
      disabled && 'opacity-50',
      'focus-visible:ring-1 focus-visible:ring-text-secondary',
      className,
    )}
    {...rest}
  />
)

export default Button
