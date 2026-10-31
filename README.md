# ROOTDoom

Classic Doom running inside a native [CERN ROOT](https://root.cern/) canvas.
The real [doomgeneric](https://github.com/ozkl/doomgeneric) engine runs on a
worker thread while ROOT handles the window, graphics and keyboard input.

![Doom running inside a ROOT canvas](assets/screenshot.png)

## Build and run

Requires Linux with X11, ROOT 6 with GUI and ASImage support, CMake 3.24+,
and GCC or Clang with C++17 support. Verified with ROOT 6.40.04 and GCC 13.3
on Linux with an Ubuntu 24.04 base. Sound is disabled.

Activate your ROOT installation, then build from the project directory:

```sh
source /path/to/root/bin/thisroot.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/rootdoom --iwad /absolute/path/to/doom1.wad
```

CMake downloads a pinned version of doomgeneric. Supply your own Doom IWAD;
the shareware `doom1.wad` can be extracted from the original
[Doom95 shareware archive](https://www.gamers.org/pub/idgames/idstuff/doom/win95/).
Game data is not bundled or downloaded automatically. Configuration and saves
are stored in `rootdoom-data/`; use `--data-dir DIR` to change this.

Click the canvas to focus it. Use **arrows** to move and turn, **Ctrl** to fire,
**Space** to open doors, **Shift** to run, **Enter** to select and **Escape**
for the menu. Close the window or quit through Doom's menu to stop.

## Run from ROOT

After building, start ROOT from the project directory:

```sh
root -l
```

```cpp
.x macros/doom.C("/absolute/path/to/doom1.wad")
```

The game runs in the same ROOT process and leaves the prompt usable. Closing
the game keeps ROOT open. Restart ROOT before launching another game.
For a custom build directory, set `ROOTDOOM_LIBRARY` to the absolute path of
`librootdoom.so` before starting ROOT.

## License

ROOTDoom is [GPL-2.0-or-later](LICENSE). The pinned doomgeneric sources retain
their original copyright and license notices. Doom game data is separately
copyrighted and is not included.
