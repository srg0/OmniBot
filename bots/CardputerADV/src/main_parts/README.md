# main_parts

`main.cpp` is intentionally split into small include fragments so the firmware still builds as one Arduino translation unit.

Why this shape:
- the implementation shares a large anonymous namespace and many `g*` globals;
- include fragments preserve lookup, linkage, initialization order, and behavior;
- each fragment is cut only at top-level declarations/functions, so individual functions are not split across files.

Rules for edits:
- keep these fragments included only from `main.cpp`;
- do not add `namespace {}` wrappers inside fragments;
- keep declarations before uses, following the numeric order;
- after stable state boundaries exist, promote focused fragments to real `.h/.cpp` modules.
