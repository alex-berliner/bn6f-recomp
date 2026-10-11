#!/usr/bin/env python3
"""Minimal client for the user's other model providers (keys and endpoints
from pi's config: ~/.pi/agent/auth.json and models.json; MiniMax, OpenCode Go
and Z.ai go through the local pacing proxies).

  tools/llm.py PROVIDER/MODEL < prompt.txt > reply.txt
  import llm; llm.complete("opencode/kimi-k3", prompt, system=...)
"""
import http.client, json, os, re, sys, threading, time, urllib.error, urllib.request

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


def model_id(spec: str) -> str:
    """One name per model whichever provider serves it: `hyper/minimax-m3` and `minimax/MiniMax-M3` are both
    `minimax-m3`, `hyper/glm-5.3-flash` and `zai/glm-5.3-flash` both `glm-5.3-flash`. Pass rates compare
    models by this name; quota stays per provider (the part before the slash)."""
    return spec.split("/", 1)[-1].lower()


class ReasoningBudgetSpent(Exception):
    """complete() closed a streamed reply whose hidden reasoning passed its budget before any code began: a
    runaway, stopped early (review 3 Area 1). Its usage row is written first, with estimated tokens."""

    def __init__(self, spec: str, reasoning_tokens: int, budget: int, seconds: float):
        super().__init__(f"{spec}: stopped after about {reasoning_tokens} reasoning tokens with no code "
                         f"(budget {budget})")
        self.reasoning_tokens = reasoning_tokens
        self.budget = budget
        self.seconds = seconds


# For estimates only (a stopped reply has no usage). Measured 2026-10-11 on a streamed zai/glm-5.3-flash farm
# call: one reasoning event per token (28,686 = the usage's reasoning_tokens), 3.53 characters per reasoning
# token, 2.63 characters per prompt token (code and assembly).
CHARS_PER_TOKEN = 3.5
PROMPT_CHARS_PER_TOKEN = 2.6


class _Stream:
    """A streamed reply as it arrives: the answer's text, the reasoning so far, the usage the provider reports
    (at the end), and whether the answer has begun a code block. Providers send reasoning as
    `reasoning_content` (glm, deepseek, qwen) or `reasoning` deltas (chat completions), as `thinking_delta`s
    (Anthropic's messages), or inline in the text between <think> and </think>."""

    def __init__(self, spec: str, budget: int | None):
        self.spec, self.budget = spec, budget
        self.text: list[str] = []
        self.reasoning_chars = self.reasoning_events = 0
        self.usage: dict = {}
        self.answered_by = None
        self.first_byte_at = None
        self._inline_think = False   # inside <think>...</think> in the text
        self._code = False           # a ``` fence has appeared in the answer
        self._tail = ""              # the answer's last two characters (a fence split across events)

    def reasoning_tokens(self) -> int:
        """One event is one token for glm (Z.ai, 2026-10-11); characters cover providers that send several."""
        return max(self.reasoning_events, round(self.reasoning_chars / CHARS_PER_TOKEN))

    def answer_tokens(self) -> int:
        return round(sum(map(len, self.text)) / CHARS_PER_TOKEN)

    def add_reasoning(self, piece: str) -> None:
        if piece:
            self.reasoning_chars += len(piece)
            self.reasoning_events += 1

    def add_text(self, piece: str) -> None:
        """Answer text; <think> blocks inside it count as reasoning."""
        while piece:
            if self._inline_think:
                inside, closed, piece = piece.partition("</think>")
                self.add_reasoning(inside)
                self._inline_think = not closed
            else:
                before, opened, piece = piece.partition("<think>")
                self.text.append(before)
                self._code = self._code or "```" in self._tail + before
                self._tail = (self._tail + before)[-2:]
                self._inline_think = bool(opened)

    def code_started(self) -> bool:
        return self._code

    def over_budget(self) -> bool:
        return self.budget is not None and self.reasoning_tokens() > self.budget and not self.code_started()


def _events(response, stream: _Stream):
    """The data of each server-sent event line; the stream notes when the first line of any kind arrived."""
    for raw in response:
        if stream.first_byte_at is None:
            stream.first_byte_at = time.time()
        line = raw.decode(errors="replace").strip()
        if line.startswith("data:"):
            yield line[5:].strip()


def _read_events(response, stream: "_Stream") -> dict:
    """A streamed chat completion, put together as the non-streamed reply would be, plus when its first
    line arrived (`first_byte_at`, epoch seconds): the wait before it is the pacer's and the provider's
    queue, the rest is generation. With the stream's budget, _Stopped is raised (and the caller closes the
    connection) once the reasoning passes it with no code begun."""
    for payload in _events(response, stream):
        if payload == "[DONE]":
            break
        event = json.loads(payload)
        if event.get("error"):
            raise RuntimeError(f"{stream.spec}: error in the stream: {str(event['error'])[:300]}")
        stream.usage = event.get("usage") or stream.usage
        stream.answered_by = stream.answered_by or event.get("model")
        for choice in event.get("choices") or []:
            delta = choice.get("delta") or {}
            stream.add_reasoning(delta.get("reasoning_content") or delta.get("reasoning") or "")
            stream.add_text(delta.get("content") or "")
        if stream.over_budget():
            raise _Stopped(stream)
    return {"choices": [{"message": {"content": "".join(stream.text)}}], "usage": stream.usage,
            "model": stream.answered_by, "first_byte_at": stream.first_byte_at}


