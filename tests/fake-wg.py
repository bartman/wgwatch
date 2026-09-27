#!/usr/bin/env python3
"""Fake `wg` executable for wgwatch demos. Only speaks `show all dump`.

Usage:
    FAKE_WG_INTERFACES=2 FAKE_WG_PEERS=1,2 \\
    FAKE_WG_LOAD=zero/low,mid/high,bursty/random \\
    FAKE_WG_LOW=1 FAKE_WG_MID=2 FAKE_WG_HIGH=4 \\
    wgwatch --command tests/fake-wg.py

Design
------
The script is stateless between wgwatch restarts but stateful across polls:
everything dynamic lives in the state file, everything static is derived
deterministically from the peer index so output is stable call to call.

Peers are numbered globally from 1 in interface order (wg0's peers first).
Peer n always gets endpoint n.n.n.n:<50000+n>, allowed IPs
192.168.0.n/32, and base64 keys from sha256("fake-wg-peer-n") (real
44-char shape, fabricated content). Interfaces get wgN names, listen
ports 51820+N, and matching fabricated private/public keys. Handshakes
report the current time, so all peers always look freshly connected.

FAKE_WG_LOAD is a flat sequence split on "," and "/" and dealt out as
rx,tx,rx,tx... over the peers in global order (a bare name covers one
direction; the list cycles if shorter than 2*peers). Rates are MB/s
(1024*1024 bytes) against FAKE_WG_LOW/MID/HIGH (default 1/2/4):
zero holds 0; low/mid/high hold their level; random draws
uniform(0, HIGH) each poll; bursty holds low 1s, mid 1s, high 4s,
mid 1s, low 1s, zero 4s (12s cycle); sin/cos swing 0..HIGH on a 10s period.

State file (.fake-wg-state in the invocation cwd, JSON):
{"sup", "t0", "last", "rx": {peer: bytes}, "tx": {...}}. "sup" is the
supervisor PID: the parent chain walked past privilege wrappers
(sudo/doas/su/runuser, which re-fork every poll) to the persistent
loop shell. A mismatch means a new supervisor, so counters, the time
origin "t0", and "last" reset to zero/now. Otherwise each poll
adds rate(t - t0) * MB * dt with dt clamped to 5s, so a paused supervisor
can't teleport counters. A missing or corrupt file starts fresh.

Environment (all optional):
    FAKE_WG_INTERFACES  number of wgN interfaces (default 2)
    FAKE_WG_PEERS       comma-separated peer counts per interface
                        (default "1,2": 1 peer on wg0, 2 on wg1;
                        short lists pad with 0)
    FAKE_WG_LOAD        flat load sequence, see above
                        (default "sin/cos,bursty,bursty,random,random")
    FAKE_WG_LOW/MID/HIGH  MB/s levels (default 1/2/4)
"""

import base64
import hashlib
import json
import math
import os
import random
import re
import sys
import time

STATE_PATH = ".fake-wg-state"
MB = 1024 * 1024
MAX_DT = 5.0
# Assumed poll interval for the first invocation after a reset, so it
# emits data instead of zeros (matches wgwatch's default update).
FIRST_DT = 1.0
DEFAULT_INTERFACES = 2
DEFAULT_PEERS = [1, 2]
DEFAULT_LOAD = "sin/cos,bursty,bursty,random,random"
DEFAULT_LOW, DEFAULT_MID, DEFAULT_HIGH = 1.0, 2.0, 4.0


def fake_key(tag):
    digest = hashlib.sha256(tag.encode()).digest()
    return base64.b64encode(digest).decode()


def getenv_float(name, default):
    try:
        return float(os.environ.get(name, default))
    except (TypeError, ValueError):
        return default


def peer_counts(n_ifaces):
    try:
        counts = [int(x) for x in os.environ.get("FAKE_WG_PEERS", "").split(",")
                  if x.strip() != ""]
        if not counts:
            raise ValueError
    except ValueError:
        counts = list(DEFAULT_PEERS)
    counts = counts[:n_ifaces]
    while len(counts) < n_ifaces:
        counts.append(0)
    return [max(0, c) for c in counts]


def load_names():
    raw = os.environ.get("FAKE_WG_LOAD", DEFAULT_LOAD)
    return [x.strip().lower() for x in re.split(r"[/,]", raw) if x.strip()]


_warned = set()


