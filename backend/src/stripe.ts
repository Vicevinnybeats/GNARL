/*
    Payments: Stripe tells this Worker that somebody paid, and the Worker
    issues their licence key (docs/backend.md, docs/release.md).

    THE WEBHOOK IS THE ONE ENDPOINT THAT MUST BE TRUSTED. /activate answers a
    question about a key anybody may ask; this one CREATES keys, so every
    request is checked against Stripe's signature before anything is read.
    An unsigned or stale request is refused with 400 and changes nothing.

    IDEMPOTENT BY CONSTRUCTION. Stripe delivers at least once and retries
    for days, so the same `checkout.session.completed` can arrive many
    times. The licence row is keyed on the checkout session (a unique
    column) and inserted with `on conflict do nothing`: a repeat finds the
    key it already made. One payment, one key, however often Stripe calls.

    Our faults are 5xx, never a quiet 200: Stripe retries a 5xx, so a
    database hiccup delays a key instead of losing it.
*/

import type { Env } from "./index";

/** Five minutes, Stripe's own default tolerance: older is a replay. */
export const SIGNATURE_TOLERANCE_SECONDS = 300;

const encoder = new TextEncoder();

function hex(buffer: ArrayBuffer): string {
    return [...new Uint8Array(buffer)].map((b) => b.toString(16).padStart(2, "0")).join("");
}

/** Constant-time string comparison, so a timing attack learns nothing. */
function equal(a: string, b: string): boolean {
    if (a.length !== b.length) return false;
    let diff = 0;
    for (let i = 0; i < a.length; i++) diff |= a.charCodeAt(i) ^ b.charCodeAt(i);
    return diff === 0;
}

/**
    Stripe's `Stripe-Signature` header: `t=<unix seconds>,v1=<hex>[,v1=...]`.
    The signature is HMAC-SHA256 over `<t>.<raw body>` with the endpoint's
    signing secret. Any v1 may match (Stripe sends two while a secret rolls).
*/
export async function verifyStripeSignature(
    body: string,
    header: string | null,
    secret: string,
    nowSeconds = Math.floor(Date.now() / 1000),
): Promise<boolean> {
    if (!header || !secret) return false;

    let timestamp = "";
    const signatures: string[] = [];
    for (const part of header.split(",")) {
        const [name, value] = part.split("=", 2);
        if (name === "t" && value) timestamp = value;
        else if (name === "v1" && value) signatures.push(value);
    }

    const t = Number(timestamp);
    if (!Number.isFinite(t) || signatures.length === 0) return false;
    if (Math.abs(nowSeconds - t) > SIGNATURE_TOLERANCE_SECONDS) return false;

    const key = await crypto.subtle.importKey(
        "raw",
        encoder.encode(secret),
        { name: "HMAC", hash: "SHA-256" },
        false,
        ["sign"],
    );
    const expected = hex(await crypto.subtle.sign("HMAC", key, encoder.encode(`${timestamp}.${body}`)));
    return signatures.some((signature) => equal(signature, expected));
}

/*  The key a customer types or pastes: GNARL-XXXX-XXXX-XXXX-XXXX.
    Thirty-two symbols - the digits 2-9 and every letter but I and O - so it
    survives being read aloud or copied by hand (no 0/O, no 1/I); 16 of them
    are 80 bits of randomness, beyond guessing. 32 divides 256, so a random
    byte modulo 32 picks each symbol with equal chance. */
export const KEY_SYMBOLS = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";

export function generateKey(): string {
    const bytes = crypto.getRandomValues(new Uint8Array(16));
    const symbols = [...bytes].map((b) => KEY_SYMBOLS[b % KEY_SYMBOLS.length]);
    const groups = [0, 4, 8, 12].map((i) => symbols.slice(i, i + 4).join(""));
    return `GNARL-${groups.join("-")}`;
}

interface StripeEvent {
    id?: string;
    type?: string;
    data?: { object?: Record<string, unknown> };
}

const json = (body: unknown, status = 200): Response =>
    new Response(JSON.stringify(body), { status, headers: { "content-type": "application/json" } });

