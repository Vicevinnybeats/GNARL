-- Payments (src/stripe.ts) on a database created before them.
-- SQLite cannot add a UNIQUE column with ALTER TABLE, so the uniqueness is a
-- unique index, which enforces the same thing and serves the lookups.
-- Run once:  npx wrangler d1 execute gnarl --remote --file=./migrations/0002_stripe.sql
alter table licenses add column stripe_session_id text;
alter table licenses add column stripe_payment_intent text;
create unique index if not exists licenses_stripe_session_idx on licenses (stripe_session_id);
create index if not exists licenses_stripe_payment_intent_idx on licenses (stripe_payment_intent);
