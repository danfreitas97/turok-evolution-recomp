# turok-evolution-recomp

[Português (Brasil)](README.pt-BR.md)

Turok: Evolution for the original Xbox, **USA (NTSC) release**, recompiled into a
native program for Windows, macOS and Linux (including SteamOS and the Steam Deck). You bring your
own copy of the game, and it is built from that copy on your computer. This
repository and its releases contain no game code or data.

- [Play](#play): download, point it at your disc, play.
- [Build it yourself](#build-it-yourself): for developers working on the port.

# Play

## What you need

- Turok: Evolution, the USA (NTSC) release: a disc image (`.iso` or
  `.xiso`) or a folder with the disc's files in it. Other releases are refused
  (see [Releases of the game](#releases-of-the-game)).
- 64-bit Windows 10 or 11, a Mac with Apple Silicon (macOS 13 or later), or
  x86-64 Linux, SteamOS included, with Vulkan drivers.
- About 500 MB of space, plus room for the game's files if you give it a disc
  image (an image is unpacked once; a folder is used where it is).

Nothing else needs installing: the download carries everything it builds the
game with.

## Download and start

Download the file for your system from the
[latest release](https://github.com/danfreitas97/turok-evolution-recomp/releases/latest):

| System | File | Start |
| --- | --- | --- |
| Windows | `turok-<version>-windows-x86_64.zip` | Unzip it and run `turok.exe` |
| macOS | `turok-<version>-macos-arm64.tar.xz` | Unpack it, then see [macOS](#macos) below |
| Linux, SteamOS | `turok-<version>-linux-x86_64.tar.xz` | Unpack it and run `./turok` |

The first time it starts, choose your installation folder with **Choose folder...**,
then select your own **Select ISO...** (ISO/XISO).
Review both paths and press **Install**. Extraction, disc validation and
compilation run automatically, with a progress bar and live log. The full log
is saved as `build.log` inside the chosen installation folder. Once finished,
press **OK**, then **Play**. Later starts remember the installation.

Choose **English** or **PT-BR** using the flags at the top of the installer
and launcher. The selection is remembered; technical logs remain verbatim.

Use an empty destination folder. Updates and retries can reuse the existing
installation; saves are preserved. Keep the downloaded launcher package: it
contains the tools needed for updates.

**Windows** may say it protected your PC, because the release is not signed:
choose More info, then Run anyway.

### macOS

Move `Turok Evolution.app` to Applications. It is not signed with an Apple
Developer ID, so before opening it the first time, open Terminal and run

```sh
xattr -dr com.apple.quarantine "/Applications/Turok Evolution.app"
```

That clears the flag macOS puts on everything downloaded, from the app and the
tools it builds the game with. Then open it.

### Steam Deck and SteamOS

Unpack the release in desktop mode and run `turok` once to build the game.
To play from Game Mode, add `turok` to Steam (Add a Non-Steam Game) and give
the shortcut `icon.png` from the release folder as its icon.

## Status

Work in progress. This is a port of
[phobos665/turok-evolution-recomp](https://github.com/phobos665/turok-evolution-recomp),
which plays the PAL release (intro movies, front end, the first jungle level),
to the USA release's `default.xbe`. The toolkit is the same; what differs are
the addresses in `config/` and `src/recomp_manual.c`. No release is published
yet; build it yourself for now.

Features carried over from the PAL port: 60 fps and higher frame rates,
Hor+ widescreen, rendering up to 8x 640x480, fullscreen and variable refresh
rate, controllers, keyboard and mouse look.

## Controls

Controllers work as an Xbox pad. The keyboard plays as controller 1:

| Xbox | Key |
| --- | --- |
| D-pad | Arrows |
| A, B, X, Y | Z, X, A, S |
| Start, Back | Enter, Backspace |
| White, Black | Q, W |
| Left, right trigger | E, R |

Change them in the launcher's Input tab. Its mouse look option turns the
mouse into the right stick.

While playing:

| Key | |
| --- | --- |
| F9 | Show or hide the frame rate |
| F10 | Change the frame cap: adaptive, 60, 30, 120, 144, off |
| F11 | Save a screenshot and a capture of the frame, for bug reports |
| Alt+Enter | Switch between fullscreen and a window |

## Settings

In the launcher's Video tab:

| Setting | What it does |
| --- | --- |
| Resolution | Renders at 1 to 8 times 640x480 and filters down. 2 is much sharper and costs little |
| Widescreen | 16:9, with the wider camera |
| Texture sharpness | Anisotropic filtering, 1 to 16 |
| Frame rate | Adaptive (the default, 60 as on the console), 60, 30, 90, 120, 144, 165, 240, or uncapped |
| Show frame rate | Start with the counter on |
| Fullscreen | Borderless fullscreen |
| Variable refresh rate | In fullscreen, frames reach the display as they are ready. It has to be on in the display's driver too |

For smoother motion, raise the frame cap to your display's refresh rate rather
than using frame interpolation, which is not tuned for this game. Uncapped,
the game can run at several hundred frames a second, and the loading screens
at thousands; a cap saves power, which matters on a handheld.

## Where things are kept

The built game, its saves and logs are in the installation folder you choose.
The default locations are:

| | |
| --- | --- |
| Windows | `%LOCALAPPDATA%\xboxrecomp\builds\41430004\` |
| macOS | `~/Library/Application Support/xboxrecomp/builds/41430004/` |
| Linux | `~/.local/share/xboxrecomp/builds/41430004/` |

In it, `game/UDATA/41430004/` holds your saves, `build.log` is the build's
log and `turok_recomp.log` the game's. Your own disc or folder is never
written to.

The settings and controls are kept apart from the build:

| | Settings | Controls |
| --- | --- | --- |
| Windows | `%APPDATA%\xboxrecomp\titles\41430004.conf` | `%APPDATA%\xboxrecomp\input_bindings.json` |
| macOS | `~/Library/Application Support/xboxrecomp/titles/41430004.conf` | `~/Library/Application Support/xboxrecomp/input_bindings.json` |
| Linux | `~/.config/xboxrecomp/titles/41430004.conf` | `~/.config/xboxrecomp/input_bindings.json` |

## Updating

Download the new release and start it. It sees that the game was built by an
older release and offers **Build again**, with your disc remembered. Saves and
settings carry over.

## Problems

- The disc is refused: it is not the release below. Please open an issue with
  the title, region and SHA-1 the message shows.
- The build fails: open an issue and attach `build.log` (see [Where things
  are kept](#where-things-are-kept)).
- A black window and nothing else: attach `turok_recomp.log` from the same
  folder.
- A rendering bug in one place: press F11 there, and attach the `.bmp` and
  `.d3dcap` files it writes beside the game.

## Releases of the game

The game is recognised by the SHA-1 of its `default.xbe`:

| Release | `default.xbe` SHA-1 |
| --- | --- |
| USA (NTSC) | `25e1d77b216a2e557d7a5de6266cda64ae62627b` |

Any other disc is refused; please open an issue with its SHA-1. For the
European (PAL) release, use
[phobos665/turok-evolution-recomp](https://github.com/phobos665/turok-evolution-recomp).

# Build it yourself

For working on the port. This builds the same game by hand, from a clone,
against the [DirectXbox-Recomp](https://github.com/phobos665/DirectXbox-Recomp)
toolkit (`xboxrecomp/` here): the build translates the game's code to C, then
compiles it.

## What you need to build

Turok: Evolution, the USA (NTSC) release, extracted from your disc. The
build reads `default.xbe` and warns if it is a different release.

Windows 10 or 11, 64-bit:

- Visual Studio 2022 or 2019 with the Desktop development with C++ workload
- [CMake](https://cmake.org/download/) 3.20 or newer, on PATH
- Python 3.10 or newer with `capstone`: `py -3 -m pip install capstone`
- Git

Linux, x86-64:

- GCC or Clang, CMake 3.20 or newer, Ninja, pkg-config, Git, curl
- Development headers for X11 and Wayland, PulseAudio or PipeWire, OpenSSL,
  FreeType and fontconfig
- Vulkan drivers (Mesa's RADV, ANV, or NVIDIA's own)
- Python 3.10 or newer with `capstone`, in a virtual environment:
  `python3 -m venv ~/.venvs/recomp && ~/.venvs/recomp/bin/pip install capstone`

On Debian or Ubuntu that is

```sh
sudo apt install build-essential cmake ninja-build pkg-config git curl python3-venv \
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev \
    libxfixes-dev libxtst-dev libxkbcommon-dev libwayland-dev wayland-protocols \
    libegl-dev libdrm-dev libgbm-dev libpulse-dev libasound2-dev libudev-dev \
    libssl-dev libfreetype-dev libfontconfig-dev
```

SteamOS has no compiler, and its system is read-only, so build inside the
Freedesktop SDK, which runs as a Flatpak and has all of the above. It is
often installed already, because Flatpak apps such as VS Code use it; if
`flatpak list` does not show `org.freedesktop.Sdk` 25.08, install it with
`flatpak install flathub org.freedesktop.Sdk//25.08`. Open a shell in it from
Konsole:

```sh
flatpak run --filesystem=home --share=network --device=dri \
    --socket=wayland --socket=fallback-x11 --socket=pulseaudio \
    --command=bash org.freedesktop.Sdk//25.08
```

and do everything below in that shell, venv included. The game it builds also
runs outside it, straight from SteamOS.

## Build

```sh
git clone --recursive https://github.com/danfreitas97/turok-evolution-recomp.git
cd turok-evolution-recomp
```

Copy the extracted disc into a folder called `game`, so that
`game/default.xbe` exists with `Data/`, `Game.txt` and the `supertree*.tre`
archives beside it.

On Linux, fetch the shader compiler first (once; distributions do not package
it):

```sh
bash xboxrecomp/tools/linux/fetch_dxc.sh
```

Then:

```sh
py -3 scripts/build.py                        # Windows
~/.venvs/recomp/bin/python scripts/build.py   # Linux
```

The first run translates the game, which takes a few minutes, then compiles
it, which takes longer. Later runs only recompile. The program ends up in
`build/Release/`:

| | Windows | Linux |
| --- | --- | --- |
| The game | `turok_recomp.exe` | `turok_recomp` |
| Settings window, then the game | `turok_recomp_launcher.exe` | `turok_recomp_launcher` |

If your extracted games live somewhere else, `game` can be a link to the folder
(`ln -s "/path/to/Turok Evolution" game`), or pass `--game DIR` to the build and
set `RECOMP_GAME_DIR` when you play.

If you cloned without `--recursive`, run `git submodule update --init --recursive`.

## Running a build of your own

The controls and settings are the ones in [Play](#play). Every setting is also
a line in the settings file and an environment variable, which wins over the
file:

| Setting | File | Environment |
| --- | --- | --- |
| Resolution | `resolution_scale` | `RECOMP_RES_SCALE` |
| Widescreen | `widescreen` | `RECOMP_WIDESCREEN` |
| Texture sharpness | `anisotropy` | `RECOMP_ANISO` |
| Frame rate | `frame_cap` (0 for uncapped) | `RECOMP_FPS_CAP` |
| Show frame rate | `fps_overlay` | `RECOMP_FPS_OVERLAY` |
| Fullscreen | `fullscreen` | `RECOMP_FULLSCREEN` |
| Variable refresh rate | `vrr` | `RECOMP_VRR` |

The settings and controls files are as in [Where things are
kept](#where-things-are-kept); the controls' format is in
`xboxrecomp/docs/technical/input-binding.md`. What differs from a release:

| | Windows | Linux |
| --- | --- | --- |
| Saves | `game\UDATA\41430004\` | `game/UDATA/41430004/` |
| Log, when not started from a terminal | `build\Release\turok_recomp.log` | `build/Release/turok_recomp.log` |

## Other releases of the game

`config/` lists addresses inside the USA `default.xbe`: function entry points
the translation needs, and the XDK functions the toolkit replaces. The fixes
in `src/recomp_manual.c` attach to addresses in it too. Any other disc needs
its own addresses. The build prints a warning with
the SHA-1 and carries on, but the result is unlikely to run; please open an
issue with it.

## Updating a clone

```sh
git pull
git submodule update --init --recursive
py -3 scripts/build.py --relift
```

On Linux, use `~/.venvs/recomp/bin/python` in place of `py -3`. `--relift`
translates the game again, which is needed whenever the toolkit, `config/` or
`src/recomp_manual.c` changes. `--relift --from lift` repeats only the last
stage, which is enough after a change to `src/recomp_manual.c`.

## Making a release

Pushing a version tag (`git tag v1.0 && git push origin v1.0`) runs
`.github/workflows/release.yml`, which builds the Windows, macOS and Linux
downloads from `release/turok.json` and attaches them to the tag's GitHub
release. No game files are involved. How the downloads work is in the
toolkit's
[self-build-release.md](https://github.com/phobos665/DirectXbox-Recomp/blob/main/docs/technical/self-build-release.md).

## Build problems

- "no default.xbe in ...": the disc's files are not in `game/`.
- Linux, "DXC (the shader compiler) is missing": run
  `bash xboxrecomp/tools/linux/fetch_dxc.sh`, then build again.

# Credits

- [phobos665/turok-evolution-recomp](https://github.com/phobos665/turok-evolution-recomp)
  (GPL-3.0): the PAL port this repository is based on.
- [phobos665/DirectXbox-Recomp](https://github.com/phobos665/DirectXbox-Recomp)
  (GPL-3.0): the toolkit, `xboxrecomp/` here.
- [sp00nznet/xboxrecomp](https://github.com/sp00nznet/xboxrecomp) (MIT): the
  original toolkit DirectXbox-Recomp is forked from.

# Legal

You need your own copy of Turok: Evolution. Do not share the game's files, the
built game, a clone's `src/recomp/` (it is the game's translated code) or
anything in `build/`. This repository, its releases and xboxrecomp contain no
game code or assets.

The code in this repository is licensed under the GNU General Public License
v3.0; see [LICENSE](LICENSE). That covers the files committed here, not the game.

On Windows, **Video → Graphics card** selects the adapter. Automatic prefers
the GPU with more dedicated video memory. The selected card is recorded in
the game log. The build includes `turok.ico` for shortcuts.
