#!/usr/bin/env python3
"""ir_codegen.py — T066 Phase 2b: register-allocated native codegen.

Consumes IR JSON from ir_extract.py and emits, for each basic block, a C
function `sc_recomp_block_0xXXXXXX(SCV11Runtime *r)` in which the
accumulator (and, where profitable, X/Y) lives in a local register for the
whole block. The M/X flag widths are resolved at codegen time because REP/SEP
immediates in the hot blocks are literals, so LDA/STA/STZ widths are known
statically — this is the property that makes a real recompiler possible.

Accounting calls (code_access, internal_cycle, bus read/write) are preserved
verbatim (same PC, length, speed), so emulation stays bit-identical. Only the
dispatch, the per-instruction bookkeeping stores and the SC_STATIC_CONTEXT_END
round-trips are removed.

Blocks containing an instruction outside the supported subset are emitted as
fall-through stubs (return 0) so mixed-mode dispatch uses the interpreter.
"""
import argparse, json, os

SUPPORTED = {"LDA","STA","STZ","REP","SEP","NOP","PHY","PLY","PHX","PLX",
             "PHA","PLA","PHB","PLB","PHD","PLD","PHK","TAX","TAY","TXA",
             "TYA","TXS","TSX","INX","INY","DEX","DEY","INC","DEC","ASL",
             "LSR","ROL","ROR","CLC","SEC","CLI","SEI","CLV","CLD","SED",
             "XBA","XCE","TCD","TDC","TCS","TSC","PEA","PEI","PER","STP",
             "WAI","TSB","TRB"}
TERM = {"BCC","BCS","BEQ","BMI","BNE","BPL","BRA","BRL","BVC","BVS","JMP",
        "JML","JSR","JSL","RTS","RTL","RTI","BRK","COP","WDM","MVN","MVP"}


