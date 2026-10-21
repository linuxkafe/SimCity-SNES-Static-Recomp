/* sc_recomp_ir.h -- T066 Phase 1: block-level intermediate representation.
 *
 * The current "static recompilation" is a per-address dispatch in portable C:
 * every emulated instruction is a `case` in a switch(PC), with CPU state
 * round-tripping through SCV11Runtime between instructions. A real recompiler
 * needs an IR it can allocate registers over and inline memory access into.
 *
 * This header defines that IR. It is produced by tools/ir_extract.py from the
 * generated shards (static-recomp/generated/v11-shards/*.c) and consumed by the
 * Phase 2 codegen. Verification: a block lowered from this IR must produce
 * identical master-clock deltas and identical --hash/--shots output to the
 * interpreted shard it replaces.
 *
 * Addressing modes are the 65816 modes the generator actually emits; the
 * enum is closed over the observed set (see tools/ir_extract.py census), not
 * over the full 65816 ISA.
 */
#ifndef SC_RECOMP_IR_H
#define SC_RECOMP_IR_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    IR_AM_IMP,   /* implied / accumulator */
    IR_AM_IMM,   /* immediate */
    IR_AM_ABS,   /* absolute (bank from DBR) */
    IR_AM_ABSX,  /* absolute indexed X */
    IR_AM_ABSY,  /* absolute indexed Y */
    IR_AM_DP,    /* direct page */
    IR_AM_DPX,   /* direct page indexed X */
    IR_AM_DPY,   /* direct page indexed Y */
    IR_AM_IND,   /* (dp) indirect */
    IR_AM_INDX,  /* (dp,X) */
    IR_AM_INDY,  /* (dp),Y */
    IR_AM_LNG,   /* long absolute */
    IR_AM_LNGX,  /* long absolute indexed X */
    IR_AM_SR,    /* stack relative */
    IR_AM_SRY,   /* stack relative indexed Y */
    IR_AM_BLK    /* block move src,dst */
} IrAddrMode;

/* Effect on the M (8-bit A) and X (8-bit index) flags. Only REP/SEP #0x20
 * and #0x10 matter; everything else is "none". Codegen uses these to know
 * the operand width at each point without tracking P at runtime when the
 * immediate is a literal (the common case in the hot blocks). */
typedef struct {
    int8_t m;   /* -1 none, 0 clear (16-bit), 1 set (8-bit) */
    int8_t x;
} IrFlagEffect;

typedef struct {
    uint32_t    pc;          /* emulated PC at instruction entry */
    uint8_t     length;      /* byte length (1..4) */
    uint8_t     opcode;      /* raw 65816 opcode */
    IrAddrMode  mode;
    uint32_t    operand;     /* immediate / absolute address / dp offset */
    IrFlagEffect flags;
    /* callee census: which runtime helpers this instruction lowers to.
       Used by codegen to decide inlining. */
    uint8_t     calls_code_access;   /* scheduler_code_access */
    uint8_t     calls_internal_cycle;/* scheduler_internal_cycle */
    uint8_t     calls_bus_read;
    uint8_t     calls_bus_write;
} IrInsn;

typedef struct {
    uint32_t start_pc;
    uint32_t end_pc;       /* PC of the terminator */
    uint32_t ninsns;
    IrInsn  *insns;
    /* terminator kind, for control-flow reconstruction */
    const char *terminator; /* "RTS","RTL","RTI","BRA","BRL","JMP","JML",
                               "JSR","JSL","Bcc","COP","BRK","MVN","MVP","..." */
} IrBlock;

typedef struct {
    uint32_t group_base;   /* 1 KiB-aligned PC base, e.g. 0x09000 */
    uint32_t nblocks;
    IrBlock *blocks;
    uint64_t total_cycles; /* from the profiler; 0 if unprofiled */
} IrGroup;

#endif /* SC_RECOMP_IR_H */
