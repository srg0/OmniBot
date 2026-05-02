# Cardputer ADV main_parts

This directory is a single-translation-unit split of `src/main.cpp`.
The `.cpp.inc` fragments are included sequentially by `main.cpp`; they are not independent `.cpp` files.
This preserves anonymous-namespace linkage, global initialization order, and the current firmware behavior.
Fragments are split only at preprocessor/comment-safe boundaries; some long functions may continue across adjacent fragments.
