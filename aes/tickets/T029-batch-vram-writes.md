---
ticket: T029
title: Batch VRAM writes during city load
type: feature
phase: plan
status: open
priority: high
closure_condition: "VRAM tile‑map initialisation uses memcpy/word stores instead of byte‑wise stores; headless‑profile load window render spikes drop below 1 ms/frame."
---

## Context
PC‑group profile (window 300‑900) shows group **0x09000** ≈ 89 % of cycles. This 1 KiB window corresponds to generated code that writes VRAM / builds the tile map byte‑by‑byte.

## Acceptance criteria
1. Identify the generated C functions in `static-recomp/generated/` that belong to group 0x09000.
2. Replace per‑byte VRAM stores with `memcpy` / 16‑bit/32‑bit stores where alignment permits.
3. Re‑run the load‑window profile (`--window 300:900 --dump-pc 200`) and verify render time ≤ 1 ms/frame on average.
4. All existing tests (`ctest --test-dir build`) still pass.