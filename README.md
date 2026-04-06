# OpenClaw Calculator

A simple calculator application for **Windows x64**, written in C++ using the
native Win32 API.

---

## Features

- Standard arithmetic: **+**, **−**, **×**, **÷**
- Decimal point input
- Sign toggle (**+/−**)
- Percentage (**%**)
- Clear (**C**), Clear Entry (**CE**), and Backspace (**⌫**)
- Full **keyboard support** (numpad included)
- Clean Win32 GUI — no external runtime dependencies

---

## Requirements

| Tool | Minimum version |
|------|----------------|
| Windows | 10 (x64) |
| CMake | 3.20 |
| C++ compiler | MSVC 2019 / Clang-cl / MinGW-w64 (targeting x64) |

---

## Building

### Using CMake + MSVC (Visual Studio)

```bat
cmake -B build -A x64
cmake --build build --config Release
```

The executable is written to `build\bin\Release\calculator.exe`.

### Using CMake + MinGW-w64 (MSYS2)

```bash
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The executable is written to `build/bin/calculator.exe`.

---

## Running

```bat
build\bin\Release\calculator.exe
```

Or simply double-click the executable in Explorer.

---

## Keyboard shortcuts

| Key(s) | Action |
|--------|--------|
| `0`–`9`, numpad `0`–`9` | Digit input |
| `.` / numpad `.` | Decimal point |
| `+` / numpad `+` | Addition |
| `-` / numpad `-` | Subtraction |
| `*` / numpad `*` | Multiplication |
| `/` / numpad `/` | Division |
| `Enter` / `=` | Evaluate |
| `Backspace` | Delete last digit |
| `Escape` | Clear all (C) |
| `%` | Percentage |
