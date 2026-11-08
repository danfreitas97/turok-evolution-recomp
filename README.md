# Turok: Evolution — Static Recompilation

A native Windows (x86-64) build of **Turok: Evolution** (Xbox, 2002, NTSC-U) made by
statically recompiling the original `default.xbe` to C with
[xboxrecomp](https://github.com/sp00nznet/xboxrecomp). No emulator: the game's own
code, lifted instruction by instruction, compiled by MSVC and linked against
xboxrecomp's kernel, D3D8, audio and input layers.

> **Status: early.** The whole XBE lifts (12,987 functions, 0 failures) and builds.
> Getting it to boot, then to render, then to play is the work in progress. See
> [Progress](#progress).

## Bring your own game

**This repository contains no game code, data or assets.** You need your own copy of
*Turok: Evolution* (USA) for the original Xbox, dumped to an `.iso` (Redump or XISO).
Everything derived from the disc — the extracted files, the XBE, the generated C —
stays on your machine and is ignored by git.

| | |
|---|---|
| Title | Turok Evolution |
| Title ID | `0x41430004` (Acclaim) |
| Region | North America |
| XDK | 1.0.4627 |
| Entry point | `0x0001106F` |

Other regions have different XBEs and are not supported yet.

## Requirements

- Windows 10/11 x64
- Visual Studio 2022 or Build Tools 2022 with **Desktop development with C++**
  (the bundled CMake and Ninja are used)
- Python 3.10+ with `capstone` — `py -3 -m pip install capstone`
- Git
- ~10 GB free: ~2.2 GB for the extracted disc, ~60 MB of generated C, a few GB of build output

## Build

```powershell
git clone --recursive https://github.com/danfreitas97/turok-evolution-recomp.git
cd turok-evolution-recomp

# 1. Extract your disc (default destination: ..\turok\extracted, override with $env:TUROK_DATA)
.\scripts\extract-disc.ps1 -Image "D:\path\to\Turok - Evolution (USA).iso"

# 2. Analyse the XBE: sections, functions, library code, calling conventions (~2 min)
.\scripts\analyze.ps1

# 3. Lift it to C into src\recomp\gen (~2 min, ~1.3M lines)
.\scripts\recomp.ps1

# 4. Compile (Release by default; -Preset win-x64-debug for a debug build)
.\scripts\build.ps1

# 5. Run, with stdout/stderr captured in .\logs
.\scripts\run.ps1
```

If PowerShell refuses to run the scripts, allow local scripts once with
`Set-ExecutionPolicy -Scope CurrentUser RemoteSigned`.

The executable looks for the game in `.\game` (a junction `extract-disc.ps1`
creates) or in `$env:TUROK_GAME_DIR`.

## Layout

```
tools/xboxrecomp/      the toolkit (git submodule, pinned to a release tag)
src/main.c             host entry: loads default.xbe, maps Xbox memory, boots the guest
src/recomp_manual.c    hand-written overrides for lifted functions
src/recomp/gen/        generated C (never committed)
scripts/               extract -> analyze -> recomp -> build -> run
```

## Progress

- [x] Disc extraction and dump check
- [x] Analysis: 12,961 functions found across `.text` and the statically linked XDK
      sections (D3D, D3DX, XGRPH, DSOUND, XMV, XPP)
- [x] Lift: 12,987/12,987 functions, 1 unresolved call target stubbed
- [x] Boot past the CRT and XAPI start-up (needed a toolkit fix: the utility drive
      reports the 64 KB clusters the XBE's init flags ask for —
      [upstream PR #321](https://github.com/sp00nznet/xboxrecomp/pull/321); until it
      lands the submodule tracks `danfreitas97/xboxrecomp` branch `turok/v0.17`)
- [x] D3D init completes (D3D's GPU semaphore handed to the pushbuffer executor:
      `sub_00198650_enter` in `src/recomp_manual.c`)
- [x] DirectSound init (emulated APU answers the DSP mailbox)
- [x] Loads its text and data from the `supertree*.tre` archives and starts the
      intro movie (`data\video\intro.xmv`), ~35 s after launch
- [x] Render on the host GPU (Direct3D 11, `src/gpu_translator.c`) when there is
      one, CPU rasteriser as the fallback (`TUROK_RENDERER=cpu` forces it).
      The toolkit is FFNY Online's fork for now (submodule branch `turok/gpu`),
      which has the GPU path; the generic parts go upstream as they settle.
- [x] First frames on screen: the Acclaim logo and the legal screen
- [ ] Anti-aliased frames are shown zoomed 2x (top-left quarter): Turok draws
      each scene into a 1280x480 supersampled surface and filters it to the
      640x480 front buffer, which the translator does not resolve right yet
- [ ] Intro movie visible
- [ ] Audio: emulated APU, clean since the 64 ms XAudio2 queue; the DSP is
      still a passthrough stub (no effects)
- [ ] Linux/macOS on the GPU (the fork's Vulkan device needs a host window)
- [ ] Menus, audio, input
- [ ] In-game

## Debugging

The toolkit's runtime is driven by `RECOMP_*` environment variables (watchdog, kernel
logs, ICALL traces, vblank pacing and more) — see
[environment-variables.md](https://github.com/sp00nznet/xboxrecomp/blob/main/docs/runtime/environment-variables.md)
and the toolkit's [debugging guide](https://github.com/sp00nznet/xboxrecomp/blob/main/docs/pipeline/06-debugging.md).
Builds keep a PDB and a linker map (`build\<preset>\turok_recomp.map`), so a crash
report names the guest function (`sub_XXXXXXXX`) it happened in.

## Legal

Turok: Evolution is © its respective rights holders. This project is not affiliated
with or endorsed by them. It distributes only original tooling and source code; you
must own the game you build it with.

## Credits

- [xboxrecomp](https://github.com/sp00nznet/xboxrecomp) by sp00nznet and contributors (MIT; parts LGPL-2.1, see the submodule's `NOTICE`)
- Project structure inspired by [DefJamFFNY-recomp](https://github.com/vyanhursky/DefJamFFNY-recomp)
