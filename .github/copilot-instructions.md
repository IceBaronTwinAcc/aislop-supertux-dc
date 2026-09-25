# Copilot instructions

## Build and run

This is a Dreamcast/KallistiOS port. Source the KallistiOS environment first so
`KOS_BASE`, `KOS_PORTS`, the `kos-*` tools, and `KOS_LOADER` are available.

Install the required kos-ports dependencies:

```sh
make -C "$KOS_PORTS/SDL" install clean
make -C "$KOS_PORTS/libpng" install clean
make -C "$KOS_PORTS/libjpeg" install clean
make -C "$KOS_PORTS/libmp3" install clean
```

Build the bundled compatibility libraries, then the game:

```sh
make -C ports/SDL_image-1.2.12
make -C ports/mikmod/lmikmod/dc
make -C ports/fake_mixer
make
```

Useful targets and variants:

```sh
make src/world.o                  # compile one translation unit
make clean
make run                          # load supertux.elf with KOS_LOADER
make GLDC_LIBDIR=/path/to/GLdc/dcbuild
make PVR_PIPELINE=0               # required with an unpatched GLdc
make clean && make PVR_RENDERER=1 # direct KallistiOS PVR renderer
make PROFILE_AUTORUN=1
make PROFILE_AUTORUN=1 PROFILE_AUTORUN_LEVEL=3
```

`PROFILE_AUTORUN_START_X`, `PROFILE_AUTORUN_WARM_PREVIOUS`,
`PROFILE_AUTORUN_MOVE`, `PROFILE_AUTORUN_WORLDMAP`, and
`PROFILE_KEEP_SESSION_TEXTURES` enable narrower serial-console profiling
scenarios. `PROFILE_AUTORUN_INVINCIBLE=1` prevents enemy-contact damage during
autorun while retaining pit and crushing deaths. `PROFILE_AUTORUN_RUNTIME`
controls the milliseconds spent in each profiled level and defaults to 2000.
`PROFILE_REFRESH_TOGGLE=1` switches from the region default to 50 Hz after
shared assets load so renderer recreation can be tested under autorun. The
default `PVR_PIPELINE=1` assumes that `ports/gldc-pipeline.patch` has been
applied to the GLdc build.

There is no game-owned automated test or lint target. Treat a successful
cross-build plus execution on Dreamcast or an emulator as validation; the
autorun builds emit per-second frame timing to the serial console.

## Debug with Flycast and GDB

A Flycast build compiled with `ENABLE_GDB_SERVER=ON` is available at:

```sh
/home/ice/.copilot/session-state/8416d03a-cfde-4798-b8e7-7b271571eda2/files/flycast-gdb-src/build/flycast
```

The local Dreamcast image builder is:

```sh
/home/ice/mkdcdisc/builddir/mkdcdisc
```

