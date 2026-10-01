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

## Later: downloads for buyers only

**The design the site already describes** (checkout page, Phase 7): the
download is free and is the whole instrument; buying gets a licence key,
and the key turns on preset saving. Under that design the download buttons
stay public, and paying changes what the plugin can do, not whether you can
get it. It needs no change here: deploy the Worker (docs/backend.md), set
`-DGNARL_LICENCE_ENDPOINT` in the release build, and connect Stripe.

**If downloads should be for buyers only instead**, the files cannot stay on
GitHub Releases: a release on a public repository is public, and anyone with
the link downloads it whether the button is shown or not. Hiding a button
gates nothing. What it would take:

1. The zips go to private storage (Cloudflare R2, beside the licence Worker)
   instead of the release.
2. Stripe's payment link sends the buyer to a thank-you page
   (`thanks.html`) carrying the checkout session id.
3. The Worker checks that session with Stripe, issues the licence key, and
   answers with short-lived signed download links. The page shows the
   buttons only then.

Two things stay true whichever is chosen:
- The phone version is a web page and is public by nature. It can carry the
  same licence check as the plugin instead.
- GPLv3 lets any buyer pass GNARL on, and the source is public, so a
  download gate makes paying the easy path; it does not stop copying
  (CLAUDE.md §8).
