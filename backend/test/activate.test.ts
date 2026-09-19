import { env, SELF } from "cloudflare:test";
import { beforeEach, describe, expect, it } from "vitest";

import schema from "../schema.sql?raw";

/*
    The licence endpoint, driven over HTTP through the real runtime.

    THE RULE EVERY CASE HERE PROTECTS (CLAUDE.md section 9): the licence
    check never silences the plugin, and the difference between "no" and "no
    answer" is what makes that true. `rejected` disables preset saving;
    anything that is not an answer opens a 30-day grace period instead. A
    service that returns `rejected` when its own database hiccups would lock
    out a paying customer for a fault that was ours, so the 503 cases below
    matter as much as the happy path.
*/

const KEY = "GNARL-TEST-0001";

/*  D1's `exec` takes one statement at a time and treats a newline as a
    statement boundary, so the schema has to be flattened before it is fed
    in - and the COMMENTS have to go first.

    Flattening a file with `--` comments still in it puts the rest of the
    line inside the comment, which silently swallows the statement; and
    splitting on `;` before stripping them breaks on the semicolon inside
    "a chargeback; they mean different things". Both of those happened. */
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

async function post(body: unknown): Promise<Response> {
    return SELF.fetch("https://licence.test/activate", {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify(body),
    });
}

beforeEach(async () => {
    // A fresh database per test: an activation left behind by a previous
    // case would make the limit tests depend on execution order.
    await env.DB.exec("drop table if exists activations");
    await env.DB.exec("drop table if exists licenses");

    for (const statement of splitStatements(schema)) {
        await env.DB.exec(statement);
    }

    await env.DB.prepare(
        `insert into licenses (id, key, status, max_activations)
         values ('lic-1', ?, 'active', 2)`,
    )
        .bind(KEY)
        .run();
});

describe("activation", () => {
    it("accepts a good key on a new machine", async () => {
        const response = await post({ key: KEY, machineId: "m1" });

        expect(response.status).toBe(200);
        expect(await response.json()).toEqual({
            status: "valid",
            reason: "new activation",
        });
    });

    it("does not spend a seat when the same machine checks in again", async () => {
        /*  THE ONE THAT MATTERS MOST for a customer who does nothing wrong.
            The plugin re-checks periodically; if every check took a seat, a
            three-machine licence would be exhausted in three days and the
            customer's next session would tell them their licence is bad. */
        await post({ key: KEY, machineId: "m1" });

        const again = await post({ key: KEY, machineId: "m1" });

        expect(await again.json()).toEqual({
            status: "valid",
            reason: "known machine",
        });

        const counted = await env.DB.prepare(
            "select count(*) as n from activations",
        ).first<{ n: number }>();

        expect(counted?.n).toBe(1);
    });

    it("advances last_seen on a re-check without moving first_seen", async () => {
        await post({ key: KEY, machineId: "m1" });

        await env.DB.prepare(
            "update activations set first_seen = ?, last_seen = ?",
        )
            .bind("2020-01-01T00:00:00Z", "2020-01-01T00:00:00Z")
            .run();

        await post({ key: KEY, machineId: "m1" });

        const row = await env.DB.prepare(
            "select first_seen, last_seen from activations",
        ).first<{ first_seen: string; last_seen: string }>();

        // first_seen is when they bought in; last_seen is whether they are
        // still around. Support needs both, and an update that moved the
        // first would quietly destroy the more useful one.
        expect(row?.first_seen).toBe("2020-01-01T00:00:00Z");
        expect(row?.last_seen).not.toBe("2020-01-01T00:00:00Z");
    });

    it("fills the seats it has and then refuses", async () => {
        expect(await (await post({ key: KEY, machineId: "m1" })).json())
            .toMatchObject({ status: "valid" });
        expect(await (await post({ key: KEY, machineId: "m2" })).json())
            .toMatchObject({ status: "valid" });

        const third = await post({ key: KEY, machineId: "m3" });

        expect(await third.json()).toEqual({
            status: "rejected",
            reason: "activation limit reached",
            maxActivations: 2,
        });

        // A refusal is still a 200. It is an ANSWER - the client must be able
        // to tell it apart from the service being down, because one disables
        // preset saving and the other opens the grace period.
        expect(third.status).toBe(200);
    });

    it("rejects a key it has never seen", async () => {
        const response = await post({ key: "GNARL-NOPE", machineId: "m1" });

        expect(response.status).toBe(200);
        expect(await response.json()).toMatchObject({
            status: "rejected",
            reason: "unknown key",
        });
    });

    it("rejects revoked, refunded and expired keys by name", async () => {
        for (const status of ["revoked", "refunded", "expired"]) {
            await env.DB.prepare("update licenses set status = ? where key = ?")
                .bind(status, KEY)
                .run();

            const response = await post({ key: KEY, machineId: "m1" });

            // The reason carries the status rather than a generic "no", so
            // support can tell a chargeback from a leak without a database
            // query.
            expect(await response.json()).toMatchObject({
                status: "rejected",
                reason: status,
            });
        }
    });

    it("does not activate a machine for a key it just rejected", async () => {
        await env.DB.prepare("update licenses set status = 'revoked'").run();

        await post({ key: KEY, machineId: "m1" });

        const counted = await env.DB.prepare(
            "select count(*) as n from activations",
        ).first<{ n: number }>();

        expect(counted?.n).toBe(0);
    });
});

