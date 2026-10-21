#!/usr/bin/env python3
"""ir_stitch.py — T066 Phase 2a: stitch a shard's per-PC bodies into per-block C."""
import argparse, json, os

TERM = {"BCC","BCS","BEQ","BMI","BNE","BPL","BRA","BRL","BVC","BVS","JMP",
        "JML","JSR","JSL","RTS","RTL","RTI","BRK","COP","WDM","MVN","MVP"}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("ir_json")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()
    data = json.load(open(args.ir_json))
    group = data["group_base"]
    os.makedirs(args.out, exist_ok=True)
    c_path = os.path.join(args.out, "sc_recomp_stitched_0x%05X.c" % group)
    h_path = os.path.join(args.out, "sc_recomp_stitched_0x%05X.h" % group)

    allins = []
    for b in data["blocks"]:
        allins.extend(b["insns"])
    allins.sort(key=lambda i: i["pc"])

    blocks = []; cur = []
    for ins in allins:
        cur.append(ins)
        if ins["mnemonic"] in TERM:
            blocks.append(cur); cur = []
    if cur:
        blocks.append(cur)

    with open(c_path, "w") as fh:
        fh.write('#include "sc_v11_runtime.h"\n')
        fh.write('#include "sc_core_internal.h"\n\n')
        for blk in blocks:
            sp = blk[0]["pc"]; ep = blk[-1]["pc"]
            fh.write("/* Block 0x%06X..0x%06X (%d insns, term %s) */\n"
                     % (sp, ep, len(blk), blk[-1]["mnemonic"]))
            fh.write("int sc_recomp_block_0x%06X(SCV11Runtime *r) {\n" % sp)
            fh.write("    SCCpuState *c = &r->machine.cpu;\n")
            for ins in blk:
                fh.write("    /* --- 0x%06X %02X %-4s --- */\n"
                         % (ins["pc"], ins["opcode"], ins["mnemonic"]))
                fh.write(ins["body"] + "\n")
            fh.write("    return 1;  /* block completed natively */\n")
            fh.write("}\n\n")
    with open(h_path, "w") as fh:
        fh.write("#ifndef SC_RECOMP_STITCHED_H\n#define SC_RECOMP_STITCHED_H\n")
        fh.write('#include "sc_v11_runtime.h"\n')
        for blk in blocks:
            fh.write("int sc_recomp_block_0x%06X(SCV11Runtime *r);\n" % blk[0]["pc"])
        fh.write("#endif\n")
    print("%s: %d blocks (%d insns)" % (c_path, len(blocks), len(allins)))
    print("%s: %d decls" % (h_path, len(blocks)))


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("ir_json")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()
    main()
