import { cn } from '../cn'

export interface BarcodeProps extends Omit<React.ComponentPropsWithRef<'div'>, 'color'> {
  widths?: Array<number>
  height?: number
  gap?: number
  color?: string
  /** 'horizontal' (default) renders bars side-by-side; 'vertical' stacks them top-to-bottom. */
  direction?: 'horizontal' | 'vertical'
}

const DEFAULT_WIDTHS = [3,1,2,1,4,1,1,3,1,2,1,1,3,2,1,1,4,1,2,1,3,1,1,2,1,4,1,1,3,1,2,1,1,2,3,1,1,4,1,2,1,3,1,1,2,1,1,3,2,1]

const Barcode = ({
  widths = DEFAULT_WIDTHS,
  height = 48,
  gap = 1,
  color = 'var(--color-text-primary)',
  direction = 'horizontal',
  className,
  style,
  ref,
  ...rest
}: BarcodeProps) => {
  const vertical = direction === 'vertical'
  return (
    <div
      ref={ref}
      className={cn(vertical ? 'flex flex-col items-end' : 'flex items-end', className)}
      style={vertical ? { width: height, gap, ...style } : { height, gap, ...style }}
      {...rest}
    >
      {widths.map((barSize, ix) => (
        <div
          key={ix}
          style={
            vertical
              ? { height: barSize, width: '100%', backgroundColor: ix % 2 === 0 ? color : 'transparent' }
              : { width: barSize, height: '100%', backgroundColor: ix % 2 === 0 ? color : 'transparent' }
          }
        />
      ))}
    </div>
  )
}

export default Barcode
