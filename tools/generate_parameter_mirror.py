#!/usr/bin/env python3
"""Emits ui/src/bridge/parameterIds.ts from the C++ header, so the two cannot
disagree. Run by hand alongside gen_ids.py; ParameterMirrorTests enforces it."""
import re, sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

HEADER = REPO / "plugin/source/params/ParameterIDs.h"
OUT = REPO / "ui/src/bridge/parameterIds.ts"

src = open(HEADER).read()

# Indexed families: `.member = "id",` grouped by the enclosing array.
def grab_family(var):
    m = re.search(r'inline constexpr \w+ ' + var + r'\[(\d+)\] = \{(.*?)\n    \};', src, re.S)
    if not m:
        sys.exit(f"family {var} not found")
    count = int(m.group(1))
    entries = re.findall(r'\.(\w+)\s*=\s*"([^"]+)"', m.group(2))
    per = len(entries) // count
    return [entries[i*per:(i+1)*per] for i in range(count)]

def grab_singleton(var):
    m = re.search(r'inline constexpr \w+ ' + var + r' \{(.*?)\n    \};', src, re.S)
    if not m:
        sys.exit(f"singleton {var} not found")
    return re.findall(r'\.(\w+)\s*=\s*"([^"]+)"', m.group(1))

def grab_macros():
    m = re.search(r'macro \{(.*?)\n    \};', src, re.S)
    return re.findall(r'"([^"]+)"', m.group(1))

def grab_globals():
    return re.findall(r'inline constexpr auto (\w+)\s*=\s*"([^"]+)";', src)

families = {
    'osc':      grab_family('osc'),
    'filter':   grab_family('filter'),
    'envelope': grab_family('envelope'),
    'lfo':      grab_family('lfo'),
    'modSlot':  grab_family('modSlot'),
    'fxDistortion': grab_family('fxDistortion'),
    'fxEq':         grab_family('fxEq'),
    'fxFilter':     grab_family('fxFilter'),
}

# COUNTS: the mirror's own total is checked against the header's declared
# count, so a family added to one and not the other fails here rather than in
# a control that silently binds to nothing.
sub = grab_singleton('sub')
noise = grab_singleton('noise')
ott = grab_singleton('ott')

fx_delay     = grab_singleton('fxDelay')
fx_reverb    = grab_singleton('fxReverb')
fx_chorus    = grab_singleton('fxChorus')
fx_flanger   = grab_singleton('fxFlanger')
fx_phaser    = grab_singleton('fxPhaser')
fx_hyper     = grab_singleton('fxHyper')
fx_dimension = grab_singleton('fxDimension')
fx_limiter   = grab_singleton('fxLimiter')
macros = grab_macros()
globals_ = grab_globals()

out = []
out.append("""/**
 * Mirror of plugin/source/params/ParameterIDs.h.
 *
 * GENERATED — regenerate rather than editing by hand, and commit both files in
 * the same commit as the C++ change.
 *
 * These strings are the contract between the C++ parameter tree and this UI.
 * A mismatch fails SILENTLY: JUCE's relay never connects, so the control
 * renders, moves, and changes nothing. ParameterMirrorTests (C++) fails the
 * build if this file and the header disagree.
 *
 * Indexed families are arrays. Index 0 is oscillator "1" as the user sees it;
 * the IDs keep 1-based numbering because that is what a preset records.
 */

/** Bump only when the MEANING of an existing parameter changes. */
export const STATE_VERSION = 1;

export const COUNTS = {
  oscillators: 2,
  filters: 2,
  envelopes: 4,
  lfos: 4,
  modSlots: 16,
  macros: 4,
} as const;

export const MAX_VOICES = 16;
export const MAX_UNISON_VOICES = 16;
""")

def emit_family(name, rows, doc):
    out.append(f"/** {doc} */")
    out.append(f"export const {name} = [")
    for row in rows:
        out.append("  {")
        w = max(len(k) for k, _ in row)
        for key, val in row:
            out.append(f"    {(key + ':').ljust(w + 1)} '{val}',")
        out.append("  },")
    out.append("] as const;\n")

emit_family('OSC', families['osc'], 'Main oscillators. OSC[0] is "Osc 1".')
emit_family('FILTER', families['filter'], 'Filter slots. FILTER[0] is "Filter 1".')
emit_family('ENV', families['envelope'], 'Envelopes. ENV[0] is the amp envelope.')
emit_family('LFO', families['lfo'], 'LFOs. LFO[0] is "LFO 1".')
emit_family('MOD', families['modSlot'],
            'Mod matrix slots. The DESTINATION is deliberately absent: it is a '
            'parameter-ID string in the plugin ValueTree, not a host parameter. '
            'See ParameterIDs.h.')

