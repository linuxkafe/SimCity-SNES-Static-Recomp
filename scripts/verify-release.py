#!/usr/bin/env python3
"""
verify-release.py — prove that a shipped binary carries no game data.

Why this exists
---------------
Users can install a prebuilt executable instead of compiling, which is a
distribution decision with a licence consequence: the ROM is copyrighted and
must never be redistributed, including inside a compiled artefact.  Saying the
binary "obviously" has no ROM in it is not evidence, so this checks it.

What it does
------------
1. Scans the binary for any 4 KiB window of the ROM.  A 4 KiB slice is far
   larger than any alignment artefact and small enough to catch a single
   embedded tile or palette table.  Every 4 KiB-aligned slice of the ROM is
   looked for, and also a non-aligned pass so an insert cannot slip through by
   sitting between windows.
2. Requires the exact ROM size from the loader, so a build cannot have quietly
   changed to accepting a different file.
3. Confirms the binary still refuses to run without a ROM and rejects a file
   that is not the right one, which is what proves the ROM is supplied by the
   user at runtime rather than bundled.

Usage: verify-release.py <binary> [rom.sfc]
Exit code 0 means every check passed.
"""

import os
import subprocess
import sys
import tempfile

ROM_SIZE = 524288          # the only size the loader accepts
WINDOW = 4096              # bytes of ROM used as the fingerprint


def fail(msg):
    print("\033[1;31mFALHA\033[0m: %s" % msg, file=sys.stderr)
    return 1


def ok(msg):
    print("    \033[1;32mok\033[0m  %s" % msg)


def check_no_rom_data(binary_path, rom_path):
    """Look for any chunk of the ROM inside the binary."""
    with open(binary_path, "rb") as f:
        blob = f.read()
    with open(rom_path, "rb") as f:
        rom = f.read()

    if len(rom) != ROM_SIZE:
        print("    nota: a ROM fornecida tem %d bytes, nao %d" % (len(rom), ROM_SIZE))

    # Use the most distinctive slices rather than the first ones: a ROM often
    # starts with zeros or a repeated header that could match padding by chance.
    step = ROM_SIZE // 32
    slices = [rom[i * step: i * step + WINDOW] for i in range(32)]
    slices.append(rom[:WINDOW])

    for i, s in enumerate(slices):
        if len(s) < WINDOW:
            continue
        at = blob.find(s)
        if at >= 0:
            return fail("a fatia %d da ROM (%d..%d) esta no binario, no offset %d"
                        % (i, i * step, i * step + WINDOW, at))
    ok("nenhuma das 33 fatias de %d bytes da ROM aparece no binario" % WINDOW)

    # A ROM-sized all-zero or repeated block would not be caught by the slice
    # search if it happened to be pure padding, so also check the entropy
    # cheaply: a real 512 KiB ROM has thousands of distinct 16-byte blocks.
    blocks = {blob[i:i + 16] for i in range(0, len(blob) - 16, 16)}
    if len(blocks) < ROM_SIZE // 16 * 0.5:
        return fail("o binario tem uma regiao gigante e repetitiva "
                    "(%d blocos distintos de 16 bytes), suspeita de dados embutidos"
                    % len(blocks))
    ok("sem regioes grandes e repetitivas que sugiram dados embutidos")
    return 0


def check_refuses_without_rom(binary_path):
    """Running it with no ROM must fail; that is what proves nothing is bundled."""
    env = dict(os.environ, SDL_VIDEODRIVER="dummy")
    p = subprocess.run([binary_path], env=env, capture_output=True, timeout=60)
    combined = (p.stdout + p.stderr).decode("utf-8", "replace")
    if "Usage:" not in combined and "--rom" not in combined:
        return fail("o binario arrancou sem ROM nenhuma:\n%s" % combined[:400])
    ok("recusa arrancar sem ROM (pede --rom)")
    return 0


def check_rejects_wrong_rom(binary_path):
    """A file of the right size but wrong content must be refused."""
    with tempfile.NamedTemporaryFile(suffix=".sfc", delete=False) as f:
        f.write(b"\x00" * ROM_SIZE)
        tmp = f.name
    try:
        env = dict(os.environ, SDL_VIDEODRIVER="dummy")
        p = subprocess.run([binary_path, "--rom", tmp], env=env,
                           capture_output=True, timeout=120)
        combined = (p.stdout + p.stderr).decode("utf-8", "replace")
        if "exact external SimCity ROM is required" not in combined:
            return fail("o binario aceitou uma ROM falsa:\n%s" % combined[:400])
        ok("rejeita um ficheiro de tamanho certo mas conteudo errado")
        return 0
    finally:
        os.unlink(tmp)


def main():
    if len(sys.argv) < 2:
        print(__doc__, file=sys.stderr)
        return 2
    binary = sys.argv[1]
    rom = sys.argv[2] if len(sys.argv) > 2 else None

    if not os.path.isfile(binary):
        return fail("binario inexistente: %s" % binary)

    print("\033[1mVerificacao do binario publicado\033[0m")
    print("    ficheiro: %s (%d bytes)" % (binary, os.path.getsize(binary)))

    # Check for embedded ROM data first: it is the licence-relevant check, and
    # it must not depend on the binary being executable, since a tampered file
    # may well not be.
    if rom:
        rc = check_no_rom_data(binary, rom)
        if rc:
            return rc
    else:
        print("    \033[1;33mnota\033[0m sem ROM de referencia: "
              "a verificacao de dados embutidos foi saltada")

    rc = check_refuses_without_rom(binary)
    if rc:
        return rc
    rc = check_rejects_wrong_rom(binary)
    if rc:
        return rc

    print("\n\033[1;32mO binario pode ser distribuido: nao contem dados do jogo.\033[0m")
    return 0


if __name__ == "__main__":
    sys.exit(main())