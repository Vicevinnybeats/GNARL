# Backend — licensing

Phase 7's server side. What exists, where it lives, and how to check it is
working.

---

## What is deployed

| | |
|---|---|
| Supabase project | `GNARL` — ref `irkzzbxnihighzvzoxdy`, region `eu-west-1` |
| Activation endpoint | `https://irkzzbxnihighzvzoxdy.supabase.co/functions/v1/activate` |
| Tables | `public.licenses`, `public.activations` |
| Stripe | not yet connected |

The project ref is in every request URL and is not a secret. **Keys are
not in this file and must not be.** The service role key lives only in the
edge function's environment, which Supabase injects; nothing else ever holds
it.

## Why the plugin holds no Supabase credential

`verify_jwt` is **off** on the `activate` function, deliberately.

The alternative is embedding an anon key in the binary, and a key shipped
inside a downloadable plugin is a key shipped to everyone — it can be pulled
out with `strings`. The licence key **is** the credential, and it is checked
server-side. The endpoint being publicly callable is the correct trade: it
answers one question about one key, and it holds nothing worth taking.

## The schema

`licenses` — one row per purchase. `key` is the identity and is unique.
`status` is `active` / `revoked` / `refunded` / `expired`, kept distinct
rather than collapsed to a boolean because they mean different things to
support. `max_activations` defaults to 3: a desktop, a laptop, and the one
they buy next year.

`activations` — one row per machine per licence, with a **unique constraint
on `(license_id, machine_id)`**. That constraint is load-bearing: the plugin
re-checks periodically, and without it a customer checking in daily would
exhaust a three-machine licence in three days. `machine_id` is a hash; the
server cannot tell whose machine it is and does not need to.

**RLS is enabled on both tables with no policies**, which denies anon and
authenticated everything. The Supabase linter reports this as
`rls_enabled_no_policy` at INFO level — that is the intent, not an oversight.
Every read and write goes through the service role inside the edge function,
which is the only place the activation count can be enforced atomically
rather than suggested.

## Three answers, not two

The endpoint returns `{"status": "valid" | "rejected", "reason": "..."}`.

Anything else — a 503, a timeout, a dropped connection — is **not an answer**,
and `LicenseManager` treats it as `unreachable`, which opens the 30-day grace
period. That distinction is the whole reason a producer with no wifi keeps
working, so the function **never returns `rejected` for an internal failure**.
A failed database lookup, or an insert that loses a race on the unique
constraint, returns 503: the customer stays inside grace rather than being
told their licence is bad because of our fault.

## Checking it

The build sandbox's egress proxy blocks `*.supabase.co`, so these cannot be
run from a Claude Code session — run them from your own machine.

```bash
U=https://irkzzbxnihighzvzoxdy.supabase.co/functions/v1/activate
p() { echo "--- $1"; curl -s -X POST "$U" \
        -H 'Content-Type: application/json' -d "$2" -w '  [HTTP %{http_code}]\n'; }

p "unknown key"        '{"key":"GNARL-NOPE","machineId":"m1"}'
p "valid, machine 1"   '{"key":"GNARL-TEST-0000-0000-0000","machineId":"m1","machineLabel":"Studio PC"}'
p "same machine again" '{"key":"GNARL-TEST-0000-0000-0000","machineId":"m1"}'
p "machine 2 of 2"     '{"key":"GNARL-TEST-0000-0000-0000","machineId":"m2"}'
p "machine 3, over"    '{"key":"GNARL-TEST-0000-0000-0000","machineId":"m3"}'
p "missing machineId"  '{"key":"GNARL-TEST-0000-0000-0000"}'
```

Expected, in order: `rejected`/unknown key · `valid`/new activation ·
`valid`/known machine · `valid`/new activation ·
`rejected`/activation limit reached · HTTP 400.

`GNARL-TEST-0000-0000-0000` is a test key with a limit of 2. **Delete it
before launch** — `docs/release-process.md` has the checklist item.

To reset it between runs:

```sql
delete from public.activations;
```

### What has actually been verified

Against the real schema, through SQL: the unique constraint rejects a
re-check on a known machine (the property the daily check-in depends on),
two activations fit a limit of two, and deleting a licence cascades to its
activations.

**The HTTP hop has not been exercised from here** — the sandbox cannot reach
it. Run the block above once from your own machine before wiring the plugin
to it.

## Pointing the plugin at it

Enforcement is off until an endpoint is configured, and configuring one is
the single change that turns it on:

```bash
cmake -B build -DGNARL_LICENCE_ENDPOINT=https://irkzzbxnihighzvzoxdy.supabase.co/functions/v1/activate
```

With it empty (the default) the plugin reports `Status::unenforced` and its
banner reads "Development build". See CLAUDE.md section 9.

`LicenseManager` still needs its real verifier wired to this URL — the
constructor currently calls `verify()` with the default verifier when an
endpoint is set. That is the next piece of work on this side.

## Not built yet

- The Stripe webhook that creates a `licenses` row on purchase.
- Licence key generation (format, entropy, checksum).
- A customer-facing way to deactivate a machine.
- **Rate limiting.** The endpoint is public and unauthenticated, so a
  high-entropy key format is doing the work that a rate limiter should also
  be doing. Worth adding before launch.
