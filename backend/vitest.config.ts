import { defineWorkersConfig } from "@cloudflare/vitest-pool-workers/config";

/*  Runs the Worker in workerd - the real runtime, not a mock - against a
    real local D1. So unlike the Supabase deployment, which this sandbox's
    egress proxy cannot reach, these tests exercise the actual HTTP handler
    and the actual SQL. */
export default defineWorkersConfig({
  test: {
    poolOptions: {
      workers: {
        wrangler: { configPath: "./wrangler.toml" },
      },
    },
  },
});