describe("bad requests", () => {
    it("needs both a key and a machine", async () => {
        expect((await post({ key: KEY })).status).toBe(400);
        expect((await post({ machineId: "m1" })).status).toBe(400);
        expect((await post({ key: "  ", machineId: "m1" })).status).toBe(400);
    });

    it("survives a body that is not JSON", async () => {
        const response = await SELF.fetch("https://licence.test/activate", {
            method: "POST",
            body: "{not json",
        });

        expect(response.status).toBe(400);
    });

    it("ignores a non-string key instead of trusting it", async () => {
        // A client sending `{"key": {"$ne": null}}` gets a 400, not a query.
        const response = await post({ key: { evil: true }, machineId: "m1" });

        expect(response.status).toBe(400);
    });

    it("is POST only", async () => {
        const response = await SELF.fetch("https://licence.test/activate");
        expect(response.status).toBe(405);
    });
});

describe("what a rejection must never be", () => {
    it("answers a broken database with 503, never with a rejection", async () => {
        /*  THE CASE THE WHOLE DESIGN EXISTS FOR. When our side breaks, the
            customer must land in the 30-day grace period, not be told their
            licence is invalid. Dropping the table is a blunt way to break
            it, and a faithful one: the query throws exactly as it would in a
            real outage. */
        await env.DB.exec("drop table if exists licenses");

        const response = await post({ key: KEY, machineId: "m1" });

        expect(response.status).toBe(503);

        const body = (await response.json()) as { status?: string };

        // Not merely "not 200" - the body must not carry a decision at all.
        // A client that saw `rejected` here would disable preset saving over
        // an outage that lasted a minute.
        expect(body.status).toBeUndefined();
    });

    it("answers a broken activations table with 503 too", async () => {
        await env.DB.exec("drop table if exists activations");

        const response = await post({ key: KEY, machineId: "m1" });

        expect(response.status).toBe(503);
        expect(((await response.json()) as { status?: string }).status)
            .toBeUndefined();
    });
});

describe("routing", () => {
    it("has a health check", async () => {
        const response = await SELF.fetch("https://licence.test/health");

        expect(response.status).toBe(200);
        expect(await response.json()).toEqual({ ok: true });
    });

    it("404s anything else", async () => {
        const response = await SELF.fetch("https://licence.test/admin");
        expect(response.status).toBe(404);
    });
});
