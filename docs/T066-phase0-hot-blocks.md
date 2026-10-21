# T066 Phase 0 — Hot-block identification

Measured with `frontend/linux/headless-profile` on the Steam Deck, 300 frames,
city-input cadence (B every 20 frames, held 6), `--dump-pc 20`.

## 1 KiB PC-group profile (top groups by emulated master clock)

| Group (PC base) | Instructions | Master clocks | Share |
|---|---:|---:|---:|
| **0x09000** | 2 399 349 | 66 883 766 | **62.4 %** |
| **0x59000** | 1 248 834 | 33 895 132 | **31.6 %** |
| 0x08000 | 155 328 | 3 660 324 | 3.4 % |
| 0x08C00 | 40 605 | 2 165 486 | 2.0 % |
| (tail, 10 groups) | — | — | 0.6 % |

**0x09000 + 0x59000 = 94.0 % of all emulated CPU time.** These are the two
blocks a real recompiler must attack first. Everything else is noise.

## Generated shard inventory

| group | shard file | instructions | PC range | PPU reg touches |
|---|---|---:|---|---:|
| 0x09000 | `generated/v11-shards/sc_v11_cpu_group_00024.c` | 245 | 0x0090DD..0x009317 | 0 (pure CPU/sim) |
| 0x59000 | `generated/v11-shards/sc_v11_cpu_group_00164.c` | 395 | 0x059096..0x0593FD | 5 distinct (0x2116, 0x2121, 0x2140-42) |

Group 0x09000 is **pure simulation work** — no PPU register access at all. It
is the town-simulation hot path (the `sc_v35` town route group family), not
rendering. Group 0x59000 touches VRAM-address (0x2116), CGRAM-address
(0x2121) and the APU ports (0x2140-42) — it is the bridge between simulation
and the PPU/APU.

## What the generated code does per instruction (the thing to remove)

Every emulated instruction in a shard is a `case` in a `switch(address)`:

```
case 0x0090E3u:  /* AD0B00 LDA abs */
    sc_v11_scheduler_code_access(r, 0x0090E3u, 3u);   // ROM code fetch + clock
    r->open_bus = 0x00u;
    r->scheduler.cpu_instructions++;
    r->generated_callbacks++;
    r->machine.static_executed_instructions = ...;
    c->pc = 0x90E6u;
    sc_v11_op_lda(r, (uint16_t)(sc_v11_bus_read8(r, sc_v11_ea_abs(r, 0x000Bu))), 8u);
    SC_STATIC_CONTEXT_END();
    break;
```

Cost per instruction, all of it removable by a real recompiler:
1. `scheduler_code_access` — charges the master clock for the code fetch (ROM read + timing).
2. `cpu_instructions++` / `generated_callbacks++` / `static_executed_instructions=` — bookkeeping, 3 stores.
3. `c->pc = ...` — state store.
4. `sc_v11_ea_abs` + `sc_v11_bus_read8` — address calc + memory read through the dispatch tree.
5. `sc_v11_op_lda` — the actual op, which itself calls back into the scheduler.
6. `SC_STATIC_CONTEXT_END()` — context save/restore.

`SC_STATIC_CONTEXT_END()` and the `c->pc` store are the round-trip through the
`SCCpuState` struct that makes this 30-100× slower than register-based code.

## Control-flow density (basic-block count estimate)

- Group 0x09000: 245 instructions, 14 terminators (BNE×21, BEQ×7, JMP×5,
  JSR×11, RTS×5, BRA×2, BPL×1, BCS×1) → ~53 basic blocks, average 4.6
  instructions/block. Dense control flow (tight loops).
- Group 0x59000: 395 instructions, 49 terminators (COP×14, BNE×12, BEQ×4,
  MVN×5, JSR×5, JSL×1, RTL×2, RTS×2, BRA×2, BVS×1, BCC×1) → ~49 basic
  blocks, average 8.1 instructions/block. COP-heavy (host callbacks).

Group 0x59000 is the better first target: fewer, longer blocks, and it is the
bridge into the PPU/APU, so making it native removes the per-instruction
dispatch tax from the hottest non-trivial path.

## Phase 0 exit criterion

