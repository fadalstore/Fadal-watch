---
name: Guardian local setup
description: First-run requirements for the imported Guardian workspace.
---

The Guardian dashboard depends on both the workspace dependencies and the development PostgreSQL schema before its data routes can serve successfully.

**Why:** The frontend and API processes can start while dependencies or `guardian_*` tables are still missing, which otherwise appears as a blank/loading dashboard or API 500 responses.

**How to apply:** On a fresh workspace, install from the lockfile, apply the development schema using the documented DB push command, then verify `/api/healthz` and a data route before diagnosing frontend rendering.