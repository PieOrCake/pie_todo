# Pie Todo – Raidcore Nexus Addon

Nexus addon with **feature parity** to the desktop Pie Todo app (`pie_todo.py`). Uses **ImGui 1.80** to match the Nexus loader.

## Features (same as desktop)

- **Tasks**: Add, edit, delete, reorder (Up/Down)
- **Repeat**: Daily (midnight GMT) or Weekly (Monday 07:30 GMT)
- **Completed**: Checkbox per task; green row when “Colour” mode
- **Options**: Show completed as “Colour” or “Hide”
- **Resets**: Daily Reset / Weekly Reset buttons; auto-reset every 60s check
- **Persistence**: `todos.json` in the addon folder (same JSON shape as desktop)

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
