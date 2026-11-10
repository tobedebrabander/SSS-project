#!/usr/bin/env python3
import os
import subprocess
import sys

from guardlib import Source, Rewrite


# --- CHANGE CODE HERE ---
def instrument(code: bytes) -> bytes:
    src = Source(code)
    rw = Rewrite(code) # The rewriter modifies raw bytes of the original source code

    # Include our library
    if not src.has_include("guard.h"):
        rw.insert(0, '#include "guard.h"\n')

    # --- Rewriter 2: wrap indirect calls ---
    # TODO
    print(src.calls(), file=sys.stderr)

    # --- Rewriter 3: shadow stack ---
    # TODO
    print(src.functions(), file=sys.stderr)

    # --- Rewriter 1: registration + seal ---
    for func in src.functions():
        # TODO: To help you, we already give some sample code you can get inspiration off.
        #       For more info, refer to the heading of the guardlib.py code to see the API docs for the functions you can use.
        if func.is_main:
            rw.insert(func.body_open, " /* in main */")
        else:
            rw.insert(func.body_open, " /* not in main */")

    taken = src.address_taken_functions()
    if taken:
        print(taken, file=sys.stderr)

    return rw.result()
# --- END CHANGE CODE HERE ---


# --- DO NOT MODIFY ---
def main(argv):
    out = None
    emit_only = False
    cc = os.environ.get("GUARDCC_CC", "gcc")
    inputs, passthrough = [], []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "-o":
            out = argv[i + 1]
            i += 2
            continue
        if a == "--emit-only":
            emit_only = True
            i += 1
            continue
        if a.endswith(".c"):
            inputs.append(a)
            i += 1
            continue
        passthrough.append(a)
        i += 1
    if not inputs:
        sys.stderr.write("guardcc: no .c input files\n")
        return 1
    rewritten = []
    for path in inputs:
        with open(path, "rb") as f:
            code = f.read()
        with open(path[:-2] + ".guard.c", "wb") as f:
            f.write(instrument(code))
        rewritten.append(path[:-2] + ".guard.c")
    if emit_only:
        print("\n".join(rewritten))
        return 0
    here = os.path.dirname(os.path.abspath(__file__))
    guard_flags = ["-fno-omit-frame-pointer", "-fno-optimize-sibling-calls", "-falign-functions=16"]
    cmd = [cc, *guard_flags, f"-I{here}/../libguard",
           *rewritten, f"{here}/../libguard/guard.c", *passthrough]
    if out:
        cmd += ["-o", out]
    return subprocess.call(cmd)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
