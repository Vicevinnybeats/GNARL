/*
    Preset sync (Phase 8, docs/design/phase8-01-sync.md): one list of saved
    patches shared by the plugin and the phone.

    THE CODE IS THE IDENTITY, as the licence key is for licensing: no
    accounts, no email, no password. `POST /sync/new` hands out a code like
    SYNC-7KQ3-M2XH-9TPA-WD4R (80 bits, the licence key's alphabet); whoever
    holds it reads and writes that list. The database never stores the code
    itself, only a SHA-256 of it, so a copy of the database opens nothing.

    It needs no payment, so it works during the free beta. Everything a
    stranger could do with it is bounded: a list holds at most MAX_PRESETS
    patches of at most MAX_PATCH_BYTES, and one address may open at most
    MAX_NEW_PER_HOUR lists an hour.

    CORS is open (`*`) because the callers are the phone page, wherever it
    is opened from, and the plugin's web view, whose origin is a custom
    scheme. That is safe here because nothing rides on cookies: the code is
    sent in an Authorization header on every request.
*/

import type { Env } from "./index";
import { KEY_SYMBOLS } from "./stripe";

/** A patch is about 180 kB, mostly wavetables; five of those fit. */
export const MAX_PATCH_BYTES = 1_000_000;
/** Many times what anyone keeps, few enough to bound a list at 300 MB. */
export const MAX_PRESETS = 300;
export const MAX_NAME = 64;
/** Per address: a person needs one, a script that mints them gets ten. */
export const MAX_NEW_PER_HOUR = 10;

const CORS = {
    "access-control-allow-origin": "*",
    "access-control-allow-methods": "GET, PUT, POST, DELETE, OPTIONS",
    "access-control-allow-headers": "authorization, content-type",
    "access-control-max-age": "86400",
};

const json = (body: unknown, status = 200): Response =>
    new Response(JSON.stringify(body), {
        status,
        headers: { "content-type": "application/json", ...CORS },
    });

/** Our fault, never the code's: the client keeps its own copy and retries. */
const unavailable = (): Response => json({ error: "temporarily unavailable" }, 503);

async function sha256(text: string): Promise<string> {
    const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(text));
    return [...new Uint8Array(digest)].map((b) => b.toString(16).padStart(2, "0")).join("");
}

export function generateSyncCode(): string {
    const bytes = crypto.getRandomValues(new Uint8Array(16));
    const symbols = [...bytes].map((b) => KEY_SYMBOLS[b % KEY_SYMBOLS.length]);
    const groups = [0, 4, 8, 12].map((i) => symbols.slice(i, i + 4).join(""));
    return `SYNC-${groups.join("-")}`;
}

/** Codes are typed by hand on a phone: case and spaces do not matter. */
export function normaliseCode(raw: string): string {
    const symbols = raw.toUpperCase().replace(/[^0-9A-Z]/g, "");
    // The prefix only when it is there: a code's own symbols may start SYNC.
    return symbols.length === 20 && symbols.startsWith("SYNC") ? symbols.slice(4) : symbols;
}

const spaceId = (code: string): Promise<string> => sha256(`gnarl-sync:${normaliseCode(code)}`);

/** The space a request's code opens, or the response to send instead. */
async function space(request: Request, env: Env): Promise<string | Response> {
    const header = request.headers.get("authorization") ?? "";
    const code = header.replace(/^Bearer\s+/i, "");
    if (normaliseCode(code).length !== 16) return json({ error: "a sync code is required" }, 401);
    const id = await spaceId(code);
    try {
        const row = await env.DB.prepare("select id from sync_spaces where id = ?").bind(id).first();
        if (row === null) return json({ error: "unknown code" }, 404);
    } catch (error) {
        console.error("sync lookup failed", error);
        return unavailable();
    }
    return id;
}

async function newSpace(request: Request, env: Env): Promise<Response> {
    const address = request.headers.get("cf-connecting-ip") ?? "unknown";
    // Hashed and kept a day, only to count: the address itself is never stored.
    const hour = Math.floor(Date.now() / 3_600_000);
    const bucket = `${(await sha256(`gnarl-rate:${address}`)).slice(0, 16)}:${hour}`;
    try {
        await env.DB.prepare("delete from sync_rate where hour < ?").bind(hour - 24).run();
        const counted = await env.DB.prepare(
            `insert into sync_rate (bucket, hour, n) values (?, ?, 1)
             on conflict (bucket) do update set n = n + 1
             returning n`,
        )
            .bind(bucket, hour)
            .first<{ n: number }>();
        if ((counted?.n ?? 0) > MAX_NEW_PER_HOUR) {
            return json({ error: "too many new codes from here; try again in an hour" }, 429);
        }
        const code = generateSyncCode();
        await env.DB.prepare("insert into sync_spaces (id) values (?)").bind(await spaceId(code)).run();
        return json({ code }, 201);
    } catch (error) {
        console.error("sync new failed", error);
        return unavailable();
    }
}

