# Phase 8-01: preset sync — one list of patches for the plugin and the phone

The producer works in FL Studio and on a phone. Until now a patch moved
between them as a file: EXPORT on the phone, rename `.vital.json` to
`.vital`, open it in the plugin. Phase 8 is a shared list: UPLOAD on one,
tap it on the other.

## No accounts: a sync code

`docs/backend.md` parked sync behind accounts, and Cloudflare has no
Supabase-Auth equivalent. It does not need one. A list is opened by a **sync
code**, `SYNC-XXXX-XXXX-XXXX-XXXX`: 16 symbols from the licence key's
32-symbol alphabet (no 0/O, no 1/I), 80 bits, beyond guessing. Whoever holds
the code reads and writes that list, exactly as the licence key is the
identity for licensing.

- It needs no payment and no email, so it works during the free beta.
- The database stores a **SHA-256 of the code**, never the code: a copy of
  the database opens nothing (test: "stores a hash of the code").
- Codes are typed by hand on a phone: case, spaces and dashes do not matter.
- Later, a paid licence key can open a list too; nothing here assumes it.

## The service (`backend/src/sync.ts`)

The licence Worker and its D1 database (`gnarl`) gain:

| Request | Does |
|---|---|
| `POST /sync/new` | A new, empty list: `{code}` (201). At most 10 per address per hour (the address is hashed and kept a day, only to count) |
| `GET /sync` | The list: `{presets: [{name, updated_at, size}]}` |
| `GET /sync/p/NAME` | One patch: `{name, patch, updated_at}` |
| `PUT /sync/p/NAME` `{patch}` | Save or replace; the patch must be a .vital JSON object with `settings` |
| `DELETE /sync/p/NAME` | Remove |

The code travels as `Authorization: Bearer SYNC-...`. No code: 401; an
unknown one: 404; a database fault: 503, never "unknown" (the licence
service's rule, for the same reason: our failure must not look like the
user's). Bounds, because anyone may ask for a code: a patch at most 1 MB
(GNARL's own run 180–340 kB; Sig Wob 1 is 338 kB), a name 1–64 characters,
a list at most 300 patches. CORS is `*`: the callers are the phone page,
from wherever it is opened, and the plugin's web view, whose origin is a
custom scheme (`juce://juce.backend` on a Mac); nothing rides on cookies.

Tables: `sync_spaces` (hashed code), `sync_presets` (space, name, patch,
size, updated_at), `sync_rate`. In `schema.sql`, and
`migrations/0003_sync.sql` for the existing database; both migrations are
applied to the live `gnarl` database (2026-10-02).

## The panel (`ui/src/sync.ts`, the CLOUD section of the preset sheet)

Without a code: NEW CODE, or USE CODE (type one). With one: the code (tap
to copy), UPLOAD the current patch under its name, the list (tap to load,
× to delete), FORGET (this device only; the list stays). The code is kept in
the page's `localStorage`.

- **Phone**: the patch is the web engine's own save; loading checks the
  version as OPEN FILE does.
- **Plugin**: the page asks the plugin for the current patch with
  `gnarlPresetSave` — new in `web_panel.cpp`, answered with the same JSON a
  DAW saves with a project — and loads one as a starting sound
  (`gnarlPresetFactory {name, patch}`, through `setStateInformation`).
- **The licence's one gate** (`presetSavingAllowed`) closes UPLOAD in the
  plugin as it closes saving to disk; loading stays open.

**Off until configured**, like the licence check: the server's address is
baked in at build time (`VITE_GNARL_SYNC_URL`, from the repository variable
`GNARL_BACKEND_URL` in the release workflow). Without one the sheet shows
no CLOUD and the page makes no request.

## Deploying (`.github/workflows/backend.yml`)

Run by hand from the Actions tab. It needs the repository secret
`CLOUDFLARE_API_TOKEN` (and `CLOUDFLARE_ACCOUNT_ID` if the token sees more
than one account); it runs the tests, migrates the database, deploys, and
prints the address and its `/health`. Then set `GNARL_BACKEND_URL` to that
address and make a release.

## Tests

- `backend/test/sync.test.ts` — 16 cases in workerd against a real local D1:
  codes (format, hash only, sloppy typing, the SYNC-prefix edge, the rate
  limit), lists kept apart, save/list/read/delete, replace, refusals (not a
  patch, too large, bad names, a full list), CORS preflight from
  `juce://juce.backend`, 503 on a database fault. With the hash removed, the
  hash test fails.
- `ui/tests/sync.test.mjs` — two phones in Chromium against a stand-in
  server on another origin: A makes a code and uploads; B types the code
  sloppily, sees the patch and loads it; FORGET deletes nothing; an unknown
  and a malformed code are refused in words; a delete; the server gone.
  And a page with no server shows no CLOUD and makes no request.
- By hand, `wrangler dev` with a local D1: a new code, Sig Wob 1 (338 kB)
  uploaded, listed and read back byte for byte.

## Not yet

- Conflict handling is last-write-wins per name: one producer, two devices.
- In the plugin the code lives in the web view's storage; if a host clears
  it, USE CODE again.
- No automatic sync: UPLOAD is a deliberate act, as SAVE is.
- Not played on a real phone or in FL Studio.