def _read_anthropic_events(response, stream: "_Stream") -> dict:
    """A streamed Anthropic-style message (MiniMax), put together as the non-streamed reply would be."""
    for payload in _events(response, stream):
        event = json.loads(payload)
        kind = event.get("type")
        if kind == "error":
            raise RuntimeError(f"{stream.spec}: error in the stream: {str(event.get('error'))[:300]}")
        if kind == "message_start":
            message = event.get("message") or {}
            stream.answered_by = message.get("model")
            stream.usage = {**stream.usage, **{k: v for k, v in (message.get("usage") or {}).items() if v}}
        elif kind == "content_block_start":
            block = event.get("content_block") or {}
            stream.add_reasoning(block.get("thinking") or "")
            stream.add_text(block.get("text") or "")
        elif kind == "content_block_delta":
            delta = event.get("delta") or {}
            stream.add_reasoning(delta.get("thinking") or "")
            stream.add_text(delta.get("text") or "")
        elif kind == "message_delta":
            stream.usage = {**stream.usage, **{k: v for k, v in (event.get("usage") or {}).items() if v is not None}}
        elif kind == "message_stop":
            break
        if stream.over_budget():
            raise _Stopped(stream)
    return {"content": [{"type": "text", "text": "".join(stream.text)}], "usage": stream.usage,
            "model": stream.answered_by, "first_byte_at": stream.first_byte_at}


class _Stopped(Exception):
    """Inside complete(): the stream passed its budget (the connection is closed on the way out)."""

    def __init__(self, stream: _Stream):
        super().__init__(stream.spec)
        self.stream = stream


def _limit_lifts_in(message: str):
    """Seconds until a rate limit lifts, when the provider says ("try again in 1h2m3s"), else None."""
    m = re.search(r"try again in ((?:\d+h)?(?:\d+m)?(?:\d+(?:\.\d+)?s)?)", message)
    if not m or not m.group(1):
        return None
    parts = dict((unit, float(n)) for n, unit in re.findall(r"(\d+(?:\.\d+)?)([hms])", m.group(1)))
    return int(parts.get("h", 0) * 3600 + parts.get("m", 0) * 60 + parts.get("s", 0))


