import { env, SELF } from "cloudflare:test";
import { beforeEach, describe, expect, it } from "vitest";

import schema from "../schema.sql?raw";
import { generateKey, KEY_SYMBOLS, stripeWebhook, verifyStripeSignature } from "../src/stripe";

/*
    Payments (src/stripe.ts), through the real runtime and a real local D1.

    The webhook CREATES licence keys, so the cases that matter most are the
    refusals: an unsigned, mis-signed, stale or tampered event changes
    nothing. Then the promise to a buyer: one payment, exactly one key,
    however many times Stripe delivers the event - and a full refund ends it.
*/

const SECRET = "whsec_test_secret";

function splitStatements(sql: string): string[] {
    const withoutComments = sql
        .split("\n")
        .map((line) => line.replace(/--.*$/, ""))
        .join("\n");
    return withoutComments
        .split(";")
        .map((statement) => statement.replace(/\s+/g, " ").trim())
        .filter((statement) => statement.length > 0)
        .map((statement) => statement + ";");
}

async function hmacHex(secret: string, message: string): Promise<string> {
    const key = await crypto.subtle.importKey(
        "raw",
        new TextEncoder().encode(secret),
        { name: "HMAC", hash: "SHA-256" },
        false,
        ["sign"],
    );
    const signature = await crypto.subtle.sign("HMAC", key, new TextEncoder().encode(message));
    return [...new Uint8Array(signature)].map((b) => b.toString(16).padStart(2, "0")).join("");
}

/** A request as Stripe sends one: the raw body, signed `t.<body>`. */
async function stripeRequest(
    event: unknown,
    { secret = SECRET, time = Math.floor(Date.now() / 1000), tamper = false } = {},
): Promise<Request> {
    const body = JSON.stringify(event);
    const signature = await hmacHex(secret, `${time}.${body}`);
    return new Request("https://licence.test/stripe/webhook", {
        method: "POST",
        headers: { "content-type": "application/json", "stripe-signature": `t=${time},v1=${signature}` },
        body: tamper ? body.replace("paid", "PAID") : body,
    });
}

function paidSession(id = "cs_test_a1b2c3d4e5", overrides: Record<string, unknown> = {}) {
    return {
        id: `evt_${id}`,
        type: "checkout.session.completed",
        data: {
            object: {
                id,
                payment_status: "paid",
                customer: "cus_123",
                payment_intent: `pi_${id}`,
                customer_details: { email: "producer@example.com" },
                ...overrides,
            },
        },
    };
}

async function licences(): Promise<Array<Record<string, unknown>>> {
    return (await env.DB.prepare("select * from licenses order by created_at").all()).results;
}

/** The one licence there should be; fails the test if there is not exactly one. */
async function onlyLicence(): Promise<Record<string, unknown>> {
    const rows = await licences();
    expect(rows).toHaveLength(1);
    return rows[0] as Record<string, unknown>;
}

beforeEach(async () => {
    await env.DB.exec("drop table if exists activations");
    await env.DB.exec("drop table if exists licenses");
    for (const statement of splitStatements(schema)) {
        await env.DB.exec(statement);
    }
});

describe("the webhook refuses what Stripe did not sign", () => {
    it("accepts a correctly signed event", async () => {
        const response = await SELF.fetch(await stripeRequest(paidSession()));
        expect(response.status).toBe(200);
        expect(await licences()).toHaveLength(1);
    });

    for (const [name, options] of [
        ["the wrong secret", { secret: "whsec_somebody_else" }],
        ["a timestamp six minutes old (a replay)", { time: Math.floor(Date.now() / 1000) - 360 }],
        ["a body changed after signing", { tamper: true }],
    ] as const) {
        it(`refuses ${name}, and issues nothing`, async () => {
            const response = await SELF.fetch(await stripeRequest(paidSession(), options));
            expect(response.status).toBe(400);
            expect(await licences()).toHaveLength(0);
        });
    }

    it("refuses an event with no signature header", async () => {
        const response = await SELF.fetch("https://licence.test/stripe/webhook", {
            method: "POST",
            body: JSON.stringify(paidSession()),
        });
        expect(response.status).toBe(400);
        expect(await licences()).toHaveLength(0);
    });

    it("refuses everything when no secret is configured (500, so Stripe retries)", async () => {
        const response = await stripeWebhook(await stripeRequest(paidSession()), { ...env, STRIPE_WEBHOOK_SECRET: undefined });
        expect(response.status).toBe(500);
        expect(await licences()).toHaveLength(0);
    });

    it("accepts any of several v1 signatures (Stripe sends two while a secret rolls)", async () => {
        const body = "{}";
        const time = Math.floor(Date.now() / 1000);
        const good = await hmacHex(SECRET, `${time}.${body}`);
        expect(await verifyStripeSignature(body, `t=${time},v1=${"0".repeat(64)},v1=${good}`, SECRET, time)).toBe(true);
        expect(await verifyStripeSignature(body, `t=${time},v1=${"0".repeat(64)}`, SECRET, time)).toBe(false);
    });
});

