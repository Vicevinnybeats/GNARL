/*
    GNARL licence activation, on Cloudflare Workers + D1.

    WHAT THIS ENDPOINT CANNOT DO is the important part. It answers "is this
    key good on this machine" and nothing else. It cannot silence the plugin,
    because the plugin does not ask it for permission to make sound - audio
    is a compile-time `true` on the client (CLAUDE.md section 9). The worst
    this can do to somebody mid-take is disable preset saving.

    THE PLUGIN CARRIES NO CREDENTIAL FOR THIS SERVICE, and it must not: a key
    embedded in a downloadable binary is a key shipped to everyone the moment
    somebody runs `strings` on it. The licence key IS the credential, and it
    is checked here. That makes the endpoint publicly callable, which is the
    correct trade - it answers one question about one key and holds nothing
    worth taking - and it is why `verifyStripeSignature` below is not
    optional on the webhook, which DOES need to be trusted.
*/

export interface Env {
    DB: D1Database;
    /** Set with `wrangler secret put STRIPE_WEBHOOK_SECRET`. */
    STRIPE_WEBHOOK_SECRET?: string;
}

/*
    THREE ANSWERS, NOT TWO, and the third one is the whole design.

    `valid` and `rejected` are decisions. Anything else - a 503, a timeout, a
    dropped connection - is NOT AN ANSWER, and the client treats it as
    `unreachable`, which is what opens the 30-day grace period. That
    distinction is why a producer in a studio with no wifi keeps working.

    So this service must NEVER return `rejected` because something broke on
    our side. When the database fails, it says so with a 503 and the customer
    stays inside grace. Being unreachable is safe; being wrong is not.
*/
type Decision =
    | { status: "valid"; reason: string }
    | { status: "rejected"; reason: string; maxActivations?: number };

const json = <T,>(body: T, status = 200): Response =>
    new Response(JSON.stringify(body), {
        status,
        headers: { "content-type": "application/json" },
    });

/** 503, deliberately: see the note on three answers above. */
const unavailable = (): Response => json({ error: "temporarily unavailable" }, 503);

interface LicenseRow {
    id: string;
    status: string;
    max_activations: number;
}

export async function activate(
    request: Request,
    env: Env,
): Promise<Response> {
    let payload: { key?: unknown; machineId?: unknown; machineLabel?: unknown };

    try {
        payload = await request.json();
    } catch {
        return json({ error: "malformed body" }, 400);
    }

    const key = typeof payload.key === "string" ? payload.key.trim() : "";
    const machineId =
        typeof payload.machineId === "string" ? payload.machineId.trim() : "";
    const machineLabel =
        typeof payload.machineLabel === "string"
            ? payload.machineLabel.slice(0, 120)
            : null;

    if (key.length === 0 || machineId.length === 0) {
        return json({ error: "key and machineId are required" }, 400);
    }

    let license: LicenseRow | null;

    try {
        license = await env.DB.prepare(
            "select id, status, max_activations from licenses where key = ?",
        )
            .bind(key)
            .first<LicenseRow>();
    } catch (error) {
        // A failed LOOKUP is not a failed licence. Grace covers our faults.
        console.error("lookup failed", error);
        return unavailable();
    }

    if (license === null) {
        return json<Decision>({ status: "rejected", reason: "unknown key" });
    }

    if (license.status !== "active") {
        return json<Decision>({ status: "rejected", reason: license.status });
    }

    const now = new Date().toISOString();

    try {
        /*  Re-check first. A plugin checking in daily is not asking for a new
            seat, and treating it as one would exhaust a three-machine licence
            in three days. The unique constraint makes that structural, but
            asking first is what turns a constraint violation into an ordinary
            "yes, still you". */
        const existing = await env.DB.prepare(
            "select id from activations where license_id = ? and machine_id = ?",
        )
            .bind(license.id, machineId)
            .first<{ id: string }>();

        if (existing !== null) {
            await env.DB.prepare(
                "update activations set last_seen = ? where id = ?",
            )
                .bind(now, existing.id)
                .run();

            return json<Decision>({ status: "valid", reason: "known machine" });
        }

        const counted = await env.DB.prepare(
            "select count(*) as n from activations where license_id = ?",
        )
            .bind(license.id)
            .first<{ n: number }>();

        const used = counted?.n ?? 0;

        if (used >= license.max_activations) {
            return json<Decision>({
                status: "rejected",
                reason: "activation limit reached",
                maxActivations: license.max_activations,
            });
        }

        await env.DB.prepare(
            `insert into activations (id, license_id, machine_id, machine_label,
                                      first_seen, last_seen)
             values (?, ?, ?, ?, ?, ?)`,
        )
            .bind(crypto.randomUUID(), license.id, machineId, machineLabel, now, now)
            .run();

        return json<Decision>({ status: "valid", reason: "new activation" });
    } catch (error) {
        /*  Two machines activating at once can lose here on the unique
            constraint, and that is not a rejection either - the seat went to
            the other request, and a retry gets the honest answer. 503 keeps
            the customer inside grace rather than telling them their licence
            is bad because we raced ourselves. */
        console.error("activation failed", error);
        return unavailable();
    }
}

export default {
    async fetch(request: Request, env: Env): Promise<Response> {
        const url = new URL(request.url);

        if (url.pathname === "/health") {
            return json({ ok: true });
        }

        if (url.pathname === "/activate") {
            if (request.method !== "POST") {
                return json({ error: "POST only" }, 405);
            }

            return activate(request, env);
        }

        return json({ error: "not found" }, 404);
    },
} satisfies ExportedHandler<Env>;
