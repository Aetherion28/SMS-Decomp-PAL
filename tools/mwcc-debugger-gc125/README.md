# mwcc-debugger for GC/1.2.5

This is a port of [cadmic/mwcc-debugger](https://github.com/cadmic/mwcc-debugger) to the GC/1.2.5 compiler used for most of SMS.
Upstream only supports GC/1.0–1.1 and later 2.x builds.
The port adds one version entry whose addresses were found by aligning the 1.2.5 binary against the 1.1 one.
It reuses the 1.1 struct layouts, which hold for 1.2.5.
It was checked against real 1.2.5 output: frame sizes and stack-slot offsets agree exactly.

`nodenames_size` and `opcodeinfo_size` are copied from 1.1 and are unverified; node/opcode names look right in practice.

## Setup

```sh
git clone https://github.com/cadmic/mwcc-debugger            # tested at bad9cea
cd mwcc-debugger && git apply /path/to/sms/tools/mwcc-debugger-gc125/gc125.patch
git clone -b gdb-stub https://github.com/encounter/retrowin32  # tested at 11dbea5
cd retrowin32 && cargo build -p retrowin32 -F x86-unicorn --profile lto
```

The debugger drives `gdb` (override with its gdb option) against retrowin32's stub on port 9001, so only one run at a time.

## Running

Take the compile command for a unit from `ninja -t commands build/GMSJ01/src/<unit>.o`.
Drop the `wibo` / `sjiswrap` / `transform_dep` wrappers so the command starts at `compilers/GC/1.2.5/mwcceppc.exe`, and point `-o` somewhere disposable.
Run from the repository root:

```sh
python3 mwcc_debugger.py -e retrowin32/target/lto/retrowin32 -a "<mwcceppc command>" '<mangled function name>' out/
```

`out/variables.txt` lists every local and temporary with its register or stack slot.
`out/frontend-*.txt` are the AST dumps (after inlining), `out/backend-*.txt` the PCode after each pass, and `out/regalloc-*` the interference graphs.

## What it showed for bosswanwan

Slots are handed out in list order: named locals first, then parse-time temporaries, then inliner temporaries grouped by inline depth.
Any local that ends up with no register still gets a slot, even when all its code was optimised away.
That is why frame-only mismatches are so common: every missing inline call, accessor, or dead block in the original shows up only as extra stack bytes.

`slot_gaps.py` lines our slots up against the target's live stack offsets.
It shows where in that list the target has extra bytes.
