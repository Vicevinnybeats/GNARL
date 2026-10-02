import { env, SELF } from "cloudflare:test";
import { beforeEach, describe, expect, it } from "vitest";

import schema from "../schema.sql?raw";
import { MAX_NEW_PER_HOUR, MAX_PATCH_BYTES, MAX_PRESETS, normaliseCode } from "../src/sync";

/*
    Preset sync (src/sync.ts), through the real runtime and a real local D1.

    What matters most: a list opens only with its code, and the database
    never holds the code; then that the bounds hold, since anyone may ask
    for a code.
*/

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

const BASE = "https://licence.test";
const PATCH = JSON.stringify({ synth_version: "1.0.6", settings: { volume: 4300 } });

async function newCode(address = "203.0.113.7"): Promise<Response> {
    return SELF.fetch(`${BASE}/sync/new`, { method: "POST", headers: { "cf-connecting-ip": address } });
}

async function code(): Promise<string> {
    const response = await newCode();
    expect(response.status).toBe(201);
    return ((await response.json()) as { code: string }).code;
}

function call(path: string, key: string | null, init: RequestInit = {}): Promise<Response> {
    const headers = new Headers(init.headers);
    if (key !== null) headers.set("authorization", `Bearer ${key}`);
    return SELF.fetch(`${BASE}${path}`, { ...init, headers });
}

const put = (key: string, name: string, patch: string = PATCH): Promise<Response> =>
    call(`/sync/p/${encodeURIComponent(name)}`, key, {
        method: "PUT",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ patch }),
    });

beforeEach(async () => {
    for (const table of ["sync_presets", "sync_spaces", "sync_rate", "activations", "licenses"]) {
        await env.DB.exec(`drop table if exists ${table}`);
    }
    for (const statement of splitStatements(schema)) {
        await env.DB.exec(statement);
    }
});

describe("codes", () => {
    it("hands out a code in the licence key's alphabet", async () => {
        const key = await code();
        expect(key).toMatch(/^SYNC-[2-9A-HJ-NP-Z]{4}(-[2-9A-HJ-NP-Z]{4}){3}$/);
    });

    it("stores a hash of the code, never the code", async () => {
        const key = await code();
        const rows = await env.DB.prepare("select id from sync_spaces").all<{ id: string }>();
        expect(rows.results).toHaveLength(1);
        expect(rows.results[0]?.id).toMatch(/^[0-9a-f]{64}$/);
        expect(rows.results[0]?.id).not.toContain(normaliseCode(key));
    });

    it("reads a code however it is typed", async () => {
        const key = await code();
        const sloppy = key.toLowerCase().replace(/-/g, " ").replace(/^sync /, "");
        expect((await call("/sync", sloppy)).status).toBe(200);
    });

    it("keeps the symbols when a code's own symbols start with SYNC", () => {
        expect(normaliseCode("SYNC2345ABCDEFGH")).toBe("SYNC2345ABCDEFGH");
        expect(normaliseCode("SYNC-SYNC-2345-ABCD-EFGH")).toBe("SYNC2345ABCDEFGH");
    });

    it("limits new codes per address per hour", async () => {
        for (let i = 0; i < MAX_NEW_PER_HOUR; i += 1) expect((await newCode()).status).toBe(201);
        expect((await newCode()).status).toBe(429);
        // Another address is not held back by the first.
        expect((await newCode("198.51.100.4")).status).toBe(201);
    });
});

describe("a list opens only with its code", () => {
    it("refuses a request with no code", async () => {
        expect((await call("/sync", null)).status).toBe(401);
    });

    it("refuses an unknown code", async () => {
        expect((await call("/sync", "SYNC-2222-3333-4444-5555")).status).toBe(404);
    });

    it("keeps two lists apart", async () => {
        const a = await code();
        const b = await code();
        expect((await put(a, "Mine")).status).toBe(200);
        const seen = (await (await call("/sync", b)).json()) as { presets: unknown[] };
        expect(seen.presets).toEqual([]);
        expect((await call("/sync/p/Mine", b)).status).toBe(404);
    });
});

describe("patches", () => {
    it("saves, lists, reads and deletes", async () => {
        const key = await code();
        expect((await put(key, "Wob Talk 2")).status).toBe(200);

        const listed = (await (await call("/sync", key)).json()) as {
            presets: { name: string; size: number; updated_at: string }[];
        };
        expect(listed.presets.map((p) => p.name)).toEqual(["Wob Talk 2"]);
        expect(listed.presets[0]?.size).toBe(PATCH.length);

        const read = (await (await call(`/sync/p/${encodeURIComponent("Wob Talk 2")}`, key)).json()) as {
            patch: string;
        };
        expect(read.patch).toBe(PATCH);

        expect((await call("/sync/p/Wob%20Talk%202", key, { method: "DELETE" })).status).toBe(204);
        expect((await call("/sync/p/Wob%20Talk%202", key)).status).toBe(404);
    });

    it("replaces a patch of the same name instead of adding one", async () => {
        const key = await code();
        await put(key, "A");
        const newer = JSON.stringify({ settings: { volume: 5000 } });
        await put(key, "A", newer);
        const listed = (await (await call("/sync", key)).json()) as { presets: unknown[] };
        expect(listed.presets).toHaveLength(1);
        const read = (await (await call("/sync/p/A", key)).json()) as { patch: string };
        expect(read.patch).toBe(newer);
    });

    it("refuses what is not a patch", async () => {
        const key = await code();
        expect((await put(key, "x", "not json")).status).toBe(400);
        expect((await put(key, "x", JSON.stringify({ no: "settings" }))).status).toBe(400);
        const raw = await call("/sync/p/x", key, { method: "PUT", body: "{" });
        expect(raw.status).toBe(400);
    });

    it("refuses a patch over the size limit", async () => {
        const key = await code();
        const big = JSON.stringify({ settings: {}, pad: "x".repeat(MAX_PATCH_BYTES) });
        expect((await put(key, "big", big)).status).toBe(413);
    });

    it("refuses an empty or over-long name", async () => {
        const key = await code();
        expect((await put(key, " ")).status).toBe(400);
        expect((await put(key, "n".repeat(65))).status).toBe(400);
    });

    it("holds at most MAX_PRESETS patches, and still replaces one when full", async () => {
        const key = await code();
        const id = ((await env.DB.prepare("select id from sync_spaces").first()) as { id: string }).id;
        const statements = [];
        for (let i = 0; i < MAX_PRESETS; i += 1) {
            statements.push(
                env.DB.prepare(
                    "insert into sync_presets (space_id, name, patch, size, updated_at) values (?, ?, ?, ?, ?)",
                ).bind(id, `p${i}`, PATCH, PATCH.length, "2026-10-02T00:00:00Z"),
            );
        }
        await env.DB.batch(statements);
        expect((await put(key, "one more")).status).toBe(409);
        expect((await put(key, "p0")).status).toBe(200);
    });
});

describe("browsers and failures", () => {
    it("answers a CORS preflight from any origin", async () => {
        const response = await SELF.fetch(`${BASE}/sync`, {
            method: "OPTIONS",
            headers: { origin: "juce://juce.backend", "access-control-request-method": "GET" },
        });
        expect(response.status).toBe(204);
        expect(response.headers.get("access-control-allow-origin")).toBe("*");
        expect(response.headers.get("access-control-allow-headers")).toContain("authorization");
    });

    it("says 503, not 'unknown code', when the database fails", async () => {
        const key = await code();
        await env.DB.exec("drop table sync_presets");
        await env.DB.exec("drop table sync_spaces");
        expect((await call("/sync", key)).status).toBe(503);
    });
});