describe("one payment, one key", () => {
    it("issues a key for a paid checkout, with the buyer's email and payment", async () => {
        await SELF.fetch(await stripeRequest(paidSession()));
        const row = await onlyLicence();
        expect(row.key).toMatch(/^GNARL-[2-9A-HJ-NP-Z]{4}(-[2-9A-HJ-NP-Z]{4}){3}$/);
        expect(row.status).toBe("active");
        expect(row.email).toBe("producer@example.com");
        expect(row.stripe_session_id).toBe("cs_test_a1b2c3d4e5");
        expect(row.stripe_payment_intent).toBe("pi_cs_test_a1b2c3d4e5");
        expect(row.max_activations).toBe(3);
    });

    it("issues ONE key when Stripe delivers the same payment three times", async () => {
        for (let i = 0; i < 3; i++) {
            const response = await SELF.fetch(await stripeRequest(paidSession()));
            expect(response.status).toBe(200);
        }
        const rows = await licences();
        expect(rows).toHaveLength(1);
    });

    it("keeps the first key when a repeat arrives, rather than replacing it", async () => {
        await SELF.fetch(await stripeRequest(paidSession()));
        const first = await onlyLicence();
        await SELF.fetch(await stripeRequest(paidSession()));
        const again = await onlyLicence();
        expect(again.key).toBe(first.key);
    });

    it("issues nothing for a checkout that is not paid yet, and a key once it is", async () => {
        await SELF.fetch(await stripeRequest(paidSession("cs_test_bank_1", { payment_status: "unpaid" })));
        expect(await licences()).toHaveLength(0);
        const settled = paidSession("cs_test_bank_1");
        settled.type = "checkout.session.async_payment_succeeded";
        await SELF.fetch(await stripeRequest(settled));
        expect(await licences()).toHaveLength(1);
    });

    it("answers other event types with 200 and changes nothing", async () => {
        const response = await SELF.fetch(await stripeRequest({ id: "evt_x", type: "customer.created", data: { object: {} } }));
        expect(response.status).toBe(200);
        expect(await licences()).toHaveLength(0);
    });

    it("answers 503 when the database fails, so Stripe retries instead of the key being lost", async () => {
        await env.DB.exec("drop table licenses");
        const response = await SELF.fetch(await stripeRequest(paidSession()));
        expect(response.status).toBe(503);
    });

    it("makes keys of 16 symbols from the unambiguous alphabet, and no two alike", () => {
        expect(KEY_SYMBOLS).toHaveLength(32);
        expect(KEY_SYMBOLS).not.toMatch(/[01IO]/);
        const keys = new Set(Array.from({ length: 2000 }, generateKey));
        expect(keys.size).toBe(2000);
    });
});

describe("refunds", () => {
    async function refund(refunded: boolean) {
        return SELF.fetch(
            await stripeRequest({
                id: "evt_refund",
                type: "charge.refunded",
                data: { object: { id: "ch_1", payment_intent: "pi_cs_test_a1b2c3d4e5", refunded } },
            }),
        );
    }

    it("a full refund ends the licence, and the plugin is then told no", async () => {
        await SELF.fetch(await stripeRequest(paidSession()));
        const row = await onlyLicence();
        expect((await refund(true)).status).toBe(200);
        const after = await onlyLicence();
        expect(after.status).toBe("refunded");

        const activation = await SELF.fetch("https://licence.test/activate", {
            method: "POST",
            body: JSON.stringify({ key: row.key, machineId: "m1" }),
        });
        expect(await activation.json()).toEqual({ status: "rejected", reason: "refunded" });
    });

    it("a partial refund leaves it active", async () => {
        await SELF.fetch(await stripeRequest(paidSession()));
        await refund(false);
        const after = await onlyLicence();
        expect(after.status).toBe("active");
    });
});

describe("the thank-you page's lookup", () => {
    const lookup = (id: string) => SELF.fetch(`https://licence.test/licence?session_id=${encodeURIComponent(id)}`);

    it("answers 'pending' (404) until the webhook has landed, then the key", async () => {
        const before = await lookup("cs_test_a1b2c3d4e5");
        expect(before.status).toBe(404);
        expect(await before.json()).toEqual({ status: "pending" });

        await SELF.fetch(await stripeRequest(paidSession()));
        const row = await onlyLicence();
        const after = await lookup("cs_test_a1b2c3d4e5");
        expect(after.status).toBe(200);
        expect(await after.json()).toEqual({ status: "active", key: row.key });
        expect(after.headers.get("access-control-allow-origin")).toBe("https://gnarl.vercel.app");
        expect(after.headers.get("cache-control")).toBe("no-store");
    });

    it("refuses something that is not a checkout session id", async () => {
        expect((await lookup("anything")).status).toBe(400);
        expect((await lookup("cs_x' or 1=1 --")).status).toBe(400);
    });
});
