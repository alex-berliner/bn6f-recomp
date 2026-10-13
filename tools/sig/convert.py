#!/usr/bin/env python3
"""Send a subsystem prompt (tools/sig/subsys_prompt.py) or a repair to an outside model; keep its file.

    python3 tools/sig/convert.py MODEL PROMPT.txt OUT.c [--tag T]

The reply's last ```c block is written to OUT.c (the reply itself next to it, OUT.c.reply).
Tokens go to build/sig/usage.jsonl (tools/llm.py), tagged.
"""
from __future__ import annotations

import argparse
import os
import re
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import llm  # noqa: E402

SYSTEM = ("You port Game Boy Advance game code (Mega Man Battle Network 6) to clear, idiomatic C for a "
          "source port. Your code is compiled and tested automatically against the original on real game "
          "states, so it must behave exactly the same. Write it the way a careful human porter would.")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("model")
    ap.add_argument("prompt")
    ap.add_argument("out")
    ap.add_argument("--tag", default="subsystem")
    ap.add_argument("--max-tokens", type=int, default=64000)
    a = ap.parse_args()
    prompt = open(a.prompt).read()
    t0 = time.time()
    reply = llm.complete(a.model, prompt, system=SYSTEM, max_tokens=a.max_tokens, timeout=3600, retries=2,
                         tag=a.tag, usage_log=os.path.join(ROOT, "build/sig/usage.jsonl"), stream=True,
                         fields={"function": a.tag})
    open(a.out + ".reply", "w").write(reply)
    blocks = re.findall(r"```c\s*\n(.*?)```", reply, re.S)
    if not blocks:
        sys.exit(f"no ```c block in the reply ({len(reply)} chars, {round(time.time() - t0)}s)")
    open(a.out, "w").write(blocks[-1])
    print(f"{a.out}: {blocks[-1].count(chr(10))} lines in {round(time.time() - t0)}s")


if __name__ == "__main__":
    main()