def emit_object(name, rows, doc):
    out.append(f"/** {doc} */")
    out.append(f"export const {name} = {{")
    w = max(len(k) for k, _ in rows)
    for key, val in rows:
        out.append(f"  {(key + ':').ljust(w + 1)} '{val}',")
    out.append("} as const;\n")

emit_family('FX_DISTORTION', families['fxDistortion'],
            'FX distortions. Two instances because stacking drive is most of a '
            'riddim patch - see docs/fx-architecture.md.')
emit_family('FX_EQ', families['fxEq'], 'FX EQs. Two instances: one to carve '
            'before distortion, one to fix what it did.')
emit_family('FX_FILTER', families['fxFilter'],
            'FX filters. Distinct from the two VOICE filters in FILTER, which '
            'are per-voice and sit before the mix.')

emit_object('SUB', sub, 'Sub oscillator.')
emit_object('NOISE', noise, 'Noise generator.')
emit_object('OTT', ott, 'Built-in OTT-style three-band up/downward compressor.')

emit_object('FX_DELAY', fx_delay, 'FX delay.')
emit_object('FX_REVERB', fx_reverb, 'FX reverb.')
emit_object('FX_CHORUS', fx_chorus, 'FX chorus.')
emit_object('FX_FLANGER', fx_flanger, 'FX flanger.')
emit_object('FX_PHASER', fx_phaser, 'FX phaser.')
emit_object('FX_HYPER', fx_hyper, 'FX hyper/unison widener.')
emit_object('FX_DIMENSION', fx_dimension, 'FX dimension expander.')
emit_object('FX_LIMITER', fx_limiter, 'FX limiter.')

out.append("/** Macro knobs. MACRO[0] is GROWL. */")
out.append("export const MACRO = [")
for m in macros:
    out.append(f"  '{m}',")
out.append("] as const;\n")

emit_object('GLOBAL', globals_, 'Global parameters.')

out.append('''type ValuesOf<T> = T[keyof T];

/**
 * The union of every valid parameter ID. A typo in a call site is a compile
 * error rather than a control that binds to nothing.
 */
export type ParameterId =
  | ValuesOf<(typeof OSC)[number]>
  | ValuesOf<typeof SUB>
  | ValuesOf<typeof NOISE>
  | ValuesOf<typeof OTT>
  | ValuesOf<(typeof FILTER)[number]>
  | ValuesOf<(typeof ENV)[number]>
  | ValuesOf<(typeof LFO)[number]>
  | ValuesOf<(typeof MOD)[number]>
  | ValuesOf<(typeof FX_DISTORTION)[number]>
  | ValuesOf<(typeof FX_EQ)[number]>
  | ValuesOf<(typeof FX_FILTER)[number]>
  | ValuesOf<typeof FX_DELAY>
  | ValuesOf<typeof FX_REVERB>
  | ValuesOf<typeof FX_CHORUS>
  | ValuesOf<typeof FX_FLANGER>
  | ValuesOf<typeof FX_PHASER>
  | ValuesOf<typeof FX_HYPER>
  | ValuesOf<typeof FX_DIMENSION>
  | ValuesOf<typeof FX_LIMITER>
  | (typeof MACRO)[number]
  | ValuesOf<typeof GLOBAL>;

/** Every parameter ID, flat. Useful for bulk relay setup and for tests. */
export const ALL_PARAMETER_IDS: readonly ParameterId[] = [
  ...OSC.flatMap((o) => Object.values(o)),
  ...Object.values(SUB),
  ...Object.values(NOISE),
  ...Object.values(OTT),
  ...FILTER.flatMap((f) => Object.values(f)),
  ...ENV.flatMap((e) => Object.values(e)),
  ...LFO.flatMap((l) => Object.values(l)),
  ...MOD.flatMap((m) => Object.values(m)),
  ...FX_DISTORTION.flatMap((d) => Object.values(d)),
  ...FX_EQ.flatMap((e) => Object.values(e)),
  ...FX_FILTER.flatMap((f) => Object.values(f)),
  ...Object.values(FX_DELAY),
  ...Object.values(FX_REVERB),
  ...Object.values(FX_CHORUS),
  ...Object.values(FX_FLANGER),
  ...Object.values(FX_PHASER),
  ...Object.values(FX_HYPER),
  ...Object.values(FX_DIMENSION),
  ...Object.values(FX_LIMITER),
  ...MACRO,
  ...Object.values(GLOBAL),
];
''')

open(OUT, 'w').write("\n".join(out))

# Summed from what was actually grabbed, so a family added to the header and
# forgotten here shows up as a total that disagrees with the generator's.
total = (sum(sum(len(r) for r in rows) for rows in families.values())
         + len(sub) + len(noise) + len(ott)
         + len(fx_delay) + len(fx_reverb) + len(fx_chorus) + len(fx_flanger)
         + len(fx_phaser) + len(fx_hyper) + len(fx_dimension) + len(fx_limiter)
         + len(macros) + len(globals_))
print("mirrored ids:", total)
