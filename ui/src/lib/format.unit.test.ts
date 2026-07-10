import { describe, expect, it } from 'vitest';
import type { CrucibleControlSchema } from './juce';
import { formatDisplay } from './format';
import { formatDb } from './db';

const control = (over: Partial<CrucibleControlSchema>): CrucibleControlSchema => ({
  type: 'hslider',
  label: 'x',
  address: '/x',
  slotId: 'slot.f.000',
  init: 0,
  min: 0,
  max: 1,
  step: 0.01,
  meta: {},
  ...over,
});

describe('formatDisplay', () => {
  it('keeps the unit on the k-suffixed >= 1000 branch', () => {
    // denormalised = 0.12 * 10000 = 1200 -> "1.2k", unit must survive
    const cutoff = control({ min: 0, max: 10000, step: 1, meta: { unit: 'Hz' } });
    expect(formatDisplay(cutoff, 0.12)).toBe('1.2k Hz');
  });

  it('keeps the unit on the negative >= 1000 branch', () => {
    // denormalised = -2000 -> "-2.0k Hz"
    const swing = control({ min: -2000, max: 0, step: 1, meta: { unit: 'Hz' } });
    expect(formatDisplay(swing, 0)).toBe('-2.0k Hz');
  });

  it('rounds integer-stepped values and appends the unit', () => {
    const semis = control({ min: 0, max: 10, step: 1, meta: { unit: 'st' } });
    expect(formatDisplay(semis, 0.5)).toBe('5 st');
  });

  it('shows two decimals with the unit for fractional controls', () => {
    const time = control({ min: 0, max: 10, step: 0.01, meta: { unit: 'ms' } });
    expect(formatDisplay(time, 0.5)).toBe('5.00 ms');
  });

  it('omits the suffix entirely when no unit is declared', () => {
    const bare = control({ min: 0, max: 10, step: 1, meta: {} });
    expect(formatDisplay(bare, 0.5)).toBe('5');
  });
});

describe('formatDb', () => {
  it('reports -INF at and below silence', () => {
    expect(formatDb(0)).toBe('-INF');
    expect(formatDb(-0.5)).toBe('-INF');
  });

  it('converts a linear amplitude to one-decimal dB', () => {
    expect(formatDb(1)).toBe('0.0');
    expect(formatDb(0.5)).toBe('-6.0');
  });
});
