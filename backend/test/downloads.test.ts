import { env, fetchMock, SELF } from "cloudflare:test";
import { afterEach, beforeAll, beforeEach, describe, expect, it } from "vitest";

import schema from "../schema.sql?raw";
import { download } from "../src/downloads";
import { licenceEmail, stripeWebhook } from "../src/stripe";

/*
    Buyers-only downloads (src/downloads.ts) and the licence email
    (src/stripe.ts emailLicence), through the real runtime, a real local D1
    and a local R2 bucket.

    Downloads: a zip for an active licence, by the thank-you page's session
    or the email's key, and nothing - not a byte - for anyone else. Email:
    one per licence however often Stripe repeats itself, retried when the
    send fails, and nothing at all until it is configured.
*/

const SECRET = "whsec_test_secret";
const ZIP = new Uint8Array([0x50, 0x4b, 0x03, 0x04, 1, 2, 3, 4, 5]);

function splitStatements(sql: string): string[] {
    return sql
        .split("\n")
        .map((line) => line.replace(/--.*$/, ""))
        .join("\n")
        .split(";")
        .map((statement) => statement.replace(/\s+/g, " ").trim())
        .filter((statement) => statement.length > 0)
        .map((statement) => statement + ";");
}

async function hmacHex(secret: string, message: string): Promise<string> {
    const key = await crypto.subtle.importKey("raw", new TextEncoder().encode(secret), { name: "HMAC", hash: "SHA-256" }, false, ["sign"]);
    const signature = await crypto.subtle.sign("HMAC", key, new TextEncoder().encode(message));
    return [...new Uint8Array(signature)].map((b) => b.toString(16).padStart(2, "0")).join("");
}

async function paid(id = "cs_test_a1b2c3d4e5"): Promise<Request> {
    const body = JSON.stringify({
        id: `evt_${id}`,
        type: "checkout.session.completed",
        data: { object: { id, payment_status: "paid", payment_intent: `pi_${id}`, customer_details: { email: "producer@example.com" } } },
    });
    const time = Math.floor(Date.now() / 1000);
    return new Request("https://licence.test/stripe/webhook", {
        method: "POST",
        headers: { "stripe-signature": `t=${time},v1=${await hmacHex(SECRET, `${time}.${body}`)}` },
        body,
    });
}

const licence = async () =>
    (await env.DB.prepare("select key, status, emailed_at from licenses").first<{ key: string; status: string; emailed_at: string | null }>())!;

beforeAll(() => {
    fetchMock.activate();
    fetchMock.disableNetConnect();
});

beforeEach(async () => {
    await env.DB.exec("drop table if exists activations");
    await env.DB.exec("drop table if exists licenses");
    for (const statement of splitStatements(schema)) await env.DB.exec(statement);
    await env.DOWNLOADS!.put("GNARL-Windows.zip", ZIP);
    await env.DOWNLOADS!.put("GNARL-macOS.zip", ZIP);
});

afterEach(() => fetchMock.assertNoPendingInterceptors());