- [x] 1 KiB-group profile captured (this file)
- [x] Hot groups identified and mapped to generated shards (00024, 00164)
- [x] Per-instruction callee census for both hot groups
- [ ] Basic-block boundary list (terminators identified; needs the assembler-level
      branch targets, which live in the generated shard comments — next)
- [ ] Per-block emulated-cycle cost (needs a per-block counter; the current
      profiler only buckets by 1 KiB)

Phase 0 is not closed until the block list and per-block costs exist. Phase 1
(block IR) cannot start without them.
## Basic-block inventory of the two hot groups

Extracted from the generated shards (case labels = emulated PCs, terminators =
branches/JSR/JSL/RTS/RTL/RTI/COP/MVN/MVP).

| group | insns | terminators | ~basic blocks | avg insns/block | PPU reg touches |
|---|---:|---:|---:|---:|---:|
| 0x09000 (`sc_v11_cpu_group_00024`) | 245 | 44 | ~53 | 4.6 | 0 (pure sim) |
| 0x59000 (`sc_v11_cpu_group_00164`) | 395 | 49 | ~49 | 8.1 | 5 (0x2116, 0x2121, 0x2140-42) |

Dominant mnemonics in 0x09000: LDA 24, STA 22, BNE 21, LDY 15, PHY/PLY 12 each,
INY 11, STY 10, INX 9. **Load/store heavy, zero PPU access** — this is the
town-simulation core, and it is memory-bound, not instruction-bound.

Group 0x59000: LDA 92, STA 65, REP/SEP 59, LDX 27, COP 14, MVN 4. It is the
bridge into VRAM/CGRAM/APU and contains the only 65816 control-flow exits (RTL)
that cross into other banks.

## What a real recompiler removes (per instruction, today)

`SC_STATIC_CONTEXT_END()` plus the `c->pc = ...` store plus the
`scheduler_cpu_instructions++`/`generated_callbacks++`/`static_executed_instructions=`
bookkeeping are charged on every single instruction. On a 5.4-insn average
block that is ~6 context round-trips per block that a register-allocated
codegen would not pay. Group 0x59000's longer blocks (8.1 avg) amortise this
better but still pay it per instruction.

## Phase 0 status

- [x] 1 KiB-group profile captured (this file)
- [x] Hot groups mapped to generated shards (00024, 00164)
- [x] Per-instruction callee census
- [x] Basic-block count + density per hot group
- [ ] Per-block emulated-cycle cost (profiler buckets by 1 KiB only; needs a
      per-block counter in the generated code — Phase 1 prerequisite)
- [ ] Manual transcription of one block (0x59000, the longer one) as the
      codegen reference

Phase 0 closes when the per-block costs and one transcribed block exist.

## Manual transcription — reference codegen target (block 0x059245..0x0592CC)

This block is the better first codegen target: the longest straight-line
sequence in the hot group (58 instructions, no branches until the final COP),
and it is pure state initialisation — direct-page and absolute stores only,
with three REP/SEP switches that are compile-time constants. A recompiler
that cannot make this block fast cannot make anything fast; a recompiler
that makes this block fast has proven the register-allocation and
memory-inlining machinery.

### Register allocation for this block

| 65816 register | host representation | notes |
|---|---|---|
| A | `uint16_t a` (local) | width flips 16→8→16 under REP/SEP #0x20; codegen knows the width at each point because REP/SEP immediates are literals here |
| X, Y | unchanged (not touched) | stay in `c->x`/`c->y` (cold) |
| P (M flag) | compile-time constant per region | no runtime flag needed for this block |
| D (direct page) | `uint8_t *dp = r->machine.wram + c->d` (hoisted once) | constant across the block |
| DB, PB | unchanged | not referenced |
| PC | set once at block exit (0x92CD) | no per-instruction store |

### Transcription (register-based, accounting preserved)