def complete(spec: str, prompt: str, system: str = "", max_tokens: int = 16000, timeout: int = 900,
             retries: int = 3, tag: str = "", usage_log: str = "", wait_out_limits: bool = True,
             stream: bool = False, reasoning_budget: int | None = None, fields: dict | None = None) -> str:
    """The reply text. With usage_log (or $LLM_USAGE_LOG), appends one JSON line
    per call: model (the spec asked for), model_id (the same model under every
    provider, model_id()), answered_by (the model the provider says answered),
    tag, seconds, first_byte (streamed calls: seconds until the reply's first
    line), input/output tokens as the provider reports them (reasoning tokens
    are part of the output where reported). Failed calls get a line too, with
    `error`; so do stopped ones (below), with `stopped`. Where the provider
    reported nothing (a stopped call, a stream cut off part way), the tokens are
    estimated from the text that arrived and `estimated` is true.
    wait_out_limits=False: a rate limit is raised at once (with when it lifts, as
    "retry after Ns") for a caller that can use another provider instead.
    stream=True: the reply comes as server-sent events, so a long reasoning call
    keeps data flowing; Z.ai drops a call that has sent nothing for about 15 minutes.
    reasoning_budget=N (streams): once the reply's reasoning passes N tokens with no
    code block begun, the connection is closed and ReasoningBudgetSpent raised (a
    runaway, stopped early; review 3 Area 1).
    fields: what the caller adds to its usage line (the farm: function, round, prompt
    and prompt_version, max_tokens)."""
    provider, model = spec.split("/", 1)
    models, auth = _providers()
    p = models[provider]
    key = auth[provider]["key"]
    anthropic = p.get("api") == "anthropic-messages"
    stream = stream or reasoning_budget is not None
    if anthropic:
        url = p["baseUrl"].rstrip("/") + "/v1/messages"
        body = {"model": model, "max_tokens": max_tokens, "messages": [{"role": "user", "content": prompt}]}
        if system:
            body["system"] = system
        if stream:
            body["stream"] = True
        headers = {"x-api-key": key, "anthropic-version": "2023-06-01", "content-type": "application/json"}
    else:
        url = p["baseUrl"].rstrip("/") + "/chat/completions"
        msgs = ([{"role": "system", "content": system}] if system else []) + [{"role": "user", "content": prompt}]
        body = {"model": model, "max_tokens": max_tokens, "messages": msgs}
        if stream:
            body.update(stream=True, stream_options={"include_usage": True})
        headers = {"Authorization": f"Bearer {key}", "content-type": "application/json"}
    data = json.dumps(body).encode()
    log = usage_log or os.environ.get("LLM_USAGE_LOG", "")
    asked_chars = len(system) + len(prompt)

    def write_usage(reply: dict | None, sent: float, error: str = "", partial: "_Stream | None" = None,
                    stopped: bool = False) -> None:
        if not log:
            return
        u = (reply or {}).get("usage") or {}
        first_byte_at = (reply or {}).get("first_byte_at") or (partial.first_byte_at if partial else None)
        answered_by = (reply or {}).get("model") or (partial.answered_by if partial else None)
        rec = {"model": spec, "model_id": model_id(spec), "answered_by": answered_by, "tag": tag,
               "seconds": round(time.time() - t0, 1),
               "first_byte": round(first_byte_at - sent, 1) if first_byte_at else None,
               "in": u.get("input_tokens", u.get("prompt_tokens")),
               "out": u.get("output_tokens", u.get("completion_tokens")),
               "reasoning": (u.get("completion_tokens_details") or {}).get("reasoning_tokens"),
               "cache_read": u.get("cache_read_input_tokens", (u.get("prompt_tokens_details") or {}).get("cached_tokens")),
               "time": time.strftime("%Y-%m-%d %H:%M:%S")}
        if partial is not None and partial.first_byte_at is not None and rec["out"] is None:
            # what the provider generated before the stream ended early: estimated from the text that arrived
            rec.update({"in": rec["in"] or round(asked_chars / PROMPT_CHARS_PER_TOKEN),
                        "out": partial.reasoning_tokens() + partial.answer_tokens(),
                        "reasoning": partial.reasoning_tokens(), "estimated": True})
        rec.update(fields or {})
        if reasoning_budget is not None:
            rec["budget"] = reasoning_budget
        if stopped:
            rec["stopped"] = "reasoning budget"
        if error:
            rec["error"] = error[:300]
        with open(log, "a") as f:
            f.write(json.dumps(rec) + "\n")

    _pace(provider)
    t0 = time.time()   # the logged seconds are the call's, not the pacing wait's
    waits = 0
    attempt = 0
    while True:
        req = urllib.request.Request(url, data=data, headers=headers, method="POST")
        sent = time.time()   # this attempt's: time to first byte counts from here
        partial = _Stream(spec, reasoning_budget)
        try:
            with urllib.request.urlopen(req, timeout=timeout) as r:   # leaving it closes the connection
                if not stream:
                    reply = json.load(r)
                elif anthropic:
                    reply = _read_anthropic_events(r, partial)
                else:
                    reply = _read_events(r, partial)
            break
        except _Stopped:
            write_usage(None, sent, partial=partial, stopped=True)
            raise ReasoningBudgetSpent(spec, partial.reasoning_tokens(), reasoning_budget or 0,
                                       round(time.time() - t0, 1)) from None
        except urllib.error.HTTPError as e:
            msg = e.read().decode(errors="replace")[:300]
            # a rate limit that says when it lifts (Hyper's hourly limit: "try again in 42m12s", or a
            # Retry-After header): wait for it, without counting a retry (at most 3 such waits per call)
            after = e.headers.get("Retry-After") if e.headers else None
            wait = (_limit_lifts_in(msg) or (int(float(after)) if after and after.replace(".", "").isdigit() else None)
                    if e.code == 429 else None)
            if wait is not None and not wait_out_limits:
                write_usage(None, sent, error=f"HTTP {e.code}: {msg}")
                raise RuntimeError(f"{spec}: HTTP {e.code} (retry after {wait}s): {msg}") from None
            if wait is not None and waits < 3 and wait <= 3900:
                waits += 1
                time.sleep(wait + 10)
                t0 = time.time()   # the logged seconds are the call's, not the wait's
                continue
            if attempt == retries or e.code not in (429, 500, 502, 503, 504):
                write_usage(None, sent, error=f"HTTP {e.code}: {msg}")
                raise RuntimeError(f"{spec}: HTTP {e.code}: {msg}") from None
        except (urllib.error.URLError, TimeoutError, ConnectionError, http.client.HTTPException) as e:
            # (a dropped connection, also part way through a streamed reply: what had arrived was paid for)
            if partial.first_byte_at is not None or attempt == retries:
                write_usage(None, sent, error=f"connection failed: {e!r}", partial=partial)
            if attempt == retries:
                raise RuntimeError(f"{spec}: connection failed: {e!r}") from None
        except RuntimeError as e:   # an error event inside the stream
            write_usage(None, sent, error=str(e), partial=partial)
            raise
        time.sleep(5 * (attempt + 1))
        attempt += 1
    write_usage(reply, sent)
    if anthropic:
        return "".join(b.get("text", "") for b in reply.get("content", []) if b.get("type") == "text")
    return reply["choices"][0]["message"].get("content") or ""


if __name__ == "__main__":
    print(complete(sys.argv[1], sys.stdin.read()))
