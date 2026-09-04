// Forwards to the repo-root config so cosmiconfig resolves "extends" against
// ui/node_modules (where @commitlint/config-conventional is installed), not
// the repo root where the gate's node_modules-less commitlint.config.mjs lives.
export { default } from "../commitlint.config.mjs";
