/* sc_recomp_native_ref.c — T066 Phase 2b reference: block 0x059245.
 *
 * Register-allocated native lowering of the 58-instruction initializer block
 * in group 0x59000 (shard sc_v11_cpu_group_00164.c).  A lives in a local
 * uint16_t for the whole block; the direct-page base is hoisted once; the M
 * flag is resolved at codegen time (the block is only entered with m==0 &&
 * x==0, per the shard guard, and REP/SEP immediates here are literals).
 *
 * Accounting calls are preserved at the same PCs with the same lengths and
 * speeds, so emulation stays bit-identical.  What is removed versus the
 * interpreted shard: the switch(address) dispatch, the m/x guard, the
 * cpu_instructions/generated_callbacks/static_executed_instructions
 * bookkeeping stores, and SC_STATIC_CONTEXT_END().
 *
 * This file is scaffolding for Phase 2b: it proves the lowering compiles and
 * documents the contract.  It is not yet wired into mixed-mode dispatch
 * (Phase 2c).
 */
#include "sc_v11_runtime.h"
#include "sc_core_internal.h"

int sc_recomp_block_0x059245(SCV11Runtime *r) {
    SCCpuState *c = &r->machine.cpu;
    uint8_t  *dp = r->machine.wram + c->d;   /* direct page, hoisted once */
    uint16_t  a  = c->a;                      /* accumulator in a register */
    /* Block entry contract (shard guard): m==0, x==0. */

    /* 0x059245 C220 REP #0x20  (M clear -> 16-bit A) */
    sc_v11_scheduler_code_access(r, 0x059245u, 2u);
    r->open_bus = 0x20u;
    c->pc = 0x9247u;
    sc_v11_unpack_p(c, (uint8_t)(c->p & (uint8_t)~0x20u));
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

    /* 0x059257 6452 STZ dp 0x52 ; 0x059259 6454 STZ dp 0x54 */
    sc_v11_scheduler_code_access(r, 0x059257u, 2u); r->open_bus = 0x52u; c->pc = 0x9259u; dp[0x52] = 0u;
    sc_v11_scheduler_code_access(r, 0x059259u, 2u); r->open_bus = 0x54u; c->pc = 0x925Bu; dp[0x54] = 0u;

    /* 0x05925B A901 LDA #0x01 ; 0x05925D 8561 STA dp 0x61 */
    sc_v11_scheduler_code_access(r, 0x05925Bu, 2u); r->open_bus = 0x01u; c->pc = 0x925Du; a = 0x01u; dp[0x61] = (uint8_t)a;
    /* 0x05925F 6466 STZ dp 0x66 */
    sc_v11_scheduler_code_access(r, 0x05925Fu, 2u); r->open_bus = 0x66u; c->pc = 0x9261u; dp[0x66] = 0u;
    /* 0x059261 A904 LDA #0x04 ; 0x059263 8567 STA dp 0x67 */
    sc_v11_scheduler_code_access(r, 0x059261u, 2u); r->open_bus = 0x04u; c->pc = 0x9263u; a = 0x04u; dp[0x67] = (uint8_t)a;
    /* 0x059265 A963 LDA #0x63 ; 0x059267 8562 STA dp 0x62 */
    sc_v11_scheduler_code_access(r, 0x059265u, 2u); r->open_bus = 0x63u; c->pc = 0x9267u; a = 0x63u; dp[0x62] = (uint8_t)a;
    /* 0x059269 A95A LDA #0x5A ; 0x05926B 8563 STA dp 0x63 */
    sc_v11_scheduler_code_access(r, 0x059269u, 2u); r->open_bus = 0x5Au; c->pc = 0x926Bu; a = 0x5Au; dp[0x63] = (uint8_t)a;
    /* 0x05926D A952 LDA #0x52 ; 0x05926F 8564 STA dp 0x64 */
    sc_v11_scheduler_code_access(r, 0x05926Du, 2u); r->open_bus = 0x52u; c->pc = 0x926Fu; a = 0x52u; dp[0x64] = (uint8_t)a;
    /* 0x059271 A981 LDA #0x81 ; 0x059273 8560 STA dp 0x60 */
    sc_v11_scheduler_code_access(r, 0x059271u, 2u); r->open_bus = 0x81u; c->pc = 0x9273u; a = 0x81u; dp[0x60] = (uint8_t)a;
    /* 0x059275 A98F LDA #0x8F ; 0x059277 856D STA dp 0x6D */
    sc_v11_scheduler_code_access(r, 0x059275u, 2u); r->open_bus = 0x8Fu; c->pc = 0x9277u; a = 0x8Fu; dp[0x6D] = (uint8_t)a;
    /* 0x059279 A9FF LDA #0xFF ; 0x05927B 856E STA dp 0x6E */
    sc_v11_scheduler_code_access(r, 0x059279u, 2u); r->open_bus = 0xFFu; c->pc = 0x927Bu; a = 0xFFu; dp[0x6E] = (uint8_t)a;
    /* 0x05927D A917 LDA #0x17 ; 0x05927F 8568 STA dp 0x68 */
    sc_v11_scheduler_code_access(r, 0x05927Du, 2u); r->open_bus = 0x17u; c->pc = 0x927Fu; a = 0x17u; dp[0x68] = (uint8_t)a;
    /* 0x059281 646C STZ dp 0x6C */
    sc_v11_scheduler_code_access(r, 0x059281u, 2u); r->open_bus = 0x6Cu; c->pc = 0x9283u; dp[0x6C] = 0u;

    /* 0x059283 C220 REP #0x20  -> 16-bit A */
    sc_v11_scheduler_code_access(r, 0x059283u, 2u);
    r->open_bus = 0x20u;
    c->pc = 0x9285u;
    sc_v11_unpack_p(c, (uint8_t)(c->p & (uint8_t)~0x20u));
    sc_v11_scheduler_internal_cycle(r, 1u);

    /* 0x059285 A9D001 LDA #0x01D0 ; 0x059288 8520 STA dp 0x20 (16-bit) */
    sc_v11_scheduler_code_access(r, 0x059285u, 3u); r->open_bus = 0xD0u; c->pc = 0x9288u; a = 0x01D0u;
    dp[0x20] = (uint8_t)(a); dp[0x21] = (uint8_t)(a >> 8);
    sc_v11_scheduler_code_access(r, 0x059288u, 3u); r->open_bus = 0x20u; c->pc = 0x928Bu; /* STA dp */ dp[0x20]=(uint8_t)(a); dp[0x21]=(uint8_t)(a>>8);

    /* 0x05928A A9A101 LDA #0x01A1 ; 0x05928D 851C STA dp 0x1C (16-bit) */
    sc_v11_scheduler_code_access(r, 0x05928Au, 3u); r->open_bus = 0xA1u; c->pc = 0x928Du; a = 0x01A1u;
    dp[0x1C] = (uint8_t)(a); dp[0x1D] = (uint8_t)(a >> 8);
    sc_v11_scheduler_code_access(r, 0x05928Du, 3u); r->open_bus = 0x1Cu; c->pc = 0x9290u; /* STA dp */ dp[0x1C]=(uint8_t)(a); dp[0x1D]=(uint8_t)(a>>8);

    /* 0x05928F A94301 LDA #0x0143 ; 0x059292 8518 STA dp 0x18 (16-bit) */
    sc_v11_scheduler_code_access(r, 0x05928Fu, 3u); r->open_bus = 0x43u; c->pc = 0x9292u; a = 0x0143u;
    dp[0x18] = (uint8_t)(a); dp[0x19] = (uint8_t)(a >> 8);
    sc_v11_scheduler_code_access(r, 0x059292u, 3u); r->open_bus = 0x18u; c->pc = 0x9295u; /* STA dp */ dp[0x18]=(uint8_t)(a); dp[0x19]=(uint8_t)(a>>8);

    /* 0x059294 A90100 LDA #0x0100 ; 0x059297 852C STA dp 0x2C (16-bit) */
    sc_v11_scheduler_code_access(r, 0x059294u, 3u); r->open_bus = 0x00u; c->pc = 0x9297u; a = 0x0100u;
    dp[0x2C] = (uint8_t)(a); dp[0x2D] = (uint8_t)(a >> 8);
    sc_v11_scheduler_code_access(r, 0x059297u, 3u); r->open_bus = 0x2Cu; c->pc = 0x929Au; /* STA dp */ dp[0x2C]=(uint8_t)(a); dp[0x2D]=(uint8_t)(a>>8);

    /* 0x059299 6416 STZ dp 0x16 ; 0x05929B 641A STZ dp 0x1A ; 0x05929D 641E STZ dp 0x1E ; 0x05929F 6430 STZ dp 0x30 */
    sc_v11_scheduler_code_access(r, 0x059299u, 2u); r->open_bus = 0x16u; c->pc = 0x929Bu; dp[0x16] = 0u; dp[0x17] = 0u;
    sc_v11_scheduler_code_access(r, 0x05929Bu, 2u); r->open_bus = 0x1Au; c->pc = 0x929Du; dp[0x1A] = 0u; dp[0x1B] = 0u;
    sc_v11_scheduler_code_access(r, 0x05929Du, 2u); r->open_bus = 0x1Eu; c->pc = 0x1Eu; dp[0x1E] = 0u; dp[0x1F] = 0u;
    sc_v11_scheduler_code_access(r, 0x05929Fu, 2u); r->open_bus = 0x30u; c->pc = 0x92A1u; dp[0x30] = 0u; dp[0x31] = 0u;

    /* 0x0592A1 LDA #0x000A ; 0x0592A4 STA abs 0x0133 */
    sc_v11_scheduler_code_access(r, 0x0592A1u, 3u); r->open_bus = 0x0Au; c->pc = 0x92A4u; a = 0x000Au;
    sc_v11_scheduler_code_access(r, 0x0592A4u, 3u); r->open_bus = 0x33u; c->pc = 0x92A7u; sc_v11_bus_write16(r, sc_v11_ea_abs(r, 0x0133u), a);

    /* 0x0592A7 LDA #0x0004 ; 0x0592AA STA abs 0x0135 */
    sc_v11_scheduler_code_access(r, 0x0592A7u, 3u); r->open_bus = 0x0Au; c->pc = 0x92AAu; a = 0x0004u;
    sc_v11_scheduler_code_access(r, 0x0592AAu, 3u); r->open_bus = 0x35u; c->pc = 0x92ADu; sc_v11_bus_write16(r, sc_v11_ea_abs(r, 0x0135u), a);

    /* 0x0592AD REP #0x20 (already 16-bit) */
    sc_v11_scheduler_code_access(r, 0x0592ADu, 3u); r->open_bus = 0x20u; c->pc = 0x92AFu;
    sc_v11_scheduler_internal_cycle(r, 1u);

    /* 0x0592AF..0x0592C4 : absolute stores */
    sc_v11_scheduler_code_access(r, 0x0592AFu, 3u); r->open_bus = 0xFCu; c->pc = 0x92B2u; a = 0x00FCu; sc_v11_bus_write16(r, sc_v11_ea_abs(r, 0x0253u), a);
    sc_v11_scheduler_code_access(r, 0x0592B2u, 3u); r->open_bus = 0x53u; c->pc = 0x92B5u;
    sc_v11_scheduler_code_access(r, 0x0592B5u, 3u); r->open_bus = 0xB0u; c->pc = 0x92B8u; a = 0x00B0u; sc_v11_bus_write16(r, sc_v11_ea_abs(r, 0x025Du), a);
    sc_v11_scheduler_code_access(r, 0x0592B8u, 3u); r->open_bus = 0x5Du; c->pc = 0x92BBu;
    sc_v11_scheduler_code_access(r, 0x0592BBu, 3u); r->open_bus = 0xB7u; c->pc = 0x92BEu; a = 0x00B7u; sc_v11_bus_write16(r, sc_v11_ea_abs(r, 0x025Fu), a);
    sc_v11_scheduler_code_access(r, 0x0592BEu, 3u); r->open_bus = 0x5Fu; c->pc = 0x92C1u;
    sc_v11_scheduler_code_access(r, 0x0592C1u, 3u); r->open_bus = 0x17u; c->pc = 0x92C4u; a = 0x0017u; sc_v11_bus_write16(r, sc_v11_ea_abs(r, 0x0261u), a);
    sc_v11_scheduler_code_access(r, 0x0592C4u, 3u); r->open_bus = 0x61u; c->pc = 0x92C7u;

    /* 0x0592C7 REP #0x20 (already 16-bit) */
    sc_v11_scheduler_code_access(r, 0x0592C7u, 3u); r->open_bus = 0x20u; c->pc = 0x92C9u;
    sc_v11_scheduler_internal_cycle(r, 1u);

    /* 0x0592C9 LDA #0x0002 */
    sc_v11_scheduler_code_access(r, 0x0592C9u, 3u); r->open_bus = 0x02u; c->pc = 0x92CCu; a = 0x0002u;

    /* 0x0592CC COP #0x00 — block terminator, fully inlined so the native
       block charges the same master clock as the interpreted case. */
    sc_v11_scheduler_code_access(r, 0x0592CCu, 2u);
    r->open_bus = 0x00u;
    c->pc = 0x92CEu;
    if (c->emulation) return 0;   /* emulation-mode COP is outside the route */
    if (!sc_v11_push8(r, c->pbr)) return 0;
    if (!sc_v11_push16(r, 0x92CEu)) return 0;
    if (!sc_v11_push8(r, c->p)) return 0;
    c->p |= SC_FLAG_I;
    c->p &= (uint8_t)~SC_FLAG_D;
    c->pbr = 0u;
    c->pc = (uint16_t)sc_v11_bus_read16(r, 0x00FFE4u);
    c->a = a;                     /* commit accumulator back to state */
    return 1;  /* 1 = block (incl. terminator) completed natively */
}
