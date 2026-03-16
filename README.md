# Pie Todo – Raidcore Nexus Addon

Nexus addon to help track your daily and weekly gameplay in Guild Wars 2.

## AI Notice

This addon has been 100% created in [Windsurf](https://windsurf.com/) using Claude. I understand that some folks have a moral, financial or political objection to creating software using an LLM. I just wanted to make a useful tool for the GW2 community, and this was the only way I could do it.

If an LLM creating software upsets you, then perhaps this repo isn't for you. Move on, and enjoy your day.

## Features (same as desktop)

- **Tasks**: Add, edit, delete, with drag&drop sorting. 
- **Repeat**: Daily (midnight GMT) or Weekly (Monday 07:30 GMT)
- **Completed**: completed tasks can be coloured or optionally hidden until reset
- **Resets**: Daily / Weekly tasks automatically reset at the same time as Guild Wars 2
- **Persistence**: `todos.json` in the addon folder

## Build

### Linux (cross-compile to Windows .dll)

Install MinGW-w64, then configure with the toolchain and build:

```bash
# Arch
sudo pacman -S mingw-w64-gcc

# Ubuntu / Debian
sudo apt install mingw-w64

# Fedora
sudo dnf install mingw64-gcc-c++
```

```bash
cd addon_todo
rm -rf build
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchains/linux-mingw-w64.cmake
make
```

Output: `build/PieTodo.dll` and (if present) `build/libwinpthread-1.dll`. Copy both into Nexus’s addons folder.

### Windows

Native build with Visual Studio or MinGW:

```bash
cd addon_todo
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

Or with MinGW: use `-G "MinGW Makefiles" -DCMAKE_TOOLCHAIN_FILE=...` if you have a Windows MinGW toolchain file.

Copy `PieTodo.dll` (and if built with MinGW, `libwinpthread-1.dll`) into Nexus’s addons folder. Create a `PieTodo` subfolder there; `todos.json` will be created automatically.

## Usage

- **Open**: Default keybind `Ctrl+Shift+T` (or from Nexus addon list).
- **Add**: Type in “Task…”, choose Daily/Weekly, click Add (or Enter).
- **Edit/Delete**: Right-click a row → Edit or Delete.
- **Reorder**: Use Up/Dn on each row.
- **Options**: Completed tasks = Colour or Hide.

## Data

- Path: `{Nexus addon dir}/PieTodo/todos.json`
- Format matches the desktop app so you can share the same file (e.g. copy from `~/.pie-todo/todos.json` if you use the Python app too).

## License

This software is provided as-is, without a warranty of any kind. Use at your own risk. It might delete your files, melt your PC, burn your house down, or cause world peace. Probably not that last one, but one can hope.