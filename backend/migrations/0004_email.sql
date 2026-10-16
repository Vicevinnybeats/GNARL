-- The licence email (src/stripe.ts emailLicence) on a database created before it.
-- Run once:  npx wrangler d1 execute gnarl --remote --file=./migrations/0004_email.sql
alter table licenses add column emailed_at text;
