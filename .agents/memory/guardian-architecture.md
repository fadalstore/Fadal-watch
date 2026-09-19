---
name: Guardian architecture
description: Durable product decisions for FadalWatch Network Guardian.
---

FadalWatch Network Guardian treats network monitoring as an evidence and explanation problem, not only a port-scanning problem. The product surface centers on posture, baseline drift, explainable alerts, activity history, and coverage.

**Why:** A raw scanner is easy to replace; a durable evidence model and clear operator decisions make the tool useful over time and give it a distinct product identity.

**How to apply:** New capabilities should persist observations, explain why they matter, and remain bounded to explicitly authorized targets. Avoid credential access, exploitation, unbounded scans, and opaque risk claims.