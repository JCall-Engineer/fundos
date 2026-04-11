# FundOS

FundOS is a cross-platform personal finance and budgeting application. Licensed under the [AGPL](https://www.gnu.org/licenses/agpl-3.0.html).

## Building

FundOS uses CMake with presets for each platform and configuration. The configure step generates build system files; the build step compiles.

**When you add or remove source files, re-run the configure step** — the build uses `GLOB_RECURSE` to discover sources, which only re-scans during configure, not during incremental builds.

### Windows (Debug)

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
```

### Windows (Release)

```powershell
cmake --preset windows-release
cmake --build --preset windows-release
```

### Linux (Debug)

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
```

### Linux (Release)

```bash
cmake --preset linux-release
cmake --build --preset linux-release
```

## Running Tests

Tests are built automatically as part of the build step.

### Windows

```powershell
.\out\windows-debug\bin\Debug\tests.exe
.\out\windows-release\bin\Release\tests.exe
```

### Linux

```bash
./out/linux-debug/bin/tests
./out/linux-release/bin/tests
```

## Output Structure

```text
out/
  build/               <- CMake generator files, do not edit
  windows-debug/
    bin/Debug/         <- tests.exe
    lib/Debug/         <- static libraries
  windows-release/
    bin/Release/
    lib/Release/
  linux-debug/
    bin/
    lib/
  linux-release/
    bin/
    lib/
```
