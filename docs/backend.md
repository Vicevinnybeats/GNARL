# Backend — licence activation

**Cloudflare Workers + D1.** The service itself, its schema and its tests
live in [`backend/`](../backend/) — see
[`backend/README.md`](../backend/README.md) for how to deploy it and what
the design is.

This file records the decision and what is still parked.

---

## Why Cloudflare and not Supabase

Supabase was built first and worked: schema, an edge function, the same
three-answer contract. It was dropped for one reason that has nothing to do
with the code — **Supabase's free tier allows two active projects per user**,
and taking a third meant pausing a live app. Cloudflare's free tier has no
project cap, so the flashcards project came back.

Two things followed from the move, one of them better than expected:

- **The tests are real.** Workers run under `workerd` locally with a real
  local D1, so `npm test` in `backend/` exercises the actual HTTP handler and
  the actual SQL. The Supabase edge function could never be tested from a
  build sandbox at all — the egress proxy denies `*.supabase.co`, so the best
  that could be done was to run the same queries against the schema by hand
  and hand over a curl block.
- **Deployment is manual.** There is no Cloudflare connector in a Claude Code
  session, so `wrangler deploy` is run by a person. The Supabase version
  could be deployed from here; that convenience is what was traded away, and
  it is worth less than being able to test.

The one genuine gap is **auth**: Cloudflare has no Supabase-Auth equivalent,
so user accounts would mean rolling them on D1 or adding Clerk/Auth.js.
Licensing does not need accounts — the key is the identity — so this does not
bite until preset sync, which is Phase 8.

## The Supabase project

`GNARL`, ref `irkzzbxnihighzvzoxdy`, still exists and still works. **Delete
it once the Worker is deployed and verified**, and unpause
`Vicevinnybeats's Project` at the same time. Keeping both running costs
nothing, but keeping both *maintained* would mean two copies of the licence
policy, which is exactly the kind of duplication that drifts.

## Stripe

Not connected yet, on either. What it needs to do: on `checkout.session.completed`,
generate a licence key and insert a `licenses` row. The webhook signature
must be verified — unlike `/activate`, that endpoint has to be trusted.
