"""Lightweight prompt routing checks for Maps vs Search."""

from semantic_pixel_router import classify_retrieval_with_reason


CASES = [
    ("give me directions to the airport", "maps"),
    ("navigate to nearest gas station", "maps"),
    ("how do i get to downtown", "maps"),
    ("distance to the nearest pharmacy", "maps"),
    ("publix near me", "maps"),
    ("closest gas station", "maps"),
    ("find parking near me", "maps"),
    ("route me to the closest hospital", "maps"),
    ("what is quantum computing", "search"),
    ("what's the weather today", "search"),
    ("will it rain tomorrow in tampa", "search"),
    ("search the web for latest ai news", "search"),
    ("who won the game last night", "search"),
]


def main() -> int:
    failures = 0
    for prompt, expected in CASES:
        actual, reason = classify_retrieval_with_reason(prompt)
        ok = actual == expected
        marker = "PASS" if ok else "FAIL"
        print(
            f"[{marker}] prompt={prompt!r} expected={expected} "
            f"actual={actual} reason={reason}"
        )
        if not ok:
            failures += 1
    if failures:
        print(f"\nRouting self-test failed: {failures} case(s) failed.")
        return 1
    print("\nRouting self-test passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
