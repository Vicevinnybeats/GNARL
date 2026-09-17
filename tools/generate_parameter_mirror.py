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
}
sub = grab_singleton('sub')
noise = grab_singleton('noise')
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

emit_object('SUB', sub, 'Sub oscillator.')
emit_object('NOISE', noise, 'Noise generator.')

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
  | ValuesOf<(typeof FILTER)[number]>
  | ValuesOf<(typeof ENV)[number]>
  | ValuesOf<(typeof LFO)[number]>
  | ValuesOf<(typeof MOD)[number]>
  | (typeof MACRO)[number]
  | ValuesOf<typeof GLOBAL>;

/** Every parameter ID, flat. Useful for bulk relay setup and for tests. */
export const ALL_PARAMETER_IDS: readonly ParameterId[] = [
  ...OSC.flatMap((o) => Object.values(o)),
  ...Object.values(SUB),
  ...Object.values(NOISE),
  ...FILTER.flatMap((f) => Object.values(f)),
  ...ENV.flatMap((e) => Object.values(e)),
  ...LFO.flatMap((l) => Object.values(l)),
  ...MOD.flatMap((m) => Object.values(m)),
  ...MACRO,
  ...Object.values(GLOBAL),
];
''')

open(OUT, 'w').write("\n".join(out))

total = (sum(len(r) for r in families['osc']) + len(sub) + len(noise)
         + sum(len(r) for r in families['filter'])
         + sum(len(r) for r in families['envelope'])
         + sum(len(r) for r in families['lfo'])
         + sum(len(r) for r in families['modSlot'])
         + len(macros) + len(globals_))
print("mirrored ids:", total)