/** POST /stripe/webhook */
export async function stripeWebhook(request: Request, env: Env): Promise<Response> {
    // The RAW body: the signature is over these exact bytes, and parsing and
    // re-serialising would change them.
    const body = await request.text();

    if (!env.STRIPE_WEBHOOK_SECRET) {
        // Not configured: refuse rather than accept unsigned events. A 500
        // makes Stripe retry, so nothing is lost while the secret is set.
        console.error("STRIPE_WEBHOOK_SECRET is not set");
        return json({ error: "webhook not configured" }, 500);
    }
    if (!(await verifyStripeSignature(body, request.headers.get("stripe-signature"), env.STRIPE_WEBHOOK_SECRET))) {
        return json({ error: "bad signature" }, 400);
    }

    let event: StripeEvent;
    try {
        event = JSON.parse(body);
    } catch {
        return json({ error: "malformed body" }, 400);
    }
    const object = event.data?.object ?? {};

    try {
        switch (event.type) {
            // A card payment completes here; a delayed method (bank transfer)
            // completes in the async event, with the same session object.
            case "checkout.session.completed":
            case "checkout.session.async_payment_succeeded": {
                if (object.payment_status !== "paid") return json({ ignored: "not paid yet" });
                await issueLicence(env, object);
                return json({ ok: true });
            }
            // A full refund ends the licence. A partial one (a discount
            // given after the fact) does not: the customer still paid.
            case "charge.refunded": {
                if (object.refunded !== true || typeof object.payment_intent !== "string") {
                    return json({ ignored: "partial refund" });
                }
                await env.DB.prepare("update licenses set status = 'refunded' where stripe_payment_intent = ?")
                    .bind(object.payment_intent)
                    .run();
                return json({ ok: true });
            }
            default:
                // Every other event Stripe may be configured to send: received,
                // nothing to do. A 2xx stops Stripe retrying it.
                return json({ ignored: event.type ?? "unknown" });
        }
    } catch (error) {
        console.error("webhook failed", error);
        return json({ error: "temporarily unavailable" }, 503);
    }
}

async function issueLicence(env: Env, session: Record<string, unknown>): Promise<void> {
    const sessionId = typeof session.id === "string" ? session.id : "";
    if (!sessionId) throw new Error("checkout session without an id");
    const details = (session.customer_details ?? {}) as { email?: unknown };
    const email = typeof details.email === "string" ? details.email : null;
    const customer = typeof session.customer === "string" ? session.customer : null;
    const paymentIntent = typeof session.payment_intent === "string" ? session.payment_intent : null;

    await env.DB.prepare(
        `insert into licenses (id, key, email, stripe_customer_id, stripe_session_id, stripe_payment_intent)
         values (?, ?, ?, ?, ?, ?)
         on conflict (stripe_session_id) do nothing`,
    )
        .bind(crypto.randomUUID(), generateKey(), email, customer, sessionId, paymentIntent)
        .run();
}

/**
    GET /licence?session_id=cs_...

    The thank-you page Stripe returns the buyer to asks for their key here.
    The checkout session id is the credential: Stripe makes it unguessable
    and gives it only to the buyer's browser, in the return URL. The webhook
    can land a moment after the buyer does, so "not yet" is a 404 the page
    polls on, not an error.
*/
export async function licenceForSession(request: Request, env: Env): Promise<Response> {
    const origin = env.SITE_ORIGIN ?? "*";
    const headers = {
        "content-type": "application/json",
        "access-control-allow-origin": origin,
        // Never cache: the answer changes from pending to ready.
        "cache-control": "no-store",
    };
    const sessionId = new URL(request.url).searchParams.get("session_id") ?? "";
    if (!/^cs_[A-Za-z0-9_]{8,200}$/.test(sessionId)) {
        return new Response(JSON.stringify({ error: "bad session id" }), { status: 400, headers });
    }

    try {
        const row = await env.DB.prepare("select key, status from licenses where stripe_session_id = ?")
            .bind(sessionId)
            .first<{ key: string; status: string }>();
        if (row === null) {
            return new Response(JSON.stringify({ status: "pending" }), { status: 404, headers });
        }
        return new Response(JSON.stringify({ status: row.status, key: row.key }), { status: 200, headers });
    } catch (error) {
        console.error("licence lookup failed", error);
        return new Response(JSON.stringify({ error: "temporarily unavailable" }), { status: 503, headers });
    }
}
