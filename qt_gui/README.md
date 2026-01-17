# Qt GUI (BLP Viewer + Batch Converter)

This directory contains a Qt Widgets front-end for previewing and batch converting
images between BMP/JPG/PNG/TGA/BLP.

## Features
- Drag & drop files into the list
- Batch conversion with input/output directories
- BLP preview with alpha checkerboard
- Zoom (wheel) and fit-to-window
- Shows image dimensions and file size
- UTF-8 paths and Chinese filenames (Qt file IO + in-memory decode)

## Build

Requirements:
- CMake 3.16+
- Qt 6 (preferred) or Qt 5 (Widgets)
- C++17 compiler
- Rust + Cargo (to build the BLP library)

### Windows (PowerShell helper)

From repo root:
```powershell
.\qt_gui\build.ps1 -BuildBlp
```

If Qt is not found automatically, pass a prefix path:
```powershell
.\qt_gui\build.ps1 -QtPrefix C:\Qt\6.6.1\msvc2019_64 -BuildBlp -Config Release
```

The script checks for `cmake`, Qt, and `cargo` (when `-BuildBlp` is used), and will
copy the BLP library next to the executable when possible.

Build the BLP library first:
```bash
cargo build --release
```

Then build the GUI:
```bash
cmake -S qt_gui -B qt_gui/build
cmake --build qt_gui/build --config Release
```

## BLP Library Loading

The GUI loads the BLP library dynamically. It looks for the following in the
application directory and common build locations:
- Windows: `blp_lib.dll` (or `blp-windows.dll`)
- macOS: `libblp_lib.dylib` (or `libblp-macos.dylib`)
- Linux: `libblp_lib.so` (or `libblp-linux.so`)

You can also set `BLP_LIB_PATH` to an explicit full path.
