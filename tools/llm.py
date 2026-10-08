#!/usr/bin/env python3
"""Minimal client for the user's other model providers (keys and endpoints
from pi's config: ~/.pi/agent/auth.json and models.json; MiniMax, OpenCode Go
and Z.ai go through the local pacing proxies).

  tools/llm.py PROVIDER/MODEL < prompt.txt > reply.txt
  import llm; llm.complete("opencode/kimi-k3", prompt, system=...)
"""
import json, os, sys, time, urllib.error, urllib.request

PI = os.path.expanduser("~/.pi/agent")


def _providers():
    models = json.load(open(os.path.join(PI, "models.json")))["providers"]
    auth = json.load(open(os.path.join(PI, "auth.json")))
    return models, auth


def complete(spec: str, prompt: str, system: str = "", max_tokens: int = 16000, timeout: int = 900,
             retries: int = 3, tag: str = "", usage_log: str = "") -> str:
    """The reply text. With usage_log (or $LLM_USAGE_LOG), appends one JSON line
    per call: model, tag, seconds, input/output tokens as the provider reports
    them (reasoning tokens are part of the output where reported)."""
    provider, model = spec.split("/", 1)
    t0 = time.time()
    models, auth = _providers()
    p = models[provider]
    key = auth[provider]["key"]
    if p.get("api") == "anthropic-messages":
        url = p["baseUrl"].rstrip("/") + "/v1/messages"
        body = {"model": model, "max_tokens": max_tokens, "messages": [{"role": "user", "content": prompt}]}
        if system:
            body["system"] = system
        headers = {"x-api-key": key, "anthropic-version": "2023-06-01", "content-type": "application/json"}
    else:
        url = p["baseUrl"].rstrip("/") + "/chat/completions"
        msgs = ([{"role": "system", "content": system}] if system else []) + [{"role": "user", "content": prompt}]
        body = {"model": model, "max_tokens": max_tokens, "messages": msgs}
        headers = {"Authorization": f"Bearer {key}", "content-type": "application/json"}
    data = json.dumps(body).encode()
    for attempt in range(retries + 1):
        try:
            req = urllib.request.Request(url, data=data, headers=headers, method="POST")
            with urllib.request.urlopen(req, timeout=timeout) as r:
                reply = json.load(r)
            break
        except urllib.error.HTTPError as e:
            msg = e.read().decode(errors="replace")[:300]
            if attempt == retries or e.code not in (429, 500, 502, 503, 504):
                raise RuntimeError(f"{spec}: HTTP {e.code}: {msg}") from None
        except (urllib.error.URLError, TimeoutError) as e:
            if attempt == retries:
                raise RuntimeError(f"{spec}: {e}") from None
        time.sleep(5 * (attempt + 1))
    log = usage_log or os.environ.get("LLM_USAGE_LOG", "")
    if log:
        u = reply.get("usage") or {}
        rec = {"model": spec, "tag": tag, "seconds": round(time.time() - t0, 1),
               "in": u.get("input_tokens", u.get("prompt_tokens")),
               "out": u.get("output_tokens", u.get("completion_tokens")),
               "reasoning": (u.get("completion_tokens_details") or {}).get("reasoning_tokens"),
               "cache_read": u.get("cache_read_input_tokens", (u.get("prompt_tokens_details") or {}).get("cached_tokens")),
               "time": time.strftime("%Y-%m-%d %H:%M:%S")}
        with open(log, "a") as f:
            f.write(json.dumps(rec) + "\n")
    if p.get("api") == "anthropic-messages":
        return "".join(b.get("text", "") for b in reply.get("content", []) if b.get("type") == "text")
    return reply["choices"][0]["message"].get("content") or ""


if __name__ == "__main__":
    print(complete(sys.argv[1], sys.stdin.read()))
