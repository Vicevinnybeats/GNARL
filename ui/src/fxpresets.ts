/*
 * Presets for each effect on the rack: a few named settings per slot, which
 * the slot's PRESET button steps through. Each sets only that effect's own
 * controls (page values, as the knobs and buttons set them) and switches it
 * on, so one effect's preset never changes the rest of the patch.
 *
 * Starting points chosen by ear-free reasoning about each control's range,
 * not by measurement: the producer judges them (CLAUDE.md, section 7).
 * Bipolar controls (feedback, EQ gains) sit at 0.5 for zero.
 */

export interface FxPreset {
  name: string;
  values: Readonly<Record<string, number>>;
}

export const FX_PRESETS: Readonly<Record<string, readonly FxPreset[]>> = {
  dist: [
    { name: 'WARM', values: { 'dist.mode': 0, 'dist.drive': 0.45, 'dist.mix': 0.6 } },
    { name: 'GRIT', values: { 'dist.mode': 1, 'dist.drive': 0.65, 'dist.mix': 0.8 } },
    { name: 'SMASH', values: { 'dist.mode': 1, 'dist.drive': 0.9, 'dist.mix': 1 } },
    { name: 'FUZZ', values: { 'dist.mode': 2, 'dist.drive': 0.85, 'dist.mix': 1 } },
    { name: 'EDGE', values: { 'dist.mode': 2, 'dist.drive': 0.35, 'dist.mix': 0.5 } },
    { name: 'BLOWN', values: { 'dist.mode': 0, 'dist.drive': 1, 'dist.mix': 1 } },
  ],
  fold: [
    { name: 'SOFT', values: { 'fold.mode': 0, 'fold.amount': 0.3, 'fold.mix': 0.6 } },
    { name: 'GROWL', values: { 'fold.mode': 0, 'fold.amount': 0.55, 'fold.mix': 1 } },
    { name: 'METAL', values: { 'fold.mode': 1, 'fold.amount': 0.75, 'fold.mix': 1 } },
    { name: 'TEAR', values: { 'fold.mode': 0, 'fold.amount': 0.8, 'fold.mix': 1 } },
    { name: 'BLEND', values: { 'fold.mode': 1, 'fold.amount': 0.45, 'fold.mix': 0.45 } },
    { name: 'WARP', values: { 'fold.mode': 1, 'fold.amount': 1, 'fold.mix': 0.8 } },
  ],
  crush: [
    { name: 'LO-FI', values: { 'crush.mode': 1, 'crush.bits': 10, 'crush.rate': 0.2 } },
    { name: '8-BIT', values: { 'crush.mode': 0, 'crush.bits': 8, 'crush.rate': 0.35 } },
    { name: 'WRECK', values: { 'crush.mode': 0, 'crush.bits': 4, 'crush.rate': 0.6 } },
    { name: 'DUST', values: { 'crush.mode': 1, 'crush.bits': 12, 'crush.rate': 0.1 } },
    { name: 'ROBOT', values: { 'crush.mode': 0, 'crush.bits': 6, 'crush.rate': 0.75 } },
    { name: 'GLITCH', values: { 'crush.mode': 0, 'crush.bits': 3, 'crush.rate': 0.85 } },
  ],
  ott: [
    { name: 'LIGHT', values: { 'ott.mode': 0, 'ott.depth': 0.25, 'ott.time': 0.5 } },
    { name: 'RIDDIM', values: { 'ott.mode': 0, 'ott.depth': 0.6, 'ott.time': 0.4 } },
    { name: 'SQUASH', values: { 'ott.mode': 0, 'ott.depth': 1, 'ott.time': 0.3 } },
    { name: '2-BAND', values: { 'ott.mode': 1, 'ott.depth': 0.6, 'ott.time': 0.5 } },
    { name: 'PUNCH', values: { 'ott.mode': 0, 'ott.depth': 0.5, 'ott.time': 0.15 } },
    { name: 'GLUE', values: { 'ott.mode': 1, 'ott.depth': 0.35, 'ott.time': 0.7 } },
  ],
  chorus: [
    { name: 'SUBTLE', values: { 'chorus.depth': 0.25, 'chorus.feedback': 0.5, 'chorus.mix': 0.2 } },
    { name: 'WIDE', values: { 'chorus.depth': 0.5, 'chorus.feedback': 0.5, 'chorus.mix': 0.35 } },
    { name: 'THICK', values: { 'chorus.depth': 0.8, 'chorus.feedback': 0.65, 'chorus.mix': 0.5 } },
    { name: 'DOUBLE', values: { 'chorus.depth': 0.15, 'chorus.feedback': 0.5, 'chorus.mix': 0.45 } },
    { name: 'WARBLE', values: { 'chorus.depth': 1, 'chorus.feedback': 0.3, 'chorus.mix': 0.4 } },
    { name: 'HAZE', values: { 'chorus.depth': 0.65, 'chorus.feedback': 0.85, 'chorus.mix': 0.6 } },
  ],
  flanger: [
    { name: 'SWEEP', values: { 'flanger.rate': 0, 'flanger.depth': 0.6, 'flanger.feedback': 0.7, 'flanger.mix': 0.5 } },
    { name: 'JET', values: { 'flanger.rate': 1, 'flanger.depth': 0.8, 'flanger.feedback': 0.85, 'flanger.mix': 0.6 } },
    { name: 'METAL', values: { 'flanger.rate': 3, 'flanger.depth': 0.3, 'flanger.feedback': 0.95, 'flanger.mix': 0.7 } },
    { name: 'GRIND', values: { 'flanger.rate': 2, 'flanger.depth': 0.5, 'flanger.feedback': 0.1, 'flanger.mix': 0.6 } },
    { name: 'SHIMMER', values: { 'flanger.rate': 1, 'flanger.depth': 0.25, 'flanger.feedback': 0.7, 'flanger.mix': 0.35 } },
    { name: 'ROBOT', values: { 'flanger.rate': 3, 'flanger.depth': 0.1, 'flanger.feedback': 0.98, 'flanger.mix': 0.8 } },
  ],
  phaser: [
    { name: 'SLOW', values: { 'phaser.rate': 0, 'phaser.feedback': 0.4, 'phaser.center': 0.5, 'phaser.mix': 0.6 } },
    { name: 'WOBBLE', values: { 'phaser.rate': 2, 'phaser.feedback': 0.7, 'phaser.center': 0.55, 'phaser.mix': 0.8 } },
    { name: 'TALK', values: { 'phaser.rate': 3, 'phaser.feedback': 0.85, 'phaser.center': 0.65, 'phaser.mix': 1 } },
    { name: 'SWIRL', values: { 'phaser.rate': 1, 'phaser.feedback': 0.6, 'phaser.center': 0.45, 'phaser.mix': 0.7 } },
    { name: 'NASAL', values: { 'phaser.rate': 0, 'phaser.feedback': 0.9, 'phaser.center': 0.75, 'phaser.mix': 0.8 } },
    { name: 'DEEP', values: { 'phaser.rate': 2, 'phaser.feedback': 0.75, 'phaser.center': 0.35, 'phaser.mix': 1 } },
  ],
  eq: [
    { name: 'FLAT', values: { 'eq.low': 0.5, 'eq.mid': 0.5, 'eq.freq': 0.56, 'eq.high': 0.5 } },
    { name: 'SCOOP', values: { 'eq.low': 0.6, 'eq.mid': 0.25, 'eq.freq': 0.55, 'eq.high': 0.6 } },
    { name: 'BITE', values: { 'eq.low': 0.45, 'eq.mid': 0.75, 'eq.freq': 0.7, 'eq.high': 0.55 } },
    { name: 'DARK', values: { 'eq.low': 0.55, 'eq.mid': 0.5, 'eq.freq': 0.56, 'eq.high': 0.2 } },
    { name: 'AIR', values: { 'eq.low': 0.45, 'eq.mid': 0.5, 'eq.freq': 0.56, 'eq.high': 0.8 } },
    { name: 'NO LOW', values: { 'eq.low': 0.1, 'eq.mid': 0.55, 'eq.freq': 0.5, 'eq.high': 0.5 } },
    { name: 'HONK', values: { 'eq.low': 0.5, 'eq.mid': 0.85, 'eq.freq': 0.45, 'eq.high': 0.4 } },
  ],
  // The unit and step length before the step count: the count is in them.
  delay: [
    { name: 'SLAP', values: { 'delay.unit': 0, 'delay.length': 0, 'delay.steps': 1, 'delay.style': 0, 'delay.feedback': 0.55, 'delay.mix': 0.2 } },
    { name: 'PING', values: { 'delay.unit': 0, 'delay.length': 2, 'delay.steps': 1, 'delay.style': 2, 'delay.feedback': 0.7, 'delay.mix': 0.25 } },
    { name: 'DOTTED', values: { 'delay.unit': 0, 'delay.length': 0, 'delay.steps': 3, 'delay.style': 1, 'delay.feedback': 0.7, 'delay.mix': 0.25 } },
    { name: 'DUB', values: { 'delay.unit': 0, 'delay.length': 2, 'delay.steps': 3, 'delay.style': 2, 'delay.feedback': 0.85, 'delay.mix': 0.3 } },
    { name: 'TRIPLET', values: { 'delay.unit': 0, 'delay.length': 1, 'delay.steps': 1, 'delay.style': 1, 'delay.feedback': 0.65, 'delay.mix': 0.22 } },
    { name: 'HALF', values: { 'delay.unit': 0, 'delay.length': 2, 'delay.steps': 4, 'delay.style': 2, 'delay.feedback': 0.6, 'delay.mix': 0.2 } },
    { name: 'DOUBLER', values: { 'delay.unit': 1, 'delay.ms': 30, 'delay.style': 1, 'delay.feedback': 0.5, 'delay.mix': 0.3 } },
  ],
  reverb: [
    { name: 'ROOM', values: { 'reverb.size': 0.3, 'reverb.decay': 0.3, 'reverb.mix': 0.18 } },
    { name: 'HALL', values: { 'reverb.size': 0.7, 'reverb.decay': 0.6, 'reverb.mix': 0.25 } },
    { name: 'HUGE', values: { 'reverb.size': 0.95, 'reverb.decay': 0.85, 'reverb.mix': 0.35 } },
    { name: 'PLATE', values: { 'reverb.size': 0.45, 'reverb.decay': 0.45, 'reverb.mix': 0.22 } },
    { name: 'SHORT', values: { 'reverb.size': 0.2, 'reverb.decay': 0.15, 'reverb.mix': 0.15 } },
    { name: 'CAVE', values: { 'reverb.size': 1, 'reverb.decay': 1, 'reverb.mix': 0.45 } },
  ],
};
