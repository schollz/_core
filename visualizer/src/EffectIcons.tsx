// Glyphs from the repository's Font Awesome Free 6.2.0 solid font.
const effects = [
  { name: "Saturate", glyph: "\uf185", bit: 0 },
  { name: "Shaper", glyph: "\uf8d7", bit: 1 },
  { name: "Fuzz", glyph: "\uf6be", bit: 2 },
  { name: "Bitcrush", glyph: "\uf83e", bit: 3 },
  { name: "Time stretch", glyph: "\uf252", bit: 4 },
  { name: "Delay", glyph: "\uf1da", bit: 5 },
  { name: "Comb", glyph: "\uf55d", bit: 6 },
  { name: "Beat repeat", glyph: "\uf2f9", bit: 7 },
  { name: "Tighten", glyph: "\uf5cb", bit: 8 },
  { name: "Expand", glyph: "\uf773", bit: 9 },
  { name: "Pan", glyph: "\uf025", bit: 10 },
  { name: "Scratch", glyph: "\uf51f", bit: 11 },
  { name: "Filter", glyph: "\uf0b0", bit: 12 },
  { name: "Repitch", glyph: "\uf001", bit: 13 },
  { name: "Reverse", glyph: "\uf2ea", bit: 14 },
  { name: "Tape stop", glyph: "\uf4db", bit: 15 },
  { name: "Slow down", glyph: "\uf554", bit: null },
  { name: "Speed up", glyph: "\uf70c", bit: null },
  { name: "Retrigger", glyph: "\uf2a1", bit: null },
];

export function EffectIcons({ mask }: { mask?: number }) {
  return <div className="effect-icons" aria-label="Effects">
    {effects.map(effect => {
      const active = effect.bit !== null && mask !== undefined && !!(mask & (1 << effect.bit));
      const status = effect.bit === null ? 'not supported by this firmware' : mask === undefined ? 'state unavailable' : active ? 'active' : 'inactive';
      return <span key={effect.name} className={active ? 'effect active' : 'effect'}
        role="img" aria-label={`${effect.name}: ${status}`} title={`${effect.name}: ${status}`}>
        {effect.glyph}
        <svg className="effect-outline" viewBox="0 0 100 100" preserveAspectRatio="none" aria-hidden="true">
          <rect x="1" y="1" width="98" height="98" pathLength="1" vectorEffect="non-scaling-stroke" />
        </svg>
      </span>;
    })}
  </div>;
}
