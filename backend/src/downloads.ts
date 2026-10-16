/*
    BUYERS-ONLY DOWNLOADS.

        GET /download?os=windows|macos&session_id=cs_...   (the thank-you page)
        GET /download?os=windows|macos&key=GNARL-...       (the key's email)

    The zips live in a PRIVATE R2 bucket (binding DOWNLOADS; the release
    workflow uploads each release's into it). Nothing public points at them:
    this endpoint streams one only for a licence that exists and is active.
    Both credentials are already secrets the buyer holds - the checkout
    session id Stripe put in their return URL, the 80-bit key in their email -
    so there is no extra token to issue, expire or leak.

    A refunded or revoked licence downloads nothing. Without the bucket (R2
    not enabled yet) the answer is 503, never a wrong 403: "not available"
    is a server state, "not yours" is a statement about the buyer.

    GNARL is GPLv3, so a buyer may pass the zip on. This makes buying the
    easy path; it does not pretend to prevent copying (CLAUDE.md section 8).
*/

import type { Env } from "./index";

const FILES: Record<string, string> = {
    windows: "GNARL-Windows.zip",
    macos: "GNARL-macOS.zip",
};

const text = (body: string, status: number): Response =>
    new Response(body + "\n", {
        status,
        headers: { "content-type": "text/plain; charset=utf-8", "cache-control": "no-store" },
    });

/** The download URL a buyer is given, on this Worker's own origin. */
export function downloadUrl(origin: string, os: "windows" | "macos", key: string): string {
    return `${origin}/download?os=${os}&key=${encodeURIComponent(key)}`;
}

export async function download(request: Request, env: Env): Promise<Response> {
    const params = new URL(request.url).searchParams;
    const file = FILES[params.get("os") ?? ""];
    if (!file) return text("Choose os=windows or os=macos.", 400);

    const session = params.get("session_id") ?? "";
    const key = (params.get("key") ?? "").trim().toUpperCase();
    let column: string;
    let value: string;
    if (/^cs_[A-Za-z0-9_]{8,200}$/.test(session)) {
        [column, value] = ["stripe_session_id", session];
    } else if (/^GNARL-[0-9A-Z]{4}(-[0-9A-Z]{4}){3}$/.test(key)) {
        [column, value] = ["key", key];
    } else {
        return text("This download link is not complete. Use the link from your GNARL email or the thank-you page.", 400);
    }

    let row: { status: string } | null;
    try {
        row = await env.DB.prepare(`select status from licenses where ${column} = ?`).bind(value).first<{ status: string }>();
    } catch (error) {
        console.error("download lookup failed", error);
        return text("Downloads are temporarily unavailable. Please try again in a minute.", 503);
    }
    // A session the webhook has not reached yet reads as not found too: the
    // thank-you page only shows these links once the key has arrived.
    if (row === null) return text("This download link is not valid.", 403);
    if (row.status !== "active") return text(`This licence is ${row.status}, so it has no downloads.`, 403);

    if (!env.DOWNLOADS) {
        console.error("download asked for, but no DOWNLOADS bucket is bound");
        return text("Downloads are not available yet. Your key is valid: please reply to your receipt email.", 503);
    }
    const object = await env.DOWNLOADS.get(file);
    if (object === null) {
        console.error(`download: ${file} is not in the bucket`);
        return text("This download is not available yet. Please try again later.", 503);
    }

    return new Response(object.body, {
        headers: {
            "content-type": "application/zip",
            "content-length": String(object.size),
            "content-disposition": `attachment; filename="${file}"`,
            // Private: never kept by a shared cache, where the next person
            // asking would be served it without a licence.
            "cache-control": "private, no-store",
            etag: object.httpEtag,
        },
    });
}