describe("downloads are for buyers", () => {
    it("streams the zip for a paid session (the thank-you page)", async () => {
        await SELF.fetch(await paid());
        const response = await SELF.fetch("https://licence.test/download?os=windows&session_id=cs_test_a1b2c3d4e5");
        expect(response.status).toBe(200);
        expect(response.headers.get("content-disposition")).toBe('attachment; filename="GNARL-Windows.zip"');
        expect(response.headers.get("cache-control")).toContain("no-store");
        expect(new Uint8Array(await response.arrayBuffer())).toEqual(ZIP);
    });

    it("streams it for the key, typed in any case (the email)", async () => {
        await SELF.fetch(await paid());
        const { key } = await licence();
        const response = await SELF.fetch(`https://licence.test/download?os=macos&key=${key.toLowerCase()}`);
        expect(response.status).toBe(200);
        expect(response.headers.get("content-disposition")).toContain("GNARL-macOS.zip");
        await response.arrayBuffer();
    });

    for (const [name, query] of [
        ["a key nobody bought", "os=windows&key=GNARL-AAAA-BBBB-CCCC-DDDD"],
        ["a session that never paid", "os=windows&session_id=cs_test_nobody12345"],
    ] as const) {
        it(`gives nothing for ${name}`, async () => {
            await SELF.fetch(await paid());
            const response = await SELF.fetch(`https://licence.test/download?${query}`);
            expect(response.status).toBe(403);
            expect(response.headers.get("content-type")).toContain("text/plain");
        });
    }

    it("gives nothing once the licence is refunded", async () => {
        await SELF.fetch(await paid());
        await env.DB.exec("update licenses set status = 'refunded'");
        const response = await SELF.fetch("https://licence.test/download?os=windows&session_id=cs_test_a1b2c3d4e5");
        expect(response.status).toBe(403);
        expect(await response.text()).toContain("refunded");
    });

    it("tells the thank-you page when the private links work", async () => {
        await SELF.fetch(await paid());
        const body = (await (await SELF.fetch("https://licence.test/licence?session_id=cs_test_a1b2c3d4e5")).json()) as { downloads: boolean };
        expect(body.downloads).toBe(true);
    });

    it("refuses an incomplete link and an unknown platform", async () => {
        expect((await SELF.fetch("https://licence.test/download?os=windows")).status).toBe(400);
        expect((await SELF.fetch("https://licence.test/download?os=linux&key=GNARL-AAAA-BBBB-CCCC-DDDD")).status).toBe(400);
        expect((await SELF.fetch("https://licence.test/download?os=windows&key=../../etc")).status).toBe(400);
    });

    it("says unavailable (503), not 'not yours', without the bucket or the file", async () => {
        await SELF.fetch(await paid());
        const request = new Request("https://licence.test/download?os=windows&session_id=cs_test_a1b2c3d4e5");
        expect((await download(request, { ...env, DOWNLOADS: undefined })).status).toBe(503);
        await env.DOWNLOADS!.delete("GNARL-Windows.zip");
        expect((await SELF.fetch(request)).status).toBe(503);
    });
});

describe("the key by email", () => {
    const mailEnv = () => ({ ...env, RESEND_API_KEY: "re_test", MAIL_FROM: "GNARL <keys@gnarl.test>" });

    it("sends one email with the key and the buyer's download links, however often Stripe repeats", async () => {
        let sent: Record<string, unknown> | null = null;
        let headers: Record<string, string> = {};
        fetchMock
            .get("https://api.resend.com")
            .intercept({ path: "/emails", method: "POST" })
            .reply(200, (options) => {
                sent = JSON.parse(String(options.body));
                headers = options.headers as Record<string, string>;
                return { id: "email_1" };
            });

        expect((await stripeWebhook(await paid(), mailEnv())).status).toBe(200);
        const row = await licence();
        expect(row.emailed_at).not.toBeNull();
        expect(sent).not.toBeNull();
        const mail = sent as unknown as { to: string[]; from: string; text: string; html: string };
        expect(mail.to).toEqual(["producer@example.com"]);
        expect(mail.from).toBe("GNARL <keys@gnarl.test>");
        expect(mail.text).toContain(row.key);
        expect(mail.text).toContain(`https://licence.test/download?os=windows&key=${row.key}`);
        expect(mail.html).toContain(row.key);
        expect(headers["authorization"]).toBe("Bearer re_test");
        expect(headers["idempotency-key"]).toBe(`gnarl-licence-${row.key}`);

        // Stripe delivers it again: no second email (no interceptor is left
        // for one, and disableNetConnect would fail a real request).
        expect((await stripeWebhook(await paid(), mailEnv())).status).toBe(200);
    });

    it("keeps the key and lets Stripe retry when the send fails", async () => {
        fetchMock.get("https://api.resend.com").intercept({ path: "/emails", method: "POST" }).reply(500, "boom");
        expect((await stripeWebhook(await paid(), mailEnv())).status).toBe(503);
        const first = await licence();
        expect(first.key).toMatch(/^GNARL-/);
        expect(first.emailed_at).toBeNull();

        fetchMock.get("https://api.resend.com").intercept({ path: "/emails", method: "POST" }).reply(200, { id: "email_2" });
        expect((await stripeWebhook(await paid(), mailEnv())).status).toBe(200);
        const second = await licence();
        expect(second.key).toBe(first.key);
        expect(second.emailed_at).not.toBeNull();
    });

    it("sends nothing until Resend is configured, and still issues the key", async () => {
        expect((await stripeWebhook(await paid(), env)).status).toBe(200);
        expect((await licence()).emailed_at).toBeNull();
    });

    it("links the public release when the private bucket is not bound yet", () => {
        const mail = licenceEmail("GNARL-AAAA-BBBB-CCCC-DDDD", "https://licence.test", false);
        expect(mail.text).toContain("https://github.com/Vicevinnybeats/GNARL/releases/latest/download/GNARL-Windows.zip");
        expect(mail.text).not.toContain("licence.test/download");
    });
});
