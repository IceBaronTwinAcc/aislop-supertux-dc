# Building for Dreamcast

This port uses the current SDL 1.2 and GLdc packages from KallistiOS
kos-ports. Source the KallistiOS environment, then install SDL (which
installs its libGL dependency), libpng, libjpeg, and libmp3:

```sh
make -C "$KOS_PORTS/SDL" install clean
make -C "$KOS_PORTS/libpng" install clean
make -C "$KOS_PORTS/libjpeg" install clean
make -C "$KOS_PORTS/libmp3" install clean
```

The bundled SDL_image and fake SDL_mixer compatibility libraries must be
rebuilt against that installed SDL:

```sh
make -C ports/SDL_image-1.2.12
make -C ports/mikmod/lmikmod/dc
make -C ports/fake_mixer
make
```

A direct KallistiOS PVR renderer can be selected instead of the default GLdc
renderer. This is a local build toggle and does not change saved video
settings:

```sh
make clean
make PVR_RENDERER=1
```

For full-speed rendering, apply `ports/gldc-pipeline.patch` to the GLdc source
and build it. Pass the resulting library directory when building SuperTux:

```sh
git -C /path/to/GLdc apply /path/to/supertux-dc/ports/gldc-pipeline.patch
cmake -S /path/to/GLdc -B /path/to/GLdc/dcbuild \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/GLdc/toolchains/Dreamcast.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_SAMPLES=OFF -DBUILD_TESTS=OFF
cmake --build /path/to/GLdc/dcbuild
make GLDC_LIBDIR=/path/to/GLdc/dcbuild
```

The patch removes GLdc's redundant full-render wait after each normal
framebuffer swap without suppressing synchronization inside KallistiOS.
Set `PVR_PIPELINE=0` when building against an unpatched GLdc library.

Gameplay writes a once-per-second FPS and frame-time profile to the serial
console. An unattended level-one profiling build can be created with:

```sh
make PROFILE_AUTORUN=1
```

Select another level with `PROFILE_AUTORUN_LEVEL`, for example:

```sh
make PROFILE_AUTORUN=1 PROFILE_AUTORUN_LEVEL=3
```

`PROFILE_AUTORUN_START_X` can position Tux at a specific horizontal coordinate
for profiling later sections of long levels.

Set `PROFILE_AUTORUN_WARM_PREVIOUS=1` to run each preceding level for two
seconds before the selected level. `PROFILE_AUTORUN_WORLDMAP=1` instead keeps
one world map alive and enters each level node through the normal
`WorldMap::update()` lifecycle, rendering the map and level for two seconds
apiece. `PROFILE_KEEP_SESSION_TEXTURES=1` disables synchronized texture
cleanup for allocator diagnostics.

The legacy `ports/SDLDH1.0` tree is retained for reference only and is not used
by the game or its supporting libraries.
