# main_parts

`main.cpp` is intentionally split into small include fragments so the firmware still builds as one Arduino translation unit.

Why this shape:
- the original file keeps most implementation inside an anonymous namespace;
- globals such as `gCanvas`, `gWs`, `gRealtimeWs`, `gSdSpi`, preferences, OTA buffers, and task handles are shared widely;
- keeping include fragments preserves lookup, linkage, initialization order, and behavior while making each slice small enough to edit.

Rules for edits:
- keep these fragments included only from `main.cpp`;
- do not add `namespace {}` wrappers inside fragments;
- keep declarations before uses, following the numeric order;
- after larger state boundaries are explicit, individual fragments can be promoted to real `.h/.cpp` files.