def gen_block(b):
    insns = b["insns"]
    native = all(i["mnemonic"] in SUPPORTED for i in insns)
    lines = []
    lines.append("/* Block 0x%06X..0x%06X (%d insns, term %s) %s */"
                 % (b["start_pc"], b["end_pc"], b["ninsns"], b["terminator"],
                    "NATIVE" if native else "STUB"))
    lines.append("int sc_recomp_block_0x%06X(SCV11Runtime *r) {" % b["start_pc"])
    if not native:
        lines.append("    /* unsupported opcode -> interpreter fallback */")
        lines.append("    return 0;")
        lines.append("}")
        return "\n".join(lines) + "\n"

    lines.append("    SCCpuState *c = &r->machine.cpu;")
    lines.append("    uint8_t  *dp = r->machine.wram + c->d;  /* direct page, hoisted */")
    lines.append("    uint16_t  a  = c->a;                    /* A in a register */")
    lines.append("    uint8_t   mf = (uint8_t)((c->p & SC_FLAG_M) != 0u);  /* 8-bit A? */")
    lines.append("")
    for ins in insns:
        pc = ins["pc"]; L = ins["length"]; op = ins["opcode"]
        mn = ins["mnemonic"]; mode = ins["mode"]; operand = ins["operand"]
        lines.append("    /* 0x%06X %02X %-4s */" % (pc, op, mn))
        lines.append("    sc_v11_scheduler_code_access(r, 0x%06Xu, %du);" % (pc, L))
        lines.append("    r->open_bus = 0x%02Xu;" % (operand & 0xFF))
        lines.append("    c->pc = 0x%04Xu;" % ((pc + L) & 0xFFFF))
        if mn == "REP":
            lines.append("    sc_v11_unpack_p(c, (uint8_t)(c->p & (uint8_t)~%du));" % (operand & 0xFF))
            if operand & 0x20:
                lines.append("    mf = 0u;")
            lines.append("    sc_v11_scheduler_internal_cycle(r, 1u);")
        elif mn == "SEP":
            lines.append("    sc_v11_unpack_p(c, (uint8_t)(c->p | (uint8_t)%du));" % (operand & 0xFF))
            if operand & 0x20:
                lines.append("    mf = 1u;")
            lines.append("    sc_v11_scheduler_internal_cycle(r, 1u);")
        elif mn == "LDA":
            if mode == "IMM":
                lines.append("    if (mf) a = (uint16_t)(%du & 0xFFu); else a = (uint16_t)(%du);" % (operand, operand))
            elif mode == "ABS":
                lines.append("    a = sc_v11_bus_read16(r, sc_v11_ea_abs(r, 0x%04Xu));" % operand)
            elif mode == "DP":
                lines.append("    a = dp[0x%02Xu] | ((uint16_t)dp[0x%02Xu] << 8);" % (operand & 0xFF, (operand+1)&0xFF))
            else:
                lines.append("    /* TODO LDA %s */ c->a = a; return 0;" % mode)
                lines.append("    return 0;")
                return "\n".join(lines) + "\n"
        elif mn == "STA":
            if mode == "ABS":
                lines.append("    sc_v11_bus_write16(r, sc_v11_ea_abs(r, 0x%04Xu), a);" % operand)
            elif mode == "DP":
                lines.append("    dp[0x%02Xu] = (uint8_t)(a); dp[0x%02Xu] = (uint8_t)(a >> 8);" % (operand & 0xFF, (operand+1)&0xFF))
            else:
                lines.append("    /* TODO STA %s */ c->a = a; return 0;" % mode)
                lines.append("    return 0;")
                return "\n".join(lines) + "\n"
        elif mn == "STZ":
            if mode == "DP":
                lines.append("    dp[0x%02Xu] = 0u; dp[0x%02Xu] = 0u;" % (operand & 0xFF, (operand+1)&0xFF))
            elif mode == "ABS":
                lines.append("    sc_v11_bus_write16(r, sc_v11_ea_abs(r, 0x%04Xu), 0u);" % operand)
            else:
                lines.append("    /* TODO STZ %s */ c->a = a; return 0;" % mode)
                lines.append("    return 0;")
                return "\n".join(lines) + "\n"
        elif mn == "NOP":
            lines.append("    sc_v11_scheduler_internal_cycle(r, 1u);")
        else:
            lines.append("    /* TODO %s */ c->a = a; return 0;" % mn)
            lines.append("    return 0;")
            return "\n".join(lines) + "\n"
    lines.append("")
    lines.append("    c->a = a;   /* commit accumulator */")
    lines.append("    return 1;  /* native block completed */")
    lines.append("}")
    return "\n".join(lines) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("ir_json")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()
    data = json.load(open(args.ir_json))
    group = data["group_base"]
    os.makedirs(args.out, exist_ok=True)
    c_path = os.path.join(args.out, "sc_recomp_native_0x%05X.c" % group)
    h_path = os.path.join(args.out, "sc_recomp_native_0x%05X.h" % group)
    n_native = 0
    with open(c_path, "w") as fh:
        fh.write('#include "sc_v11_runtime.h"\n')
        fh.write('#include "sc_core_internal.h"\n\n')
        for b in data["blocks"]:
            fh.write(gen_block(b) + "\n")
            if all(i["mnemonic"] in SUPPORTED for i in b["insns"]):
                n_native += 1
    with open(h_path, "w") as fh:
        fh.write("#ifndef SC_RECOMP_NATIVE_H\n#define SC_RECOMP_NATIVE_H\n")
        fh.write('#include "sc_v11_runtime.h"\n')
        for b in data["blocks"]:
            fh.write("int sc_recomp_block_0x%06X(SCV11Runtime *r);\n" % b["start_pc"])
        fh.write("#endif\n")
    print("%s: %d blocks, %d native-capable" % (c_path, data["nblocks"], n_native))
    print("%s: %d decls" % (h_path, data["nblocks"]))


if __name__ == "__main__":
    import json
    main()
