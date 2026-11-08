"""Sample where each thread of the running turok_recomp.exe is, by function name.

A busy loop leaves no crash and no wait to log: the watchdog shows a guest
stack scan but not which function is spinning. This suspends each thread for
a moment, reads its host RIP, resolves it against the linker map
(build/<preset>/turok_recomp.map, from /MAP) and counts hits per function, so
the hot lifted function (sub_XXXXXXXX) or runtime function is named.

    py -3 scripts/sample-threads.py [--preset win-x64-release] [--samples 40] [--interval 0.05]

Run it while the game runs (scripts/run.ps1 in another shell). Windows only.
"""
import argparse
import bisect
import collections
import ctypes
import ctypes.wintypes as wt
import os
import re
import subprocess
import sys
import time

EXE = "turok_recomp.exe"
TH32CS_SNAPTHREAD = 0x4
THREAD_ACCESS = 0x2 | 0x8 | 0x40      # SUSPEND_RESUME | GET_CONTEXT | QUERY_INFORMATION
CONTEXT_CONTROL = 0x100000 | 0x1      # AMD64 | CONTROL

k32 = ctypes.WinDLL("kernel32", use_last_error=True)
k32.OpenThread.restype = wt.HANDLE
k32.CreateToolhelp32Snapshot.restype = wt.HANDLE


class THREADENTRY32(ctypes.Structure):
    _fields_ = [("dwSize", wt.DWORD), ("cntUsage", wt.DWORD), ("th32ThreadID", wt.DWORD),
                ("th32OwnerProcessID", wt.DWORD), ("tpBasePri", wt.LONG),
                ("tpDeltaPri", wt.LONG), ("dwFlags", wt.DWORD)]


class CONTEXT(ctypes.Structure):
    """AMD64 CONTEXT up to Rip; the rest is padding. Must be 16-byte aligned."""
    _fields_ = [("Home", ctypes.c_ulonglong * 6), ("ContextFlags", wt.DWORD), ("MxCsr", wt.DWORD),
                ("Seg", wt.WORD * 6), ("EFlags", wt.DWORD), ("Dr", ctypes.c_ulonglong * 6),
                ("Gpr", ctypes.c_ulonglong * 16), ("Rip", ctypes.c_ulonglong),
                ("Rest", ctypes.c_byte * 1024)]


def find_pid():
    out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq " + EXE, "/FO", "CSV", "/NH"],
                         capture_output=True, text=True).stdout
    m = re.search(r'"%s","(\d+)"' % re.escape(EXE), out)
    return int(m.group(1)) if m else None


def threads(pid):
    snap = k32.CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0)
    te = THREADENTRY32(); te.dwSize = ctypes.sizeof(te)
    tids = []
    if k32.Thread32First(snap, ctypes.byref(te)):
        while True:
            if te.th32OwnerProcessID == pid:
                tids.append(te.th32ThreadID)
            if not k32.Thread32Next(snap, ctypes.byref(te)):
                break
    k32.CloseHandle(snap)
    return tids


def load_map(path):
    """Sorted (address, name) of every public symbol, at the map's preferred base.

    The build links with /DYNAMICBASE:NO, so the image loads at that base and
    a RIP needs no rebasing."""
    syms = []
    pat = re.compile(r"^\s*[0-9A-Fa-f]{4}:[0-9A-Fa-f]{8}\s+(\S+)\s+([0-9A-Fa-f]{16})\s")
    with open(path, errors="replace") as f:
        for line in f:
            m = pat.match(line)
            if m:
                syms.append((int(m.group(2), 16), m.group(1)))
    syms.sort()
    return [a for a, _ in syms], [n for _, n in syms]


def name_of(addrs, names, rip):
    # Past the last symbol by more than a function's worth, or below the
    # first: the thread is in a system DLL (a wait, a sleep, a syscall),
    # which the map cannot name.
    if not addrs or rip < addrs[0] or rip > addrs[-1] + 0x10000:
        return "<outside exe: system DLL>"
    i = bisect.bisect_right(addrs, rip) - 1
    return names[i]


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--preset", default="win-x64-release")
    ap.add_argument("--samples", type=int, default=40)
    ap.add_argument("--interval", type=float, default=0.05)
    ap.add_argument("--top", type=int, default=6)
    a = ap.parse_args()

    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    mappath = os.path.join(root, "build", a.preset, "turok_recomp.map")
    if not os.path.exists(mappath):
        print("no map at " + mappath); return 1
    pid = find_pid()
    if not pid:
        print(EXE + " is not running"); return 1
    addrs, names = load_map(mappath)

    hits = collections.defaultdict(collections.Counter)
    ctx = (ctypes.c_byte * (ctypes.sizeof(CONTEXT) + 16))()
    base = (ctypes.addressof(ctx) + 15) & ~15
    c = CONTEXT.from_address(base)
    for _ in range(a.samples):
        for tid in threads(pid):
            h = k32.OpenThread(THREAD_ACCESS, False, tid)
            if not h:
                continue
            if k32.SuspendThread(h) != 0xFFFFFFFF:
                c.ContextFlags = CONTEXT_CONTROL
                if k32.GetThreadContext(h, ctypes.c_void_p(base)):
                    hits[tid][name_of(addrs, names, c.Rip)] += 1
                k32.ResumeThread(h)
            k32.CloseHandle(h)
        time.sleep(a.interval)

    # Threads that are always in a wait are not interesting: list the busy
    # ones first, by how many samples were not inside a wait/sleep routine.
    idle = re.compile(r"(?i)(Wait|Sleep|NtDelay|ZwDelay|NtWait|ZwWait|ZwRemove|NtRemove|outside exe)")
    def busy(tid):
        return sum(n for f, n in hits[tid].items() if not idle.search(f))
    for tid in sorted(hits, key=busy, reverse=True):
        total = sum(hits[tid].values())
        print("thread %d: %d/%d samples busy" % (tid, busy(tid), total))
        for f, n in hits[tid].most_common(a.top):
            print("    %3d  %s" % (n, f))
    return 0


if __name__ == "__main__":
    sys.exit(main())