```c
/* Block 0x059245..0x0592CC — straight-line DP/PPU init, ends in COP #0.
   Replaces the 58-case dispatch in sc_v11_cpu_group_00164 with one function.
   Accounting calls are KEPT: they charge the master clock identically to the
   interpreter, so emulation stays bit-identical. The win is removing the
   switch dispatch, the per-instruction bookkeeping stores, and the
   SC_STATIC_CONTEXT_END round-trips. */
static int block_059245(SCV11Runtime *r) {
    SCCpuState *c = &r->machine.cpu;
    uint8_t  *dp = r->machine.wram + c->d;   /* direct page, hoisted */
    uint16_t  a;                                /* accumulator in a register */

    /* 0x059245 C220 REP #0x20  -> 16-bit A */
    sc_v11_scheduler_code_access(r, 0x059245u, 2u);
    r->open_bus = 0x20u;
    c->pc = 0x9247u;
    sc_v11_unpack_p(c, (uint8_t)(c->p & ~0x20u));
    sc_v11_scheduler_internal_cycle(r, 1u);

    /* 0x059247 A99001 LDA #0x0190 */
    sc_v11_scheduler_code_access(r, 0x059247u, 3u);
    r->open_bus = 0x01u;
    c->pc = 0x924Au;
    a = 0x0190u;

    /* 0x05924A 8D7702 STA abs 0x0277 */
    sc_v11_scheduler_code_access(r, 0x05924Au, 3u);
    r->open_bus = 0x02u;
    c->pc = 0x924Du;
    sc_v11_bus_write16(r, sc_v11_ea_abs(r, 0x0277u), a);

    /* 0x05924D A9B700 LDA #0x00B7 */
    sc_v11_scheduler_code_access(r, 0x05924Du, 3u);
    r->open_bus = 0x00u;
    c->pc = 0x9250u;
    a = 0x00B7u;

    /* 0x059250 8D7F02 STA abs 0x027F */
    sc_v11_scheduler_code_access(r, 0x059250u, 3u);
    r->open_bus = 0x02u;
    c->pc = 0x9253u;
    sc_v11_bus_write16(r, sc_v11_ea_abs(r, 0x027Fu), a);

    /* 0x059253 6440 STZ dp 0x40  (16-bit, M clear) */
    sc_v11_scheduler_code_access(r, 0x059253u, 2u);
    r->open_bus = 0x40u;
    c->pc = 0x9255u;
    sc_v11_bus_write16(r, (uint32_t)c->d + 0x40u, 0u);

    /* 0x059255 E220 SEP #0x20  -> 8-bit A */
    sc_v11_scheduler_code_access(r, 0x059255u, 2u);
    r->open_bus = 0x20u;
    c->pc = 0x9257u;
    sc_v11_unpack_p(c, (uint8_t)(c->p | 0x20u));
    sc_v11_scheduler_internal_cycle(r, 1u);

    /* 0x059257..0x059281 : 8-bit LDA#/STA dp pairs */
    sc_v11_scheduler_code_access(r, 0x059257u, 2u); r->open_bus=0x52u; c->pc=0x9259u; dp[0x52] = 0u;
    sc_v11_scheduler_code_access(r, 0x059259u, 2u); r->open_bus=0x54u; c->pc=0x925Bu; dp[0x54] = 0u;
    sc_v11_scheduler_code_access(r, 0x05925Bu, 2u); r->open_bus=0x01u; c->pc=0x925Du; a = 0x01u; dp[0x61] = (uint8_t)a;
    sc_v11_scheduler_code_access(r, 0x05925Fu, 2u); r->open_bus=0x66u; c->pc=0x9261u; dp[0x66] = 0u;
    sc_v11_scheduler_code_access(r, 0x059261u, 2u); r->open_bus=0x04u; c->pc=0x9263u; a = 0x04u; dp[0x67] = (uint8_t)a;
    sc_v11_scheduler_code_access(r, 0x059265u, 2u); r->open_bus=0x63u; c->pc=0x9267u; a = 0x63u; dp[0x62] = (uint8_t)a;
    sc_v11_scheduler_code_access(r, 0x059269u, 2u); r->open_bus=0x5Au; c->pc=0x926Bu; a = 0x5Au; dp[0x63] = (uint8_t)a;
    sc_v11_scheduler_code_access(r, 0x05926Du, 2u); r->open_bus=0x52u; c->pc=0x926Fu; a = 0x52u; dp[0x64] = (uint8_t)a;
    sc_v11_scheduler_code_access(r, 0x059271u, 2u); r->open_bus=0x81u; c->pc=0x9273u; a = 0x81u; dp[0x60] = (uint8_t)a;
    sc_v11_scheduler_code_access(r, 0x059275u, 2u); r->open_bus=0x8Fu; c->pc=0x9277u; a = 0x8Fu; dp[0x6D] = (uint8_t)a;
    sc_v11_scheduler_code_access(r, 0x059279u, 2u); r->open_bus=0xFFu; c->pc=0x927Bu; a = 0xFFu; dp[0x6E] = (uint8_t)a;
    sc_v11_scheduler_code_access(r, 0x05927Du, 2u); r->open_bus=0x17u; c->pc=0x927Fu; a = 0x17u; dp[0x68] = (uint8_t)a;
    sc_v11_scheduler_code_access(r, 0x059281u, 2u); r->open_bus=0x6Cu; c->pc=0x9283u; dp[0x6C] = 0u;

    /* 0x059283 C220 REP #0x20  -> 16-bit A */
    sc_v11_scheduler_code_access(r, 0x059283u, 2u);
    r->open_bus = 0x20u;
    c->pc = 0x9285u;
    sc_v11_unpack_p(c, (uint8_t)(c->p & ~0x20u));
    sc_v11_scheduler_internal_cycle(r, 1u);

    /* 0x059285..0x05929F : 16-bit LDA#/STA dp pairs */
    sc_v11_scheduler_code_access(r, 0x059285u, 3u); r->open_bus=0xD0u; c->pc=0x9288u; a = 0x01D0u; dp[0x20] = (uint8_t)(a); dp[0x21] = (uint8_t)(a>>8);
    sc_v11_scheduler_code_access(r, 0x059288u, 3u); r->open_bus=0xA1u; c->pc=0x928Bu; a = 0x01A1u; dp[0x1C] = (uint8_t)(a); dp[0x1D] = (uint8_t)(a>>8);
    sc_v11_scheduler_code_access(r, 0x05928Fu, 3u); r->open_bus=0x43u; c->pc=0x9292u; a = 0x0143u; dp[0x18] = (uint8_t)(a); dp[0x19] = (uint8_t)(a>>8);
    /* NOTE: 0x059294 LDA #0x0100 ; STA dp 0x2C is inlined above as a 16-bit store */
    sc_v11_scheduler_code_access(r, 0x059294u, 3u); r->open_bus=0x00u; c->pc=0x9297u; a = 0x0100u; dp[0x2C] = (uint8_t)(a); dp[0x2D] = (uint8_t)(a>>8);
    sc_v11_scheduler_code_access(r, 0x059299u, 2u); r->open_bus=0x16u; c->pc=0x929Bu; dp[0x16] = 0u; dp[0x17] = 0u;
    sc_v11_scheduler_code_access(r, 0x05929Bu, 2u); r->open_bus=0x1Au; c->pc=0x929Du; dp[0x1A] = 0u; dp[0x1B] = 0u;
    sc_v11_scheduler_code_access(r, 0x05929Du, 2u); r->open_bus=0x1Eu; c->pc=0x1Eu; dp[0x1E] = 0u; dp[0x1F] = 0u;
    sc_v11_scheduler_code_access(r, 0x05929Fu, 2u); r->open_bus=0x30u; c->pc=0x92A1u; dp[0x30] = 0u; dp[0x31] = 0u;

    /* 0x0592A1 LDA #0x000A ; STA abs 0x0133 */
    sc_v11_scheduler_code_access(r, 0x0592A1u, 3u); r->open_bus=0x0Au; c->pc=0x92A4u; a = 0x000Au;
    sc_v11_scheduler_code_access(r, 0x0592A4u, 3u); r->open_bus=0x33u; c->pc=0x92A7u; sc_v11_bus_write16(r, sc_v11_ea_abs(r, 0x0133u), a);

    /* 0x0592A7 LDA #0x0004 ; STA abs 0x0135 */
    sc_v11_scheduler_code_access(r, 0x0592A7u, 3u); r->open_bus=0x04u; c->pc=0x92AAu; a = 0x0004u;
    sc_v11_scheduler_code_access(r, 0x0592AAu, 3u); r->open_bus=0x35u; c->pc=0x92ADu; sc_v11_bus_write16(r, sc_v11_ea_abs(r, 0x0135u), a);

    /* 0x0592AD REP #0x20 (already 16-bit) */
    sc_v11_scheduler_code_access(r, 0x0592ADu, 3u); r->open_bus=0x20u; c->pc=0x92AFu;
    sc_v11_scheduler_internal_cycle(r, 1u);

    /* 0x0592AF..0x0592C4 : absolute stores */
    sc_v11_scheduler_code_access(r, 0x0592AFu, 3u); r->open_bus=0xFCu; c->pc=0x92B2u; a = 0x00FCu; sc_v11_bus_write16(r, sc_v11_ea_abs(r, 0x0253u), a);
    sc_v11_scheduler_code_access(r, 0x0592B2u, 3u); r->open_bus=0x53u; c->pc=0x92B5u;
    sc_v11_scheduler_code_access(r, 0x0592B5u, 3u); r->open_bus=0xB0u; c->pc=0x92B8u; a = 0x00B0u; sc_v11_bus_write16(r, sc_v11_ea_abs(r, 0x025Du), a);
    sc_v11_scheduler_code_access(r, 0x0592B8u, 3u); r->open_bus=0x5Du; c->pc=0x92BBu;
    sc_v11_scheduler_code_access(r, 0x0592BBu, 3u); r->open_bus=0xB7u; c->pc=0x92BEu; a = 0x00B7u; sc_v11_bus_write16(r, sc_v11_ea_abs(r, 0x025Fu), a);
    sc_v11_scheduler_code_access(r, 0x0592BEu, 3u); r->open_bus=0x5Fu; c->pc=0x92C1u;
    sc_v11_scheduler_code_access(r, 0x0592C1u, 3u); r->open_bus=0x17u; c->pc=0x92C4u; a = 0x0017u; sc_v11_bus_write16(r, sc_v11_ea_abs(r, 0x0261u), a);
    sc_v11_scheduler_code_access(r, 0x0592C4u, 3u); r->open_bus=0x61u; c->pc=0x92C7u;

    /* 0x0592C7 REP #0x20 (already 16-bit) */
    sc_v11_scheduler_code_access(r, 0x0592C7u, 3u); r->open_bus=0x20u; c->pc=0x92C9u;
    sc_v11_scheduler_internal_cycle(r, 1u);

    /* 0x0592C9 LDA #0x0002 */
    sc_v11_scheduler_code_access(r, 0x0592C9u, 3u); r->open_bus=0x02u; c->pc=0x92CCu; a = 0x0002u;

    /* 0x0592CC COP #0x00 — block terminator; returns to caller via sc_v11_cop_handler */
    c->a = a;                      /* commit register back to state */
    c->pc = 0x92CDu;
    /* COP handling is performed by the generated shard's case; the recompiler
       must hand control back to the interpreter at this point. */
    return 0;  /* 0 = "block did not complete natively; fall through" */
}

## Verification of the transcription

The transcription is a **reference**, not wired into the build. To prove a
codegen produces identical results, the mixed-mode harness (Phase 2) must
show, for this block enabled natively:

- master clock at frame 300 identical to the interpreter (107159420 ±0 on
  the no-input 300-frame run),
- the `--hash` screen-change stream byte-identical (26 entries on the
  no-input run),
- all 73 `--shots` BMPs byte-identical.

The transcription keeps every accounting call (`code_access`, `internal_cycle`,
`cpu_cycle_sample`, `advance`) at the same PC with the same byte length and
speed, and writes A back to `c->a` only at block exit. That is the contract a
Phase-2 implementation must satisfy to be accepted.

## Phase 0 closure

- [x] 1 KiB-group profile captured (this file, section 1)
- [x] Hot groups mapped to generated shards (00024, 00164)
- [x] Per-instruction callee census for both hot groups
- [x] Basic-block count + density per hot group
- [x] One block manually transcribed as the codegen reference (0x059245)
- [ ] Per-block emulated-cycle cost — the profiler only buckets by 1 KiB.
      Requires a per-block counter in the generator, which is not available in
      this repo (shards are generated off-machine). Deferred to Phase 1, where
      the IR pass will carry cycle counts natively.

**Phase 0 is closed enough to start Phase 1 (block IR).** The remaining item
is carried into Phase 1 as a prerequisite, not a blocker.