def rate_of(name, t, low, mid, high):
    if name == "zero":
        return 0.0
    if name == "low":
        return low
    if name == "mid":
        return mid
    if name == "bursty":
        t12 = t % 12.0
        if t12 < 1.0:
            return low
        if t12 < 2.0:
            return mid
        if t12 < 6.0:
            return high
        if t12 < 7.0:
            return mid
        if t12 < 8.0:
            return low
        return 0.0
    if name == "random":
        return random.uniform(0.0, high)
    if name == "sin":
        return high * (0.5 + 0.5 * math.sin(2.0 * math.pi * t / 10.0))
    if name == "cos":
        return high * (0.5 + 0.5 * math.cos(2.0 * math.pi * t / 10.0))
    if name not in _warned:
        print(f"fake-wg: unknown load '{name}', using zero", file=sys.stderr)
        _warned.add(name)
    return 0.0


# Wrappers that re-fork every poll with a fresh PID (sudo forks per
# invocation; the loop shell behind it is the stable supervisor).
_WRAPPERS = {"sudo", "doas", "su", "runuser"}


def proc_argv0(pid):
    try:
        with open(f"/proc/{pid}/cmdline", "rb") as f:
            return f.read().split(b"\0")[0].decode(errors="replace")
    except OSError:
        return ""


def proc_ppid(pid):
    try:
        with open(f"/proc/{pid}/stat") as f:
            stat = f.read()
        rest = stat.rsplit(")", 1)[1].split()
        return int(rest[1])
    except (OSError, IndexError, ValueError):
        return 0


def supervisor_pid():
    """PID identifying one supervisor session.

    wgwatch polls by re-executing this script; a bare getppid() changes
    every poll when a privilege wrapper (sudo/doas) sits between the
    persistent loop shell and us, which would reset the counters each
    time. Walk past those wrappers to the loop shell, whose PID is
    stable for the session and changes on restart.
    """
    pid = os.getppid()
    for _ in range(6):
        if not pid:
            break
        if os.path.basename(proc_argv0(pid)) not in _WRAPPERS:
            break
        pid = proc_ppid(pid)
    return pid or os.getppid()


def fresh_state(now):
    # Backdate "last" so the first poll after a reset already carries one
    # interval of data instead of zeros.
    return {"sup": supervisor_pid(), "t0": now, "last": now - FIRST_DT,
            "rx": {}, "tx": {}}


def load_state(now):
    try:
        with open(STATE_PATH) as f:
            st = json.load(f)
        if st.get("sup", st.get("ppid")) != supervisor_pid():
            return fresh_state(now)
        st.setdefault("rx", {})
        st.setdefault("tx", {})
        return st
    except (OSError, ValueError, AttributeError):
        return fresh_state(now)


def main():
    try:
        n_ifaces = int(os.environ.get("FAKE_WG_INTERFACES",
                                      DEFAULT_INTERFACES))
    except ValueError:
        n_ifaces = DEFAULT_INTERFACES
    n_ifaces = max(0, n_ifaces)
    counts = peer_counts(n_ifaces)
    names = load_names()
    low = getenv_float("FAKE_WG_LOW", DEFAULT_LOW)
    mid = getenv_float("FAKE_WG_MID", DEFAULT_MID)
    high = getenv_float("FAKE_WG_HIGH", DEFAULT_HIGH)

    now = time.time()
    st = load_state(now)
    t0 = st.get("t0", now)
    dt = max(0.0, min(now - st.get("last", now), MAX_DT))
    t = max(0.0, now - t0)

    out = []
    g = 0
    for i in range(n_ifaces):
        out.append("\t".join([
            f"wg{i}", fake_key(f"fake-wg-priv-{i}"),
            fake_key(f"fake-wg-pub-{i}"), str(51820 + i), "off",
        ]))
        for _ in range(counts[i]):
            g += 1
            pid = f"wg{i}/{g}"
            rx_name = names[(2 * (g - 1)) % len(names)] if names else "zero"
            tx_name = names[(2 * (g - 1) + 1) % len(names)] if names else "zero"
            rx = int(st["rx"].get(pid, 0) + rate_of(rx_name, t, low, mid,
                                                    high) * MB * dt)
            tx = int(st["tx"].get(pid, 0) + rate_of(tx_name, t, low, mid,
                                                    high) * MB * dt)
            st["rx"][pid] = rx
            st["tx"][pid] = tx
            out.append("\t".join([
                f"wg{i}", fake_key(f"fake-wg-peer-{g}"), "(none)",
                f"{g}.{g}.{g}.{g}:{50000 + g}", f"192.168.0.{g}/32",
                str(int(now)), str(rx), str(tx), "off",
            ]))

    st["last"] = now
    st["t0"] = t0
    with open(STATE_PATH, "w") as f:
        json.dump(st, f)
    sys.stdout.write("\n".join(out) + "\n")


main()
