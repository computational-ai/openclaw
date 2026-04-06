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
- **Toggleable console pane** at the bottom of the window
  - History area showing every expression and its result
  - Input field for typing full expressions (e.g. `3 + 4 * (2 - 1)`)
  - Results sync back to the main display for continued button operations
- **`--console` mode**: pure command-line REPL, no GUI

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

### GUI mode (default)

```bat
build\bin\Release\calculator.exe
```

Or simply double-click the executable in Explorer.

Click the **Console ▼** button at the bottom of the window to reveal the
console pane. Type any expression in the input field and press **Enter**
to evaluate it. The result appears in the history area and is reflected in
the main display so that subsequent button presses continue from that value.

### Console mode (CLI REPL)

```bat
build\bin\Release\calculator.exe --console
```

```
OpenClaw Calculator – Console Mode
Enter an expression (e.g.  3 + 4 * 2 ) or 'exit' to quit.

> 3 + 4 * 2
  = 11
> (1 + 2) * (3 + 4)
  = 21
> exit
```

The console mode supports the same expression syntax as the GUI console pane:
numbers, `+` `-` `*` `/`, parentheses, and unary minus (e.g. `-5 * 2`).

---

## Keyboard shortcuts (GUI mode)

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

> **Note:** keyboard shortcuts are suspended while the cursor is in the
> console input field so that you can type full expressions freely.
