#!/usr/bin/env python3
"""Snapshot what each provider reports as used/left (keys from pi's auth.json,
never printed): tools/convert/quota.py [OUT.json]. Hyper: credit balance;
Z.ai: coding-plan quota windows; MiniMax: coding-plan remaining counts;
OpenRouter: credits used. Compare two snapshots for a run's real cost."""
import json, os, sys, time, urllib.error, urllib.request
auth = json.load(open(os.path.expanduser("~/.pi/agent/auth.json")))
key = lambda p: auth[p]["key"]


def get(url, hdr):
    req = urllib.request.Request(url, headers=dict(hdr, **{"User-Agent": "curl/8"}))
    try:
        with urllib.request.urlopen(req, timeout=20) as r:
            return json.load(r)
    except Exception as e:
        return {"error": str(e)[:200]}


snap = {"time": time.strftime("%Y-%m-%d %H:%M:%S")}
snap["hyper"] = get("https://hyper.charm.land/v1/credits", {"Authorization": "Bearer " + key("hyper")})
z = get("https://api.z.ai/api/monitor/usage/quota/limit", {"Authorization": key("zai")})
snap["zai"] = [{k: l.get(k) for k in ("unit", "number", "usage", "currentValue", "remaining", "nextResetTime")}
               for l in (z.get("data") or {}).get("limits", [])] or z
m = get("https://api.minimax.io/v1/api/openplatform/coding_plan/remains", {"Authorization": "Bearer " + key("minimax")})
snap["minimax"] = [{k: r.get(k) for k in ("model_name", "current_interval_total_count", "current_interval_usage_count",
                                          "current_interval_remaining_percent", "current_weekly_remaining_percent")}
                   for r in m.get("model_remains", [])] or m
snap["openrouter"] = (get("https://openrouter.ai/api/v1/credits", {"Authorization": "Bearer " + key("openrouter")}).get("data"))
text = json.dumps(snap, indent=1)
if len(sys.argv) > 1:
    open(sys.argv[1], "w").write(text)
print(text)
