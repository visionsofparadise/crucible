import { glassOverlayStyle, metalOverlayStyle } from '../lib/textures';

export const METAL_BAND_GRADIENT = 'linear-gradient(180deg,#2B2925 0%,#232019 100%)';

export const MetalOverlay = (): React.ReactElement => (
  <div aria-hidden="true" data-metal-overlay="" style={metalOverlayStyle} />
);

const LENS_GLOW =
  ', 0 0 8px color-mix(in srgb, var(--color-accent-primary) 55%, transparent), 0 0 3px color-mix(in srgb, var(--color-accent-primary) 48%, transparent)';

export interface LensSurface {
  background: string;
  boxShadow: string;
}

export const lensSurface = (on: boolean): LensSurface => (on
  ? {
      background:
        'radial-gradient(120% 100% at 50% 40%, color-mix(in srgb, var(--color-accent-primary) 50%, #ffffff) 0%, var(--color-accent-primary) 44%, color-mix(in srgb, var(--color-accent-primary) 80%, #000000) 100%)',
      boxShadow: `inset 0 1px 2px rgba(0,0,0,0.3), 0 2px 0 #241E1A, 0 3px 4px rgba(0,0,0,0.45)${LENS_GLOW}`,
    }
  : {
      background:
        'linear-gradient(180deg, rgba(255,255,255,0.20) 0%, rgba(255,255,255,0.04) 16%, rgba(255,255,255,0) 36%), radial-gradient(120% 135% at 50% 20%, #575249 0%, #3E392F 54%, #2A2721 100%)',
      boxShadow:
        'inset 0 1px 0 rgba(255,255,255,0.14), inset 0 -2px 3px rgba(0,0,0,0.3), 0 2px 0 #201D18, 0 2px 3px rgba(0,0,0,0.45)',
    });

export const lensGlyphColor = (on: boolean): string =>
  (on ? 'color-mix(in srgb, var(--color-accent-primary) 26%, #130a0b)' : '#948E84');

export const SCREEN_BG_SHADOW = 'inset 0 2px 14px rgba(0,0,0,0.85), inset 0 0 44px rgba(0,0,0,0.4)';

const SCANLINE_BG =
  'repeating-linear-gradient(to bottom, rgba(0,0,0,0) 0px, rgba(0,0,0,0) 3px, rgba(0,0,0,0.22) 3px, rgba(0,0,0,0.22) 4px)';

export const RAIL_SPECULAR =
  'linear-gradient(157deg, rgba(255,255,255,0.08) 0%, rgba(255,255,255,0.02) 14%, rgba(255,255,255,0) 32%)';

export const EDITOR_SPECULAR =
  'linear-gradient(157deg, rgba(255,255,255,0.08) 0%, rgba(255,255,255,0.02) 12%, rgba(255,255,255,0) 30%), radial-gradient(135% 90% at 46% -25%, rgba(255,255,255,0.05), rgba(255,255,255,0) 55%), radial-gradient(140% 120% at 50% 120%, color-mix(in srgb, var(--color-accent-primary) 9%, transparent) 0%, rgba(0,0,0,0) 55%)';

export const GlassScreenLayers = ({ specular }: { specular: string }): React.ReactElement => (
  <>
    <div aria-hidden="true" style={glassOverlayStyle} />
    <div aria-hidden="true" style={{ position: 'absolute', inset: 0, pointerEvents: 'none', zIndex: 2, background: SCANLINE_BG }} />
    <div aria-hidden="true" style={{ position: 'absolute', inset: 0, pointerEvents: 'none', zIndex: 3, background: specular }} />
  </>
);
