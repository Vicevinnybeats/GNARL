# The FX chain: why it is not ten generic slots

A decision record. Phase 4 was blocked on this, and it had to be settled
*before* any parameter was declared, because **parameter IDs are frozen once
shipped** (CLAUDE.md §4) and this choice decides what they are.

## The problem

The scope asks for a **reorderable ten-slot FX rack**, where each slot can hold
any of the effect types (Distortion, EQ, Delay, Reverb,
Chorus/Flanger/Phaser, Filter, Hyper/Dimension, Limiter).

A host parameter is a number with a fixed ID and a fixed meaning. "Slot 3 can
be any effect" and "every parameter has a fixed meaning" are in direct
conflict, and something has to give.

## The options

**A — generic per-slot parameters.** Each slot declares a type plus N
anonymous parameters: `fx_slot3_type`, `fx_slot3_p1` … `fx_slot3_p8`. About
110 parameters.

Rejected. The host's automation lane reads "Slot 3 P4", which tells the
producer nothing, and — worse — **changing a slot's effect type silently
reinterprets every automation lane pointing into it**. A delay-time lane
becomes a reverb-size lane with the same numbers. That is precisely the failure
CLAUDE.md §4 rejects for mod destinations: "an index into an ordered list of
targets … shifts the moment the list changes, silently repointing every saved
preset's modulation at the wrong parameter." The same reasoning applies here
and reaches the same answer.

**B — one instance of each effect, reorderable order.** Every effect declares
its own named parameters once (`fx_reverb_size`, `fx_delay_time`), and only the
*order* of the chain is rearrangeable. About 80 parameters, every lane
meaningful and stable forever.

This is what Serum and Vital both do, and the scope explicitly says to match
Serum's information architecture. But it gives up **stacking** — no two
distortions in series, no two EQs — and stacking distortion is not an exotic
request in this genre, it is most of a riddim patch.

**C — a fixed roster with duplicates where the genre stacks them. Chosen.**

## What was chosen

A **fixed roster of effect instances**, each with its own named, permanently
meaningful parameters, and a **reorderable order** over that roster.

The roster duplicates exactly the effects that get stacked in practice:

| Instance | Why |
|---|---|
| `dist1`, `dist2` | Stacking drive is most of a riddim patch — one before the filter and one after is a standard move. |
| `eq1`, `eq2` | One to carve before distortion, one to fix what the distortion did. |
| `filter1`, `filter2` | An FX-slot filter is used both as a tone shaping stage and as a rhythmic gate; those want different settings at different points in the chain. |
| `delay`, `reverb`, `chorus`, `phaser`, `flanger`, `hyper`, `dimension`, `limiter` | One each. Two reverbs in series is a mistake, not a feature. |

Fourteen instances, not ten slots. The **order** is a permutation stored in the
ValueTree — *not* a parameter, for the same reason a mod destination is not one,
and because automating "effect order" is not a thing anyone does. It reaches
the audio thread through the same published-snapshot mechanism as the LFO
curves (`params::ModStateBridge`).

Each instance also gets `enabled` and `mix` as real parameters, because
switching an effect in and out and riding its wet level **are** things
producers automate.

## What this costs

You cannot have three distortions, or two reverbs. That is a real limitation
and it is the price of every automation lane being permanently meaningful.
The trade is deliberate: a producer who wants a third distortion can use the
two filter slots' own drive stages, or the oscillator warp, or OTT — a producer
whose automation silently repoints onto the wrong parameter after they change a
slot's type has lost work they cannot get back.

If the roster later needs another instance, **appending one is safe** — new
IDs, no existing ID changes meaning. That asymmetry is the whole point of
choosing this shape: it can grow, and option A could not be fixed.

## Consequences for the UI

The rack still *presents* as a reorderable list, because that is the right
interaction. It is a list of the fourteen instances in their current order,
each draggable, each with an enable and a mix — not a set of empty slots with
type pickers. An instance that is off sits in the list greyed rather than
vanishing, so the order stays stable as things are switched in and out.
