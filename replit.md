# FadalWatch Network Guardian

FadalWatch Network Guardian helps authorized operators understand network posture, detect baseline drift, and take safe, evidence-based action.

## Run & Operate

- `pnpm --filter @workspace/api-server run dev` — run the API server
- `pnpm --filter @workspace/fadalwatch-guardian run dev` — run the Guardian web dashboard
- `pnpm run typecheck` — full typecheck across all packages
- `pnpm run build` — typecheck + build all packages
- `pnpm --filter @workspace/api-spec run codegen` — regenerate API hooks and Zod schemas from the OpenAPI spec
- `pnpm --filter @workspace/db run push` — push DB schema changes (dev only)
- `make -C kernel` — build the standalone Fadal Kernel disk image
- `make -C kernel check` — boot-test the kernel in QEMU
- `make -C kernel run` — run the kernel in QEMU with serial output
- Required env: `DATABASE_URL` — Postgres connection string

## Stack

- pnpm workspaces, Node.js 24, TypeScript 5.9
- API: Express 5
- DB: PostgreSQL + Drizzle ORM
- Validation: Zod (`zod/v4`), `drizzle-zod`
- API codegen: Orval (from OpenAPI spec)
- Build: esbuild (CJS bundle)
- Kernel foundation: freestanding C, GNU binutils, BIOS boot path, QEMU

## Where things live

- `lib/api-spec/openapi.yaml` — source of truth for the Guardian API contract
- `lib/db/src/schema/guardian.ts` — PostgreSQL schema for assets, services, alerts, activity, scans, and coverage
- `artifacts/api-server/src/routes/guardian.ts` — Guardian API and safe observation engine
- `artifacts/fadalwatch-guardian/src/` — responsive Guardian dashboard and route views

## Architecture decisions

- The dashboard is API-first: OpenAPI generates the React Query client and Zod validators.
- Guardian stores observations as durable evidence in PostgreSQL instead of relying on browser state.
- Observation runs require an explicit authorization confirmation and only probe selected TCP ports.
- Broad network scopes are rejected above 64 IPv4 addresses to keep each run deliberate and bounded.

## Product

- Posture overview with health signal, exposure pressure, coverage, trend, and recent evidence.
- Searchable authorized asset inventory with baseline state, risk, services, and history.
- Explainable alert queue with evidence, recommendations, acknowledgement, and resolution.
- Activity ledger and zone coverage views.
- Safe observation runs for a single IPv4 address or bounded IPv4 CIDR.

## User preferences

_Populate as you build — explicit user instructions worth remembering across sessions._

## Gotchas

- After changing `lib/api-spec/openapi.yaml`, run codegen before using updated hooks or validators.
- After changing `lib/db/src/schema`, run `pnpm --filter @workspace/db run push` and rebuild library declarations.
- Observation runs are intentionally limited to 64 addresses and do not authenticate, exploit, or modify services.
- The standalone kernel currently targets the x86 BIOS path in 32-bit protected mode; it is independent of Linux and Ubuntu, owns timer IRQ0 and a future-user syscall gate, and does not yet provide userspace, filesystems, or drivers.

## Pointers

- See the `pnpm-workspace` skill for workspace structure, TypeScript setup, and package details
