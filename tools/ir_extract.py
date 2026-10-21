#!/usr/bin/env python3
"""ir_extract.py — T066 Phase 1: turn a generated v11 CPU shard into block IR.

Reads static-recomp/generated/v11-shards/sc_v11_cpu_group_XXXXX.c and emits
JSON: {group_base, nblocks, blocks:[{start_pc,end_pc,ninsns,terminator,insns:[{pc,length,opcode,mnemonic,mode,operand}]}, ...]}.

Block boundaries: an instruction that is a control-flow terminator (branch,
jump, subroutine call/return, COP/BRK, MVN/MVP) ends its basic block. The
next instruction starts a new block. This matches the generated code's own
SC_STATIC_CONTEXT_END() cadence and the shard's switch(address) dispatch.

Addressing mode + operand are recovered from the shard comments
("/* OPCODE IMMEDIATE MNEMONIC addrmode */") and the helper call arguments
(code_access length, ea_abs/ea_dp immediate). Length comes from code_access's
third argument.

Usage:
    ir_extract.py <shard.c> [--out file.json]
    ir_extract.py <shard.c> --summary   # block count, sizes, terminators
"""
import argparse, json, re, sys

TERM = {"BCC","BCS","BEQ","BMI","BNE","BPL","BRA","BRL","BVC","BVS",
        "JMP","JML","JSR","JSL","RTS","RTL","RTI","BRK","COP","WDM","MVN","MVP"}

MODE_MAP = {
    "imp": "IMP", "imm": "IMM", "abs": "ABS", "abx": "ABSX", "aby": "ABSY",
    "dp": "DP", "dpx": "DPX", "dpy": "DPY", "ind": "IND", "idx": "INDX",
    "idy": "INDY", "lng": "LNG", "lx": "LNGX", "sr": "SR", "sry": "SRY",
    "blk": "BLK",
}

def parse(path):
    t = open(path).read()
    cases = re.split(r"case 0x", t)[1:]
    insns = []
    for c in cases:
        m = re.match(r"([0-9A-Fa-f]+)u:", c)
        if not m:
            continue
        addr = int(m.group(1), 16)
        mm = re.search(r"/\*\s*(.*?)\*/", c)
        if mm:
            toks = mm.group(1).split()
            # mnemonic is the first all-alpha token
            mi = next((i for i,t in enumerate(toks) if t.isalpha()), None)
            if mi is not None:
                mnem = toks[mi].upper()
                hexblob = "".join(toks[:mi])
                am_raw = " ".join(toks[mi+1:]).lower()
            else:
                mnem = "?"; hexblob = "".join(toks); am_raw = ""
            if len(hexblob) >= 2:
                opcode = int(hexblob[0:2], 16)
                imm = hexblob[2:]
            else:
                opcode = 0; imm = ""
        else:
            opcode = 0; imm = ""; mnem = "?"; am_raw = ""
        # length from code_access(r, pc, N)
        lm = re.search(r"sc_v11_scheduler_code_access\(r,0x[0-9A-Fa-f]+u,(\d+)u\)", c)
        length = int(lm.group(1)) if lm else 0
        # operand: immediate/absolute address. Prefer the literal in the comment's
        # trailing token if it looks like a hex address; else the helper arg.
        operand = 0
        if imm:
            try:
                operand = int(imm, 16)
            except ValueError:
                operand = 0
        # ea_abs(r, 0xNNNNu) -> operand is that address
        ea = re.search(r"sc_v11_ea_abs\(r,0x([0-9A-Fa-f]+)u\)", c)
        if ea:
            operand = int(ea.group(1), 16)
        eadp = re.search(r"sc_v11_ea_dp\(r,0x([0-9A-Fa-f]+)u\)", c)
        if eadp and not ea:
            operand = int(eadp.group(1), 16)
        # raw per-instruction body, verbatim from the shard (accounting + op),
        # used by the stitcher. Bookkeeping stores are retained (diagnostic).
        bi = c.find("sc_v11_scheduler_code_access")
        bj = c.find("SC_STATIC_CONTEXT_END();", bi)
        body = c[bi:bj+len("SC_STATIC_CONTEXT_END();")] if (bi>=0 and bj>=0) else ""
        body = "\n".join(ln[6:] if ln.startswith("      ") else ln for ln in body.splitlines()).strip()

        # mode
        mode = "IMP"
        a = am_raw
        if a.startswith("imm"): mode = "IMM"
        elif a.startswith("abs"): mode = "ABS"
        elif a.startswith("dp"): mode = "DP"
        elif a.startswith("lng"): mode = "LNG"
        # flag effects
        mflag = -1; xflag = -1
        if mnem == "REP":
            if operand & 0x20: mflag = 0
            if operand & 0x10: xflag = 0
        elif mnem == "SEP":
            if operand & 0x20: mflag = 1
            if operand & 0x10: xflag = 1
        # callee census
        ca = 1 if "sc_v11_scheduler_code_access" in c else 0
        ci = 1 if "sc_v11_scheduler_internal_cycle" in c else 0
        br = 1 if "sc_v11_bus_read" in c else 0
        bw = 1 if "sc_v11_bus_write" in c else 0
        insns.append({
            "pc": addr, "length": length, "opcode": opcode,
            "mnemonic": mnem, "mode": mode, "operand": operand,
            "flags": {"m": mflag, "x": xflag},
            "calls": {"code_access": ca, "internal_cycle": ci,
                      "bus_read": br, "bus_write": bw},
            "body": body,
        })
    # basic blocks: split on terminators
    blocks = []
    cur = []
    for ins in insns:
        cur.append(ins)
        if ins["mnemonic"] in TERM:
            blocks.append(cur); cur = []
    if cur:
        blocks.append(cur)
    group_base = min(i["pc"] for i in insns) & ~0x3FF
    return {
        "group_base": group_base,
        "ninsns": len(insns),
        "nblocks": len(blocks),
        "blocks": [
            {"start_pc": b[0]["pc"], "end_pc": b[-1]["pc"],
             "ninsns": len(b),
             "terminator": b[-1]["mnemonic"],
             "insns": b} for b in blocks],
    }

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("shard")
    ap.add_argument("--out", help="write JSON here")
    ap.add_argument("--summary", action="store_true")
    args = ap.parse_args()
    data = parse(args.shard)
    if args.summary:
        print(f"{args.shard}: {data['ninsns']} insns, {data['nblocks']} blocks")
        print("  block sizes:", sorted(len(b["insns"]) for b in data["blocks"]))
        print("  terminators:", collections_counter([b["terminator"] for b in data["blocks"]]))
    else:
        out = json.dumps(data, indent=1)
        if args.out:
            open(args.out, "w").write(out)
            print("wrote", args.out)
        else:
            print(out)

import collections
def collections_counter(x):
    return dict(collections.Counter(x))

if __name__ == "__main__":
    main()
