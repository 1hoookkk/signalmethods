import { resolve } from "node:path";
import { defineConfig } from "vitest/config";

const here = import.meta.dirname;
const server = "http://127.0.0.1:8787";

export default defineConfig({
  root: here,
  build: {
    outDir: "dist",
    emptyOutDir: true,
    target: "es2022",
    rollupOptions: {
      input: {
        main: resolve(here, "index.html"),
        parity: resolve(here, "parity.html"),
      },
    },
  },
  worker: { format: "es" },
  server: {
    port: 5173,
    proxy: {
      "/api": { target: server, changeOrigin: false },
      "/ref": { target: server, changeOrigin: false },
    },
  },
  test: {
    include: ["tests/**/*.test.ts"],
    environment: "node",
  },
});
