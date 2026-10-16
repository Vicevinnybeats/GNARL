# Releases and downloads

## Making one

GitHub → Actions → **release** → *Run workflow*, with a tag such as
`v1.0.6-beta.1`. (Pushing a tag `v*` does the same.)
`.github/workflows/release.yml` then:

1. builds the JUCE 8 VST3 and standalone on Windows, and the universal VST3,
   AU and standalone on macOS, exactly as `build.yml` does;
2. builds the phone version: the engine in WebAssembly, inside the panel's
   page (docs/design/phase2-09-mobile.md);
3. publishes a GitHub Release with three files:

| File | What it is |
|---|---|
| `GNARL-Windows.zip` | `GNARL.vst3`, `GNARL.exe`, `README.txt`, `LICENSE.txt` |
| `GNARL-macOS.zip` | `GNARL.vst3`, `GNARL.component`, `GNARL.app`, `README.txt`, `LICENSE.txt` |
| `gnarl-web.html` | the phone version |

The READMEs and the release notes are in `docs/release/`.

## How the website uses them

- **Windows and macOS buttons** (`site/index.html`, the DOWNLOAD section)
  link to `https://github.com/Vicevinnybeats/GNARL/releases/latest/download/<file>`.
  GitHub resolves `latest` to the newest release that is not a pre-release,
  which is why the workflow publishes with `--latest` and never as a
  pre-release. A new release changes what the buttons download; the site
  needs no edit.
- **The phone button** opens `/app/`. `tools/assemble_deploy.mjs` fetches
  `gnarl-web.html` from the latest release on every site deploy and serves
  it there. Before the first release, or if GitHub cannot be reached, `/app`
  stays the old tombstone page and the deploy log says so. **A new release
  reaches `/app` on the site's next deploy**, not by itself: redeploy the
  site, or push any commit, after a release.

Releases so far: `v1.0.6-beta.1` (first), `beta.2` (preset sheet closes,
starting sounds), `beta.3` (effects rack), `beta.4` (the delay line). Each
was followed by a push so the site redeployed with it.

**The file names are a contract** between the workflow, the site and the
deploy script. Rename one in all three places or not at all.

## Not signed yet

Neither platform's build is code-signed:
- Windows SmartScreen may warn ("More info → Run anyway").
- macOS blocks unsigned plugins until the quarantine flag is removed; the
  macOS README gives the one Terminal command.

Signing needs an Apple Developer account (notarisation) and a Windows
code-signing certificate, both paid, and both a step for the 1.0 release.

## GPLv3

Every download is GPLv3 software, so each one says so and points at the
source (README, release notes, the phone page's start screen). The source
for a release is the tag it was built from, in this public repository.

## Downloads for buyers only (built 2026-10-16)

A release on a public repository is public, so the buyer's zips live in a
**private Cloudflare R2 bucket**, `gnarl-downloads`, beside the licence
Worker:

1. `release.yml` uploads each release's two zips into the bucket (when the
   `CLOUDFLARE_API_TOKEN` secret exists and R2 is on).
2. The Worker's `GET /download?os=windows|macos&session_id=cs_...` (the
   thank-you page) or `&key=GNARL-...` (the key's email) streams the zip,
   only for an **active** licence (backend/src/downloads.ts). A refunded or
   revoked one gets nothing.
3. `thanks.html` swaps its buttons for those links when `/licence` says
   `downloads: true`; the email carries them too.

**To switch it on:** turn on R2 in the Cloudflare dashboard, run the backend
workflow (it creates and binds the bucket), then make a release (it fills
the bucket). **To make the zips buyers-only**, set the repository variable
`GNARL_PUBLIC_DOWNLOADS` to `false`: the next release is created without
them. Do that together with changing the site's WINDOWS / MACOS buttons,
which point at the public release, to the shop.

Two things stay true whichever is chosen:
- The phone version is a web page and is public by nature. It can carry the
  same licence check as the plugin instead.
- GPLv3 lets any buyer pass GNARL on, and the source is public, so a
  download gate makes paying the easy path; it does not stop copying
  (CLAUDE.md §8).