Follow the
[Dreamcast Wiki Flycast GDB procedure](https://dreamcast.wiki/Setting_up_Flycast_GDB).
Flycast accepts transient settings with `-config`, so smoke tests should not
modify `~/.config/flycast/emu.cfg`.

Create a CDI whose executable exactly matches the ELF used by GDB. Clean-build
the desired renderer and package the repository's `cd_root` contents:

```sh
source /opt/toolchains/dc/kos/environ.sh
make clean
make PVR_RENDERER=1

SMOKE_DIR=/tmp/supertux-smoke
mkdir -p "$SMOKE_DIR"
cp supertux.elf "$SMOKE_DIR/supertux-pvr-smoke.elf"
/home/ice/mkdcdisc/builddir/mkdcdisc \
  -e supertux.elf \
  -D cd_root \
  -i sega.mr \
  -n SuperTux \
  -o "$SMOKE_DIR/supertux-pvr-smoke.cdi"
```

Use a temporary artifact directory rather than replacing the repository's
normal `supertux.cdi`. The CDI and symbol ELF must come from the same clean
build or source breakpoints and backtraces will not match.

For an unattended smoke test, enable the GDB server and serial console
transiently. This local Flycast build also adds `Debug.FastForward`, which
activates Flycast's normal fast-forward mode at game startup without requiring
a mapped controller hotkey. Keep it transient so interactive runs retain normal
timing:

```sh
FLYCAST=/home/ice/.copilot/session-state/8416d03a-cfde-4798-b8e7-7b271571eda2/files/flycast-gdb-src/build/flycast
SMOKE_DIR=/tmp/supertux-smoke

set -o pipefail
time timeout --signal=TERM 60s "$FLYCAST" \
  -config config:Debug.GDBEnabled=yes,config:Debug.GDBPort=3263,config:Debug.GDBWaitForConnection=no,config:Debug.SerialConsoleEnabled=yes,config:Debug.FastForward=yes,rend:vsync=no \
  "$SMOKE_DIR/supertux-pvr-smoke.cdi" \
  2>&1 | tee "$SMOKE_DIR/pvr-smoke.log"
```

Exit status `124` is expected when `timeout` ends an otherwise healthy run. A
successful smoke test must boot KallistiOS, find `/cd/data`, enter the title,
world map, or a level, and show continuing frame/profile output without SH4
exceptions, assertions, resets, or texture-memory failures. Fast-forward mutes
emulated audio and removes Flycast's normal frame pacing; compare `time`'s wall
clock result with the game's serial timestamps to confirm that emulated time is
advancing faster than real time. `rend:vsync=no` remains explicit so the host
display cannot reintroduce VSync throttling.

To inspect a running smoke test, attach GDB in another terminal:

```sh
gdb -q "$SMOKE_DIR/supertux-pvr-smoke.elf"
(gdb) set architecture sh4
(gdb) set endian little
(gdb) target remote :3263
(gdb) bt
(gdb) detach
```

Connecting pauses emulation. Detach or use `continue` after inspecting it.
Setting `Debug.GDBWaitForConnection=yes` is useful when execution must stop
before startup, but confirm that Flycast actually waits before relying on an
early breakpoint such as `main`.

Persistent configuration remains available when needed. In
`~/.config/flycast/emu.cfg`, under `[config]`, enable:

```ini
Debug.GDBEnabled = yes
Debug.GDBPort = 3263
Debug.GDBWaitForConnection = no
Debug.SerialConsoleEnabled = yes
```

Launch a matching CDI interactively in one terminal:

```sh
FLYCAST=/home/ice/.copilot/session-state/8416d03a-cfde-4798-b8e7-7b271571eda2/files/flycast-gdb-src/build/flycast
"$FLYCAST" "$PWD/supertux.cdi"
```

In another terminal, load symbols from the ELF built into that CDI and attach:

```sh
gdb -q supertux.elf
(gdb) set architecture sh4
(gdb) set endian little
(gdb) target remote :3263
```

This machine's `/usr/bin/gdb` supports SH4; `gdb-multiarch` is also suitable.
Common commands are:

```gdb
break main
break GameSession::run
continue
next
nexti
bt
info registers
layout asm
layout regs
```

Use `continue` after setting breakpoints; press Ctrl-C in GDB to interrupt a
running game.

## Architecture

- `src/supertux.cpp` owns process startup and shutdown: directory/config setup,
  SDL/GLdc initialization, shared resource loading, title/world-map entry, and
  final resource teardown.
- `WorldMapNS::WorldMap` is the campaign layer. Entering a node creates a
  `GameSession`; `GameSession` owns the fixed-step event/update/render loop and
  a `World`; `World` owns the loaded `Level`, player, enemies, transient game
  objects, particles, and level rendering.
- Rendering goes through `Surface` in `texture.*`, which selects SDL, OpenGL,
  or the build-time direct PVR implementation. The default Dreamcast path is
  SDL 1.2 plus GLdc; `PVR_RENDERER=1` selects direct KallistiOS PVR submission.
  Sprites, text, tiles, and world-map drawing ultimately share this
  surface/texture layer.
- `World::prepare_graphics()` preloads the current level's tiles, player
  sprites, enemies, and common HUD/gameplay graphics into texture memory.
  `GameSession` performs synchronized texture release before destroying a
  world because pipelined PVR rendering can still reference submitted
  textures.
- Game data lives under `cd_root/data`. Dreamcast builds read it as `/cd/data`;
  host-side code uses `cd_root/data`. Levels (`.stl`), world maps (`.stwm`),
  tile sets (`.stgt`/`.stwt`), sprite definitions (`.strf`), and configuration
  are Lisp-like files parsed by `lispreader.*` and `LispReader`.
- Dreamcast saves/configuration use VMU storage under `/vmu/a1`; `dreamcast.*`
  wraps VMU package metadata and direct Maple controller input.
- Audio is adapted through `ports/fake_mixer`, bundled libmikmod, and KOS MP3
  support. `ports/SDLDH1.0` is legacy reference code and is not part of the
  current game build.

## Repository-specific conventions

- Keep Dreamcast-only APIs behind `__DREAMCAST__`. Gameplay, world-map, and
  menu controller input use Maple directly on Dreamcast; SDL joystick event
  handling is primarily the non-Dreamcast fallback.
- The codebase uses manual ownership and explicit lifecycle pairs:
  `loadshared()`/`unloadshared()`, `activate_world()`/`deactivate_world()`,
  `prepare()`/`unprepare()`, and `new`/`delete`. Preserve these boundaries when
  adding resources; several subsystems also expose `current_` singleton-style
  pointers.
- Do not delete or recycle GL textures while queued PVR work may reference
  them. Use the `Surface::begin_synchronized_texture_release()` /
  `Surface::end_texture_release()` flow around session-level cleanup, and keep
  batching bracketed by `Surface::begin_draw_batch()` /
  `Surface::end_draw_batch()`.
- Level rendering assumes 32-pixel tiles, 15 tile rows, and a 640x480 viewport
  (21 visible columns including the partially visible edge). Changes to tile
  traversal, collision, or batching must preserve those assumptions unless the
  related formats and systems are updated together.
- CDFS paths are normalized in several loaders by removing `-` from asset and
  level filenames with `ReplaceAll`. Follow the same path normalization for new
  references that pass through Dreamcast CD storage.
- Add new game translation units to `OBJS` in the root `Makefile`; dependency
  files are generated with `-MMD -MP` and included automatically.
- Shared assets belong in `loadshared()` only when they persist across levels.
  Level-specific graphics should be loaded by `Level`/`World`, prepared before
  gameplay, and released at the session boundary to avoid exhausting
  Dreamcast texture memory.
