-- GNARL licensing, on Cloudflare D1.
--
-- The policy this serves is CLAUDE.md section 9 and is not negotiable: the
-- licence check NEVER silences the plugin. Nothing here can, because the
-- plugin's answer to "unreachable" is a 30-day grace period and its answer
-- to "expired" is to disable preset saving and keep playing.
--
-- D1 is SQLite, so: no uuid type (text ids generated in the Worker), no
-- timestamptz (ISO-8601 strings in UTC), and foreign keys need to be enabled
-- per connection - which the Worker binding does by default.

create table if not exists licenses (
    id                     text primary key,

    -- What the customer is given. Unique, because it IS the identity: there
    -- are no accounts in the licensing path and none are needed.
    key                    text not null unique,

    email                  text,
    stripe_customer_id     text,
    stripe_subscription_id text,
    -- The Checkout Session that paid for this licence: unique, which is what
    -- makes Stripe's repeated webhooks issue ONE key (src/stripe.ts), and how
    -- the thank-you page finds the key. The payment intent is what a refund
    -- names. Added by migrations/0002_stripe.sql on an existing database.
    stripe_session_id      text unique,
    stripe_payment_intent  text,

    -- Distinct rather than a boolean. `revoked` is a leaked key and
    -- `refunded` is a chargeback; they mean different things to whoever is
    -- answering the support email, and one bit loses that immediately.
    status                 text not null default 'active'
                           check (status in ('active', 'revoked', 'refunded', 'expired')),

    -- A desktop, a laptop, and the one they buy next year. A limit low
    -- enough to bite is a limit that generates tickets from honest people.
    max_activations        integer not null default 3
                           check (max_activations > 0),

    created_at             text not null default (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    notes                  text
);

create table if not exists activations (
    id            text primary key,
    license_id    text not null references licenses (id) on delete cascade,

    -- A hash of a machine fingerprint, never anything identifying. The
    -- server cannot tell whose machine this is and does not need to.
    machine_id    text not null,

    -- What a customer sees in an activations list, so deactivating the right
    -- one does not require guessing.
    machine_label text,

    first_seen    text not null default (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    last_seen     text not null default (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),

    -- LOAD-BEARING. The plugin re-checks periodically; without this a
    -- customer checking in daily would exhaust a three-machine licence in
    -- three days. A re-check updates last_seen instead of taking a seat.
    unique (license_id, machine_id)
);

create index if not exists activations_license_id_idx on activations (license_id);
create index if not exists licenses_stripe_customer_idx on licenses (stripe_customer_id);
create index if not exists licenses_stripe_payment_intent_idx on licenses (stripe_payment_intent);

-- Preset sync (src/sync.ts, docs/design/phase8-01-sync.md). Added by
-- migrations/0003_sync.sql on an existing database.

-- A list of patches, opened by a sync code. `id` is a SHA-256 of the code:
-- the code itself is never stored, so a copy of this table opens nothing.
create table if not exists sync_spaces (
    id         text primary key,
    created_at text not null default (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    last_seen  text
);

create table if not exists sync_presets (
    space_id   text not null references sync_spaces (id) on delete cascade,
    name       text not null,
    -- The patch as the plugin saves it (JSON text), about 180 kB.
    patch      text not null,
    size       integer not null,
    updated_at text not null,
    primary key (space_id, name)
);

-- New codes per hashed address per hour; rows older than a day are deleted
-- by the next request for a code.
create table if not exists sync_rate (
    bucket text primary key,
    hour   integer not null,
    n      integer not null
);
