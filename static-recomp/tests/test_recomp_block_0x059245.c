/* test_recomp_block_0x059245.c — T066 Phase 2d: byte-identity gate for the
 * first register-allocated native block.
 *
 * Runs block 0x059245 once through the interpreted shard and once through the
 * native lowering, from an identical CPU/WRAM/PPU state, and asserts:
 *   - both complete (return 1)
 *   - master-clock delta is identical
 *   - the WRAM region the block touches is byte-identical afterwards
 *
 * This is the smallest honest proof that a register-allocated block does not
 * diverge from the interpreter.  It is gated on SIMCITY_TEST_ROM because it
 * needs a real ROM to initialise the runtime.
 */
#include "simcity_static_recomp.h"
#include "sc_v11_runtime.h"
#include "sc_machine.h"
#include "sc_recomp_native.h"

extern int sc_v11_cpu_group_00164(SCV11Runtime *, uint32_t, uint8_t, uint8_t);

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static int failures;
#define CHECK(cond, msg) do { if (!(cond)) { fprintf(stderr, "FAIL: %s\n", (msg)); failures++; } } while (0)

static uint32_t wram_hash(const SCV11Runtime *r) {
    /* FNV-1a over the direct-page window the block writes (0x00..0x7F). */
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < 0x80u; i++) {
        h ^= r->machine.wram[i];
        h *= 16777619u;
    }
    return h;
}

static void setup(SCV11Runtime *r) {
    /* Block entry contract from the shard guard: m==0, x==0. */
    r->machine.cpu.a = 0u;
    r->machine.cpu.x = 0u;
    r->machine.cpu.y = 0u;
    r->machine.cpu.sp = 0x01FFu;
    r->machine.cpu.d = 0u;
    r->machine.cpu.pc = 0x9245u;
    r->machine.cpu.pbr = 0x05u;
    r->machine.cpu.dbr = 0x00u;
    r->machine.cpu.p = 0u;          /* M and X clear -> 16-bit */
    r->machine.cpu.emulation = 0u;
}

int main(int argc, char **argv) {
    FILE *f;
    long sz;
    uint8_t *rom;
    SCV11Runtime *rt_interp, *rt_native;

    if (argc < 2) { fprintf(stderr, "Usage: %s <rom>\n", argv[0]); return 1; }
    f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot open ROM\n"); return 1; }
    fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz != (long)SIMCITY_RECOMP_ROM_SIZE) { fprintf(stderr, "ROM size mismatch\n"); fclose(f); return 1; }
    rom = malloc((size_t)sz);
    if (!rom || fread(rom, 1, (size_t)sz, f) != (size_t)sz) { fprintf(stderr, "ROM read failed\n"); return 1; }
    fclose(f);

    rt_interp = calloc(1, sizeof(*rt_interp));
    rt_native = calloc(1, sizeof(*rt_native));
    if (!rt_interp || !rt_native) { fprintf(stderr, "oom\n"); return 1; }

    sc_v11_runtime_init(rt_interp, rom, (size_t)sz);
    sc_v11_runtime_init(rt_native,  rom, (size_t)sz);
    setup(rt_interp);
    setup(rt_native);

    /* Interpreted path */
    uint64_t mc0i = rt_interp->scheduler.master_clock;
    uint32_t h0i = wram_hash(rt_interp);
    int rc_i = sc_v11_cpu_group_00164(rt_interp, 0x059245u, 0u, 0u);
    uint64_t mc1i = rt_interp->scheduler.master_clock;
    uint32_t h1i = wram_hash(rt_interp);

    /* Native path */
    uint64_t mc0n = rt_native->scheduler.master_clock;
    uint32_t h0n = wram_hash(rt_native);
    int rc_n = sc_recomp_block_0x059245(rt_native);
    uint64_t mc1n = rt_native->scheduler.master_clock;
    uint32_t h1n = wram_hash(rt_native);

    CHECK(rc_i == 1, "interpreted block completed");
    CHECK(rc_n == 1, "native block completed");
    CHECK((mc1i - mc0i) == (mc1n - mc0n), "master-clock delta identical");
    CHECK(h1i == h1n, "WRAM (direct page) byte-identical");

    printf("interp: rc=%d dmc=%llu whash 0x%08X->0x%08X\n",
           rc_i, (unsigned long long)(mc1i-mc0i), h0i, h1i);
    printf("native: rc=%d dmc=%llu whash 0x%08X->0x%08X\n",
           rc_n, (unsigned long long)(mc1n-mc0n), h0n, h1n);

    free(rom); /* rom already freed conceptually; runtime owns its copy */
    return failures ? 1 : 0;
}
