#!/usr/bin/env python3
"""Minimal client for the user's other model providers (keys and endpoints
from pi's config: ~/.pi/agent/auth.json and models.json; MiniMax, OpenCode Go
and Z.ai go through the local pacing proxies).

  tools/llm.py PROVIDER/MODEL < prompt.txt > reply.txt
  import llm; llm.complete("opencode/kimi-k3", prompt, system=...)
"""
import json, os, re, sys, threading, time, urllib.error, urllib.request

PI = os.path.expanduser("~/.pi/agent")


def _providers():
    models = json.load(open(os.path.join(PI, "models.json")))["providers"]
    auth = json.load(open(os.path.join(PI, "auth.json")))
    return models, auth


_pace_lock = threading.Lock()
_next_start: dict = {}


def _pace(provider: str):
    """Space this process's requests to a provider evenly when $LLM_PACE asks for it
    (`hyper:15` = 15 requests a minute; comma-separated for several providers). Hyper has no local
    pacer; an even pace keeps its prompt cache warm, where bursts let it go cold between hours."""
    rates = dict(item.split(":", 1) for item in os.environ.get("LLM_PACE", "").split(",") if ":" in item)
    if provider not in rates:
        return
    interval = 60.0 / float(rates[provider])
    with _pace_lock:
        start = max(time.time(), _next_start.get(provider, 0.0))
        _next_start[provider] = start + interval
    time.sleep(max(0.0, start - time.time()))


def blocked_until(spec: str, log_dir: str = "/tmp/bn-pacer"):
    """When a provider behind a local pacer is rate-limited, when that lifts (epoch seconds), read
    from the pacer's log (`20:55:21 /v1/chat/completions 429 retry 1 in 12520s`, or `429 passed back,
    retry after 12520s` when it hands a long limit back). None when not limited or there is no pacer log."""
    path = os.path.join(log_dir, spec.split("/", 1)[0] + ".log")
    try:
        lines = open(path, errors="replace").read().splitlines()[-50:]
    except OSError:
        return None
    now = time.time()
    today = time.localtime(now)

    def when(m):
        at = time.mktime(today[:3] + (int(m.group(1)), int(m.group(2)), int(m.group(3))) + today[6:])
        return at - 86400 if at > now + 60 else at   # a log line from yesterday

    upstream_errors = []
    for line in reversed(lines):
        failed = re.match(r"(\d\d):(\d\d):(\d\d) \S+ upstream error", line)
        if failed:
            # the provider takes requests but none come back (read timeouts, 5xx): after three in a
            # row within 45 minutes, count it unavailable for half an hour after the last one
            upstream_errors.append(when(failed))
            if len(upstream_errors) == 3:
                return upstream_errors[0] + 1800 if now - upstream_errors[0] < 2700 and upstream_errors[0] + 1800 > now else None
            continue
        # a 429 the pacer waits out, or (a long one) passes back: `429 passed back, retry after 223208s`
        m = re.match(r"(\d\d):(\d\d):(\d\d) \S+ (\d+)(?: retry \d+ in (\d+)s| passed back, retry after (\d+)s)?", line)
        if not m:
            continue
        if m.group(4) != "429" or not (m.group(5) or m.group(6)):
            return None          # the latest answer got through
        lifts = when(m) + int(m.group(5) or m.group(6))
        return lifts if lifts > now else None
    return None


def _limit_lifts_in(message: str):
    """Seconds until a rate limit lifts, when the provider says ("try again in 1h2m3s"), else None."""
    m = re.search(r"try again in ((?:\d+h)?(?:\d+m)?(?:\d+(?:\.\d+)?s)?)", message)
    if not m or not m.group(1):
        return None
    parts = dict((unit, float(n)) for n, unit in re.findall(r"(\d+(?:\.\d+)?)([hms])", m.group(1)))
    return int(parts.get("h", 0) * 3600 + parts.get("m", 0) * 60 + parts.get("s", 0))


def complete(spec: str, prompt: str, system: str = "", max_tokens: int = 16000, timeout: int = 900,
             retries: int = 3, tag: str = "", usage_log: str = "", wait_out_limits: bool = True) -> str:
    """The reply text. With usage_log (or $LLM_USAGE_LOG), appends one JSON line
    per call: model, tag, seconds, input/output tokens as the provider reports
    them (reasoning tokens are part of the output where reported).
    wait_out_limits=False: a rate limit is raised at once (with when it lifts, as
    "retry after Ns") for a caller that can use another provider instead."""
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
    _pace(provider)
    t0 = time.time()   # the logged seconds are the call's, not the pacing wait's
    waits = 0
    attempt = 0
    while True:
        try:
            req = urllib.request.Request(url, data=data, headers=headers, method="POST")
            with urllib.request.urlopen(req, timeout=timeout) as r:
                reply = json.load(r)
            break
        except urllib.error.HTTPError as e:
            msg = e.read().decode(errors="replace")[:300]
            # a rate limit that says when it lifts (Hyper's hourly limit: "try again in 42m12s", or a
            # Retry-After header): wait for it, without counting a retry (at most 3 such waits per call)
            after = e.headers.get("Retry-After") if e.headers else None
            wait = (_limit_lifts_in(msg) or (int(float(after)) if after and after.replace(".", "").isdigit() else None)
                    if e.code == 429 else None)
            if wait is not None and not wait_out_limits:
                raise RuntimeError(f"{spec}: HTTP {e.code} (retry after {wait}s): {msg}") from None
            if wait is not None and waits < 3 and wait <= 3900:
                waits += 1
                time.sleep(wait + 10)
                t0 = time.time()   # the logged seconds are the call's, not the wait's
                continue
            if attempt == retries or e.code not in (429, 500, 502, 503, 504):
                raise RuntimeError(f"{spec}: HTTP {e.code}: {msg}") from None
        except (urllib.error.URLError, TimeoutError) as e:
            if attempt == retries:
                raise RuntimeError(f"{spec}: {e}") from None
        time.sleep(5 * (attempt + 1))
        attempt += 1
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
