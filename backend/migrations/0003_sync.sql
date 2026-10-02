-- Preset sync (src/sync.ts, docs/design/phase8-01-sync.md). Run once on an
-- existing database:
--   npx wrangler d1 execute gnarl --remote --file=./migrations/0003_sync.sql

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