async function list(id: string, env: Env): Promise<Response> {
    try {
        const rows = await env.DB.prepare(
            "select name, updated_at, size from sync_presets where space_id = ? order by name collate nocase",
        )
            .bind(id)
            .all<{ name: string; updated_at: string; size: number }>();
        await env.DB.prepare("update sync_spaces set last_seen = ? where id = ?")
            .bind(new Date().toISOString(), id)
            .run();
        return json({ presets: rows.results });
    } catch (error) {
        console.error("sync list failed", error);
        return unavailable();
    }
}

async function read(id: string, name: string, env: Env): Promise<Response> {
    try {
        const row = await env.DB.prepare(
            "select name, patch, updated_at from sync_presets where space_id = ? and name = ?",
        )
            .bind(id, name)
            .first<{ name: string; patch: string; updated_at: string }>();
        return row === null ? json({ error: "no such patch" }, 404) : json(row);
    } catch (error) {
        console.error("sync read failed", error);
        return unavailable();
    }
}

async function write(id: string, name: string, request: Request, env: Env): Promise<Response> {
    const text = await request.text();
    if (text.length > MAX_PATCH_BYTES + 1000) return json({ error: "patch too large" }, 413);
    let patch: unknown;
    try {
        patch = (JSON.parse(text) as { patch?: unknown }).patch;
    } catch {
        return json({ error: "malformed body" }, 400);
    }
    if (typeof patch !== "string") return json({ error: "patch must be a string" }, 400);
    if (patch.length > MAX_PATCH_BYTES) return json({ error: "patch too large" }, 413);
    // A patch is a Vital-format JSON object with its settings; anything else
    // would only fail later, on the other device, when it is loaded.
    try {
        const parsed = JSON.parse(patch) as { settings?: unknown };
        if (typeof parsed !== "object" || parsed === null || typeof parsed.settings !== "object") throw new Error();
    } catch {
        return json({ error: "not a patch" }, 400);
    }
    const now = new Date().toISOString();
    try {
        const existing = await env.DB.prepare("select 1 from sync_presets where space_id = ? and name = ?")
            .bind(id, name)
            .first();
        if (existing === null) {
            const counted = await env.DB.prepare("select count(*) as n from sync_presets where space_id = ?")
                .bind(id)
                .first<{ n: number }>();
            if ((counted?.n ?? 0) >= MAX_PRESETS) return json({ error: `a list holds ${MAX_PRESETS} patches` }, 409);
        }
        await env.DB.prepare(
            `insert into sync_presets (space_id, name, patch, size, updated_at) values (?, ?, ?, ?, ?)
             on conflict (space_id, name) do update set patch = excluded.patch, size = excluded.size,
                                                        updated_at = excluded.updated_at`,
        )
            .bind(id, name, patch, patch.length, now)
            .run();
        return json({ name, updated_at: now });
    } catch (error) {
        console.error("sync write failed", error);
        return unavailable();
    }
}

async function remove(id: string, name: string, env: Env): Promise<Response> {
    try {
        await env.DB.prepare("delete from sync_presets where space_id = ? and name = ?").bind(id, name).run();
        return new Response(null, { status: 204, headers: CORS });
    } catch (error) {
        console.error("sync delete failed", error);
        return unavailable();
    }
}

/** Everything under /sync. */
export async function sync(request: Request, env: Env): Promise<Response> {
    const url = new URL(request.url);
    if (request.method === "OPTIONS") return new Response(null, { status: 204, headers: CORS });

    if (url.pathname === "/sync/new") {
        return request.method === "POST" ? newSpace(request, env) : json({ error: "POST only" }, 405);
    }

    const opened = await space(request, env);
    if (opened instanceof Response) return opened;

    if (url.pathname === "/sync") {
        return request.method === "GET" ? list(opened, env) : json({ error: "GET only" }, 405);
    }

    const match = /^\/sync\/p\/(.+)$/.exec(url.pathname);
    if (match === null) return json({ error: "not found" }, 404);
    let name: string;
    try {
        name = decodeURIComponent(match[1] ?? "").trim();
    } catch {
        return json({ error: "bad name" }, 400);
    }
    if (name.length === 0 || name.length > MAX_NAME) return json({ error: `a name is 1-${MAX_NAME} characters` }, 400);

    if (request.method === "GET") return read(opened, name, env);
    if (request.method === "PUT") return write(opened, name, request, env);
    if (request.method === "DELETE") return remove(opened, name, env);
    return json({ error: "GET, PUT or DELETE" }, 405);
}
