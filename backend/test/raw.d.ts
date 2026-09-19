/*  A GLOBAL declaration file - no imports, deliberately. A `.d.ts` with a
    top-level import becomes a module, and a wildcard `declare module`
    inside one is not applied to the `?raw` suffix by the bundler
    resolver. Keeping this file import-free is what makes it take effect.

    Vite serves the schema as a string so the tests build their database
    from the same file that is deployed, rather than from a second copy
    that would drift from it. */
declare module "*.sql?raw" {
    const content: string;
    export default content;
}
