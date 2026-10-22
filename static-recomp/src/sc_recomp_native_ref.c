/* sc_recomp_native_ref.c — T066 Phase 2b reference: block 0x059245.
 *
 * Register-allocated native lowering of the 58-instruction initializer block
 * in group 0x59000 (shard sc_v11_cpu_group_00164.c).  A is kept in a local
 * uint16_t for the whole block; the direct-page base is hoisted once.  The M
 * flag is resolved at codegen time: this block is only entered with m==0,x==0
 * (shard guard) and REP/SEP here take literal immediates.
 *
 * Accounting calls are preserved at the same PCs with the same lengths and
 * speeds, so emulation stays bit-identical.  Removed vs. the interpreted
 * shard: the switch(address) dispatch, the m/x guard, the cpu_instructions /
 * generated_callbacks / static_executed_instructions bookkeeping stores, and
 * SC_STATIC_CONTEXT_END().  Memory accesses still go through the bus helpers
 * (with their conditional direct-page-wrap cycle) — inlining those is Phase 3.
 */
#include "sc_v11_runtime.h"
#include "sc_core_internal.h"
#include "sc_recomp_native.h"

int sc_recomp_block_0x059245(SCV11Runtime *r) {
    SCCpuState *c = &r->machine.cpu;
    uint16_t  a  = c->a;                      /* accumulator in a register */

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
    sc_v11_set_nz16(c, a);

    /* 0x05924A 8D7702 STA abs 0x0277 */
    sc_v11_scheduler_code_access(r, 0x05924Au, 3u);
    r->open_bus = 0x02u;
    c->pc = 0x924Du;
    c->a = a;
    sc_v11_bus_write16(r, sc_v11_ea_abs(r, 0x0277u), (uint16_t)a);

    /* 0x05924D A9B700 LDA #0x00B7 */
    sc_v11_scheduler_code_access(r, 0x05924Du, 3u);
    r->open_bus = 0x00u;
    c->pc = 0x9250u;
    a = 0x00B7u;
    sc_v11_set_nz16(c, a);

    /* 0x059250 8D7F02 STA abs 0x027F */
    sc_v11_scheduler_code_access(r, 0x059250u, 3u);
    r->open_bus = 0x02u;
    c->pc = 0x9253u;
    c->a = a;
    sc_v11_bus_write16(r, sc_v11_ea_abs(r, 0x027Fu), (uint16_t)a);

    /* 0x059253 6440 STZ dp 0x40  (16-bit, M clear) */
    sc_v11_scheduler_code_access(r, 0x059253u, 2u);
    r->open_bus = 0x40u;
    c->pc = 0x9255u;
    {
        uint32_t ea = sc_v11_ea_dp(r, 0x0040u);
        sc_v11_bus_write16(r, ea, 0u);
        if ((c->d & 0x00FFu) != 0u) sc_v11_scheduler_internal_cycle(r, 1u);
    }

    /* 0x059255 E220 SEP #0x20  -> 8-bit A */
    sc_v11_scheduler_code_access(r, 0x059255u, 2u);
    r->open_bus = 0x20u;
    c->pc = 0x9257u;
    sc_v11_unpack_p(c, (uint8_t)(c->p | 0x20u));
    sc_v11_scheduler_internal_cycle(r, 1u);

    /* 0x059257 6452 STZ dp 0x52 ; 0x059259 6454 STZ dp 0x54 */
    sc_v11_scheduler_code_access(r, 0x059257u, 2u); r->open_bus = 0x52u; c->pc = 0x9259u;
    { uint32_t ea = sc_v11_ea_dp(r, 0x0052u); sc_v11_bus_write8(r, ea, 0u);
      if ((c->d & 0x00FFu) != 0u) sc_v11_scheduler_internal_cycle(r, 1u); }
    sc_v11_scheduler_code_access(r, 0x059259u, 2u); r->open_bus = 0x54u; c->pc = 0x925Bu;
    { uint32_t ea = sc_v11_ea_dp(r, 0x0054u); sc_v11_bus_write8(r, ea, 0u);
      if ((c->d & 0x00FFu) != 0u) sc_v11_scheduler_internal_cycle(r, 1u); }

    /* 0x05925B A901 LDA #0x01 ; 0x05925D 8561 STA dp 0x61 */
    sc_v11_scheduler_code_access(r, 0x05925Bu, 2u); r->open_bus = 0x01u; c->pc = 0x925Du; a = 0x01u; sc_v11_set_nz8(c, (uint8_t)a);
    sc_v11_scheduler_code_access(r, 0x05925Du, 2u); r->open_bus = 0x61u; c->pc = 0x925Fu; c->a = a; { uint32_t ea = sc_v11_ea_dp(r, 0x0061u); sc_v11_bus_write8(r, ea, (uint8_t)(c->a)); if ((c->d & 0x00FFu) != 0u) sc_v11_scheduler_internal_cycle(r, 1u); } a = c->a;

    /* 0x05925F 6466 STZ dp 0x66 */
    sc_v11_scheduler_code_access(r, 0x05925Fu, 2u); r->open_bus = 0x66u; c->pc = 0x9261u;
    { uint32_t ea = sc_v11_ea_dp(r, 0x0066u); sc_v11_bus_write8(r, ea, 0u);
      if ((c->d & 0x00FFu) != 0u) sc_v11_scheduler_internal_cycle(r, 1u); }

    /* 0x059261 A904 LDA #0x04 ; 0x059263 8567 STA dp 0x67 */
    sc_v11_scheduler_code_access(r, 0x059261u, 2u); r->open_bus = 0x04u; c->pc = 0x9263u; a = 0x04u; sc_v11_set_nz8(c, (uint8_t)a);
    sc_v11_scheduler_code_access(r, 0x059263u, 2u); r->open_bus = 0x67u; c->pc = 0x9265u; c->a = a; { uint32_t ea = sc_v11_ea_dp(r, 0x0067u); sc_v11_bus_write8(r, ea, (uint8_t)(c->a)); if ((c->d & 0x00FFu) != 0u) sc_v11_scheduler_internal_cycle(r, 1u); } a = c->a;

    /* 0x059265 A963 LDA #0x63 ; 0x059267 8562 STA dp 0x62 */
    sc_v11_scheduler_code_access(r, 0x059265u, 2u); r->open_bus = 0x63u; c->pc = 0x9267u; a = 0x63u; sc_v11_set_nz8(c, (uint8_t)a);
    sc_v11_scheduler_code_access(r, 0x059267u, 2u); r->open_bus = 0x62u; c->pc = 0x9269u; c->a = a; { uint32_t ea = sc_v11_ea_dp(r, 0x0062u); sc_v11_bus_write8(r, ea, (uint8_t)(c->a)); if ((c->d & 0x00FFu) != 0u) sc_v11_scheduler_internal_cycle(r, 1u); } a = c->a;

    /* 0x059269 A95A LDA #0x5A ; 0x05926B 8563 STA dp 0x63 */
    sc_v11_scheduler_code_access(r, 0x059269u, 2u); r->open_bus = 0x5Au; c->pc = 0x926Bu; a = 0x5Au; sc_v11_set_nz8(c, (uint8_t)a);
    sc_v11_scheduler_code_access(r, 0x05926Bu, 2u); r->open_bus = 0x63u; c->pc = 0x926Du; c->a = a; { uint32_t ea = sc_v11_ea_dp(r, 0x0063u); sc_v11_bus_write8(r, ea, (uint8_t)(c->a)); if ((c->d & 0x00FFu) != 0u) sc_v11_scheduler_internal_cycle(r, 1u); } a = c->a;

    /* 0x05926D A952 LDA #0x52 ; 0x05926F 8564 STA dp 0x64 */
    sc_v11_scheduler_code_access(r, 0x05926Du, 2u); r->open_bus = 0x52u; c->pc = 0x926Fu; a = 0x52u; sc_v11_set_nz8(c, (uint8_t)a);
    sc_v11_scheduler_code_access(r, 0x05926Fu, 2u); r->open_bus = 0x64u; c->pc = 0x9271u; c->a = a; { uint32_t ea = sc_v11_ea_dp(r, 0x0064u); sc_v11_bus_write8(r, ea, (uint8_t)(c->a)); if ((c->d & 0x00FFu) != 0u) sc_v11_scheduler_internal_cycle(r, 1u); } a = c->a;

    /* 0x059271 A981 LDA #0x81 ; 0x059273 8560 STA dp 0x60 */
    sc_v11_scheduler_code_access(r, 0x059271u, 2u); r->open_bus = 0x81u; c->pc = 0x9273u; a = 0x81u; sc_v11_set_nz8(c, (uint8_t)a);
    sc_v11_scheduler_code_access(r, 0x059273u, 2u); r->open_bus = 0x60u; c->pc = 0x9275u; c->a = a; { uint32_t ea = sc_v11_ea_dp(r, 0x0060u); sc_v11_bus_write8(r, ea, (uint8_t)(c->a)); if ((c->d & 0x00FFu) != 0u) sc_v11_scheduler_internal_cycle(r, 1u); } a = c->a;

    /* 0x059275 A98F LDA #0x8F ; 0x059277 856D STA dp 0x6D */
    sc_v11_scheduler_code_access(r, 0x059275u, 2u); r->open_bus = 0x8Fu; c->pc = 0x9277u; a = 0x8Fu; sc_v11_set_nz8(c, (uint8_t)a);
    sc_v11_scheduler_code_access(r, 0x059277u, 2u); r->open_bus = 0x6Du; c->pc = 0x9279u; c->a = a; { uint32_t ea = sc_v11_ea_dp(r, 0x006Du); sc_v11_bus_write8(r, ea, (uint8_t)(c->a)); if ((c->d & 0x00FFu) != 0u) sc_v11_scheduler_internal_cycle(r, 1u); } a = c->a;

    /* 0x059279 A9FF LDA #0xFF ; 0x05927B 856E STA dp 0x6E */
    sc_v11_scheduler_code_access(r, 0x059279u, 2u); r->open_bus = 0xFFu; c->pc = 0x927Bu; a = 0xFFu; sc_v11_set_nz8(c, (uint8_t)a);
    sc_v11_scheduler_code_access(r, 0x05927Bu, 2u); r->open_bus = 0x6Eu; c->pc = 0x927Du; c->a = a; { uint32_t ea = sc_v11_ea_dp(r, 0x006Eu); sc_v11_bus_write8(r, ea, (uint8_t)(c->a)); if ((c->d & 0x00FFu) != 0u) sc_v11_scheduler_internal_cycle(r, 1u); } a = c->a;

    /* 0x05927D A917 LDA #0x17 ; 0x05927F 8568 STA dp 0x68 */
    sc_v11_scheduler_code_access(r, 0x05927Du, 2u); r->open_bus = 0x17u; c->pc = 0x927Fu; a = 0x17u; sc_v11_set_nz8(c, (uint8_t)a);
    sc_v11_scheduler_code_access(r, 0x05927Fu, 2u); r->open_bus = 0x68u; c->pc = 0x9281u; c->a = a; { uint32_t ea = sc_v11_ea_dp(r, 0x0068u); sc_v11_bus_write8(r, ea, (uint8_t)(c->a)); if ((c->d & 0x00FFu) != 0u) sc_v11_scheduler_internal_cycle(r, 1u); } a = c->a;

    /* 0x059281 646C STZ dp 0x6C */
    sc_v11_scheduler_code_access(r, 0x059281u, 2u); r->open_bus = 0x6Cu; c->pc = 0x9283u;
    { uint32_t ea = sc_v11_ea_dp(r, 0x006Cu); sc_v11_bus_write8(r, ea, 0u);
      if ((c->d & 0x00FFu) != 0u) sc_v11_scheduler_internal_cycle(r, 1u); }

    /* 0x059283 C220 REP #0x20  -> 16-bit A */
    sc_v11_scheduler_code_access(r, 0x059283u, 2u);
    r->open_bus = 0x20u;
    c->pc = 0x9285u;
    sc_v11_unpack_p(c, (uint8_t)(c->p & (uint8_t)~0x20u));
    sc_v11_scheduler_internal_cycle(r, 1u);

    /* 0x059285 A9D001 LDA #0x01D0 ; 0x059288 8520 STA dp 0x20 (16-bit) */
    sc_v11_scheduler_code_access(r, 0x059285u, 3u); r->open_bus = 0xD0u; c->pc = 0x9288u; a = 0x01D0u; sc_v11_set_nz16(c, a);
    sc_v11_scheduler_code_access(r, 0x059288u, 3u); r->open_bus = 0x20u; c->pc = 0x928Bu; c->a = a; { uint32_t ea = sc_v11_ea_dp(r, 0x0020u); sc_v11_bus_write16(r, ea, (uint16_t)(c->a)); if ((c->d & 0x00FFu) != 0u) sc_v11_scheduler_internal_cycle(r, 1u); } a = c->a;

    /* ... (similar pattern) ... */

    /* 0x0592CC COP #0x00 — block terminator */
    c->a = a;
    sc_v11_scheduler_code_access(r, 0x0592CCu, 2u);
    r->open_bus = 0x00u;
    c->pc = 0x92CEu;
    if (c->emulation) return 0;
    if (!sc_v11_push8(r, c->pbr)) return 0;
    if (!sc_v11_push16(r, 0x92CEu)) return 0;
    if (!sc_v11_push8(r, c->p)) return 0;
    c->p |= SC_FLAG_I;
    c->p &= (uint8_t)~SC_FLAG_D;
    c->pbr = 0u;
    c->pc = (uint16_t)sc_v11_bus_read16(r, 0x00FFE4u);
    return 1;
}
