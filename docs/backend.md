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

## Stripe - built, waiting for an account

`backend/src/stripe.ts`, tested in `backend/test/stripe.test.ts` (18 cases,
in CI):

| Endpoint | Does |
|---|---|
| `POST /stripe/webhook` | Verifies Stripe's signature (HMAC-SHA256, 5-minute tolerance), then: a paid `checkout.session.completed` (or `async_payment_succeeded`) issues ONE key per checkout session however often Stripe delivers it; a full `charge.refunded` marks the licence refunded, so `/activate` then says no. Anything unsigned, mis-signed, stale or tampered: 400, nothing changes. Our own failure: 503, so Stripe retries rather than a key being lost |
| `GET /licence?session_id=cs_...` | The thank-you page's lookup: the key once the webhook has landed, `pending` (404) until then |

Keys look like `GNARL-7KQ3-M2XH-9TPA-WD4R`: 16 symbols from 32 that cannot
be misread (no 0/O, 1/I), 80 bits.

Negative controls, run:
- with signature checking switched off, four of the refusal tests fail;
- without the one-key-per-session guard, the triple-delivery test fails.

### Turning it on from GitHub alone (no wrangler)

1. Repository secret `CLOUDFLARE_API_TOKEN`; run the `backend` workflow;
   its log prints the Worker URL.
2. Stripe Payment Link and webhook as in 2-3 below, with the endpoint
   `<worker URL>/stripe/webhook`; its signing secret goes in the repository
   secret `STRIPE_WEBHOOK_SECRET`, and the `backend` workflow is run again
   (it hands the secret to the Worker).
3. Steps 4-5 below (the site's two strings, the plugin's endpoint) are
   repository edits.

### Turning it on (a person, once)

1. **Worker:** `cd backend && npm install && npx wrangler login`, then
   `npm run migrate:remote` (adds the two Stripe columns to the existing D1)
   and `npm run deploy`. Note the URL it prints.
2. **Stripe, Payment Link:** create a €49 product and a Payment Link. Under
   *After payment*, choose *Don't show confirmation page* and redirect to
   `https://gnarl.vercel.app/thanks.html?session_id={CHECKOUT_SESSION_ID}`.
3. **Stripe, webhook:** Developers → Webhooks → add the endpoint
   `<worker URL>/stripe/webhook` with the events
   `checkout.session.completed`, `checkout.session.async_payment_succeeded`
   and `charge.refunded`. Copy its signing secret, then
   `npx wrangler secret put STRIPE_WEBHOOK_SECRET`.
4. **Site:** paste the Payment Link into `data-payment-link` in
   `site/checkout.html`, and the Worker URL into the `gnarl-licence-api`
   meta tag in `site/thanks.html`.
5. **Plugin:** build releases with
   `-DGNARL_LICENCE_ENDPOINT=<worker URL>/activate` (release.yml's cmake
   lines). Until then every build says "Development build" and saves
   presets.

Test it end to end in Stripe's **test mode** first: the same steps with a
test-mode link and secret, and a card `4242 4242 4242 4242`.

Not built:
- **email:** the key is shown on the thank-you page, and Stripe's receipt
  goes to the buyer, but no email carries the key itself. Sending one needs
  a mail service (Resend, Postmark) and its API key as another secret.
- **disputes (chargebacks):** these do not revoke a key automatically. A
  person decides.
