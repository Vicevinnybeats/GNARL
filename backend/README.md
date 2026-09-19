# GNARL backend — licence activation

A Cloudflare Worker and a D1 database. That is the whole licensing service.

```
backend/
├── src/index.ts      # the Worker: /activate, /health
├── schema.sql        # D1 schema
├── test/             # 15 tests, run in workerd against a real local D1
├── wrangler.toml
└── vitest.config.ts
```

---

## Deploy it

You need a Cloudflare account and `wrangler` logged in. Everything here fits
inside the free tier — Workers allow 100k requests a day, and this service
handles a few per customer per day.

**The D1 database already exists** (`gnarl`, WEUR, id in `wrangler.toml`)
and its tables are created, so deploying is two commands:

```bash
cd backend
npm install
npx wrangler login
npm run deploy
```

To rebuild the database from scratch instead:

```bash
npx wrangler d1 create gnarl     # paste the new id into wrangler.toml
npm run schema:remote
```

`wrangler deploy` prints the URL. Check it:

```bash
curl https://gnarl-licence.<your-subdomain>.workers.dev/health
# {"ok":true}
```

## Point the plugin at it

Enforcement is off until an endpoint is configured, and configuring one is
the single change that turns it on (CLAUDE.md §9):

```bash
cmake -B build -DGNARL_LICENCE_ENDPOINT=https://gnarl-licence.<sub>.workers.dev/activate
```

With it empty — the default — the plugin reports `Status::unenforced` and
its banner reads "Development build".

## Run the tests

```bash
npm test
```

They run in **workerd**, the real runtime, against a **real local D1** — not
a mock. No network, no account needed, so they run in CI and in a sandbox.

## The design, in three points

**The plugin carries no credential for this service, and must not.** A key
embedded in a downloadable binary is a key shipped to everyone the moment
somebody runs `strings` on it. The licence key *is* the credential and it is
checked here. That makes the endpoint publicly callable, which is the right
trade: it answers one question about one key and holds nothing worth taking.

**Three answers, not two.** `valid` and `rejected` are decisions. Anything
else — a 503, a timeout, a dropped connection — is *not an answer*, and
`LicenseManager` treats it as `unreachable`, which opens the 30-day grace
period. So this service **never returns `rejected` because something broke on
our side**. When the database fails it says 503 and the customer stays inside
grace. Being unreachable is safe; being wrong is not. Two tests exist purely
to hold that line, and they fail if the error path is changed to return a
decision.

**The unique constraint on `(license_id, machine_id)` is load-bearing.** The
plugin re-checks periodically. Without it, a customer checking in daily would
exhaust a three-machine licence in three days and be told their licence is
bad — for using the product normally. A re-check updates `last_seen` instead
of taking a seat.

## Test key

`GNARL-TEST-0000-0000-0000` already exists with a limit of 2 activations.
Once deployed:

```bash
U=https://gnarl-licence.<your-subdomain>.workers.dev/activate
p() { echo "--- $1"; curl -s -X POST "$U" \
        -H 'content-type: application/json' -d "$2" -w '  [HTTP %{http_code}]\n'; }

p "unknown key"        '{"key":"GNARL-NOPE","machineId":"m1"}'
p "valid, machine 1"   '{"key":"GNARL-TEST-0000-0000-0000","machineId":"m1","machineLabel":"Studio PC"}'
p "same machine again" '{"key":"GNARL-TEST-0000-0000-0000","machineId":"m1"}'
p "machine 2 of 2"     '{"key":"GNARL-TEST-0000-0000-0000","machineId":"m2"}'
p "machine 3, over"    '{"key":"GNARL-TEST-0000-0000-0000","machineId":"m3"}'
```

Expected: `rejected`/unknown key · `valid`/new activation ·
`valid`/known machine · `valid`/new activation ·
`rejected`/activation limit reached.

Reset between runs with
`npx wrangler d1 execute gnarl --remote --command "delete from activations"`.

**Delete the test key before launch** — `docs/release-process.md` has the
checklist item.

## Seeding a real key by hand

Until Stripe is wired up:

```bash
npx wrangler d1 execute gnarl --remote --command \
  "insert into licenses (id, key, email, status, max_activations)
   values (lower(hex(randomblob(16))), 'GNARL-XXXX-XXXX-XXXX', 'you@example.com', 'active', 3)"
```

## Not built yet

- The Stripe webhook that creates a licence row on purchase.
- Licence key generation: format, entropy, checksum.
- A way for a customer to deactivate a machine they no longer own.
- **Rate limiting.** The endpoint is public and unauthenticated by design, so
  key entropy is currently doing work a rate limiter should share. Cloudflare
  has this built in (`[[unsafe.bindings]]` rate limiting, or a WAF rule) —
  worth turning on before launch.
