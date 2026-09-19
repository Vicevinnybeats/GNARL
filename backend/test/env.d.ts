/*  Types the test harness needs and cannot infer.

    The Worker declares its own `Env` in src/index.ts rather than depending
    on wrangler's generated `worker-configuration.d.ts`, so nothing here
    needs a generated file to be present in a fresh clone. */

import type { Env } from "../src/index";

declare module "cloudflare:test" {
    interface ProvidedEnv extends Env {}
}
