# ROOTDoom

Doom with 3D graphics rendered through [CERN ROOT](https://root.cern/).
The real [doomgeneric](https://github.com/ozkl/doomgeneric/tree/e257a09527062bfb986b6e5e0780839b6cbd5b71) engine runs the game
simulation while ROOT's embedded OpenGL viewer draws textured levels, moving
sectors, sprites and weapons. ROOT also handles the window and keyboard input.

![ROOTDoom geometry renderer](assets/screenshot.png)

## Build and play

Requires Linux with X11/GLX, ROOT 6 with GUI, ASImage and OpenGL (RGL) support,
CMake 3.24+, and GCC or Clang with C++17 support. Verified with ROOT 6.40.04
and GCC 13.3 on Linux with an Ubuntu 24.04 base.

```sh
source /path/to/root/bin/thisroot.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/rootdoom --iwad /absolute/path/to/doom1.wad
```

CMake downloads the pinned doomgeneric sources. Supply your own Doom IWAD;
shareware `doom1.wad` can be extracted from the original
[Doom95 archive](https://www.gamers.org/pub/idgames/idstuff/doom/win95/).
Game data is not bundled or downloaded automatically. Configuration and saves
go in `rootdoom-data/`; use `--data-dir DIR` to change this.

Click the canvas to focus it. **Arrows** move/turn, **Ctrl** fires, **Space**
opens doors, **Shift** runs, **Enter** selects and **Escape** opens the menu.
Close the window or quit through Doom's menu to stop.

The default `rootgl` backend replaces Doom's software world-rendering pass;
the original menus, HUD and automap are retained. Add `--wireframe` to inspect
the geometry, or `--renderer classic` for the original software-rendered view.
Lighting, palette and invisibility effects in `rootgl` are approximations.
Sound is disabled.

## Run inside ROOT

After building, start `root -l` from the project directory, then enter:

```cpp
.x macros/doom.C("/absolute/path/to/doom1.wad")
```

The default geometry renderer runs in the same ROOT process and leaves the
prompt usable. Closing the game keeps ROOT open. Restart ROOT before another
game. To select the classic backend, pass `"classic"` as the third argument;
the optional second argument is the data directory. For a custom build,
set `ROOTDOOM_LIBRARY` to the absolute path of `librootdoom.so`.

## License

ROOTDoom is [GPL-2.0-or-later](LICENSE). doomgeneric retains its original
copyright and license notices; ROOT is an external dependency with its own
license. Doom game data is separately copyrighted and is not included.
