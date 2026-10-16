# T040: Per-pixel divisions in the background sampler

## Context
T039 established that rendering is ~71 % of the frame budget and that
`background_pixel` is called three times per pixel (main layer, sub layer,
colour windows). It carried four divisions by runtime values on every call.

## Claim
"tile_size and the map extents are always powers of two, so the divisions can
be replaced by shifts with identical results."

## Evidence
- `map_width  = (sc & 1u) ? 64u : 32u`
- `map_height = (sc & 2u) ? 64u : 32u`
- `tile_size  = (bgmode & (0x10u << layer)) ? 16u : 8u`, forced to 16 in modes 5/6

So `world_width` and `world_height` are always in {256, 512, 1024, 2048}.
`world_y` is `(sample_y + scroll) & (world_height - 1)`; both operands are
unsigned, so the value is non-negative and within range.

## Verification (the part that mattered)
The first attempt set `tile_shift = tile_size >> 3`, which yields 1 for 8 and 2
for 16 — **wrong**, because log2(8) = 3 and log2(16) = 4. The frame-hash
comparison caught it immediately: 381 changed frames versus 336 for the
baseline.

This is the concrete case for AES verify: the transformation looked obviously
correct in reasoning and was silently wrong in code.

Checks run before accepting the change:
1. Harness determinism — two runs of the same binary produce identical hashes.
2. Baseline reproducibility — the reverted build reproduces the original hashes.
3. Bisection — `world_y` mask alone: identical. Tile shift alone: differs.
4. Corrected shift (`tile_size == 16u ? 4u : 3u`): identical across 950 frames.

## Result
- Bit-identical output over 950 frames.
- 18.12 → 15.44 ms/frame (-14.7 %).
- Pure emulation unchanged at 3.58 ms/frame, confirming the gain is in render.
- All 8 ctest gates pass; `headless-test` advances 900 frames cleanly.

## Remaining candidates
`city_background_pixel` computes map coordinates with the same divide/modulo
pattern for the widescreen path and should be audited the same way, with the
same hash-based verification and the same caution about log2 values.